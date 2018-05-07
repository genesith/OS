#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "vm/invalidlist.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/vaddr.h"
#include <list.h>
#include <stdint.h>

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */
  // printf("%s\n", thread_current()->name);
  int i = 0;
  struct list_elem * target_elem;
  struct list * references;
  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */

  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  bool result = false;
  printf("%d %x %d %x %s\n", not_present, fault_addr, is_user_vaddr(fault_addr), f->esp, thread_current()->name);
  if (not_present && fault_addr > USER_VADDR_BOTTOM && is_user_vaddr(fault_addr) && (f->esp < 0xc0000000) ) {
      // printf("Page Fault!!! with %x, %x\n", fault_addr, f->esp-0x1000);

    if ((fault_addr <= f->esp - 0x1000) && (fault_addr >= USER_VADDR_BOTTOM + 0x1000)){
      printf("Address is wrong, %x %x\n", fault_addr, f->esp);
      exit(-1);
    }
  // printf("%d %d %d\n", not_present, write, user);


  uint8_t * new_addr = frame_allocate();

  // Swaped out page --> Swap in
  // Do we have to Convert fault_address to index ????


  fault_addr = (void *)((uintptr_t)fault_addr & (uintptr_t)0xfffff000);
  int sector_num = invalid_list_check((uint8_t *) fault_addr);
  printf("sector_num : %d\n", sector_num);
  
  

    if (sector_num >= 0){
      
      references = swap_in(new_addr, sector_num);
      invalid_list_remove(references);

      for (target_elem = list_begin(references); target_elem != list_end(references); target_elem = list_next(target_elem)){
        struct reference_struct * target_struct = list_entry(target_elem, struct reference_struct, reference_elem);
        
        if (i == 0){
          install_page(target_struct->vpage, (void *) new_addr, true, true);
          i++;
        }
        else{
          install_page(target_struct->vpage, (void *) new_addr, true, false);
        }
      }

    }

    else{
      
      result = install_page(fault_addr, (void *) new_addr, true, true);
      // printf("here?? Stack growth result was %d\n", result);  
    }
  }

  else{
    printf("why is it??\n");
    exit(-1);
  
  }
  // if (!(result)){
  //   exit(-1);
  // }

}



  /*
  1. Check faulted address is in invalid_list
    - If exist, swap in to the new physical memory, update Invalid_list(Delete it)
    - If not, continue

  2. Page Install - Insert to page table, FIFO List 

  


  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */

  // exit(-1);
  // printf ("Page fault at %p: %s error %s page in %s context.\n",
  //         fault_addr,
  //         not_present ? "not present" : "rights violation",
  //         write ? "writing" : "reading",
  //         user ? "user" : "kernel");
  // kill (f);
// }

