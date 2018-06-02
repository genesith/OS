#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "threads/pte.h"
#include "vm/swap.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  // printf("file_name : %s\n", file_name);
  char *fn_copy;
  char * temp;
  char * next;

  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);
  // printf("len : %d\n", strlen(fn_copy));
  // printf("file_name : %s\n", file_name);
  temp = strtok_r(file_name, " ", &next);
  // printf("temp : %s\n", temp);
  /* Create a new thread to execute FILE_NAME. */
  // printf("why twice??\n");
  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  // free(temp);
  if (tid == TID_ERROR){

    // printf("fail??\n");
    palloc_free_page (fn_copy); 
    
    }

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  // printf("%s\n", (char *)file_name_);
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  // printf("HERERERE!!!\n");
  success = load (file_name, &if_.eip, &if_.esp);
  if (strcmp(thread_current()->parent_thread->name, "main")){
    sema_up(&(thread_current()->parent_thread)->exec_sema);
    intr_disable();
    thread_block();
  }

  /* If load failed, quit. */
  // printf("here1!!!\n");

  palloc_free_page (file_name);
  // printf("here2!!!\n");

  // printf("hehehe7\n");
  if (!success){
    // printf("????\n"); 
    thread_exit ();
}
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  // hex_dump(if_.esp, if_.esp, 500, true);
  // hex_dump(if_.eip, if_.eip, 500, true);
  // printf("here!!!\n");
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  // hex_dump(if_.esp, if_.esp, 500, true);

  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  // printf("wait for %d\n", child_tid);
  struct thread * child_thread = search_by_tid(child_tid);
  sema_down(&child_thread->child_sema);
  // printf("done\n");
}

/* Free the current process's resources. */

void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
      delete_from_swap();

    }
}
/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

#define MAX_ARGV 100

static bool setup_stack (void **esp, char * file_name);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{

  // printf("LOADING!!!!!!\n");

  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  char * temp;
  char * next;
  int i;
  char dup_filename[MAX_ARGV];

  // printf("xe");
  strlcpy(dup_filename, file_name, MAX_ARGV);

  // printf("ye");
  
  // printf("dup:%s\n", dup_filename);
  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  temp = strtok_r(file_name, " ", &next);
  // ("Temp : %s\n", temp);
  // printf("Parent : %s, memcmp : %s\n", t->parent_thread->name, t->name);
  // printf("thread_current : %s, tid : %d\n", thread_current()->name, thread_current()->tid);
  // free(temp);
  file = filesys_open(file_name);
  // / / printf("hahah\n");
  if (file == NULL) 
    {
      // printf("1\n");
      printf ("load: %s: open failed\n", file_name);
      thread_current()->parent_thread->exec_normal = -1;
      // exit(-1);
      goto done; 
    }

  // file_deny_write(file);

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

    // printf("hehehe\n");
  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              // printf("%s %d %d\n", file_name, read_bytes, zero_bytes);

              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable)){
                // printf("load_segment\n");
                goto done;
              }
            }
          else
            goto done;
          break;
        }
    }
  // printf("hehehe2\n");
  /* Set up stack. */
  // printf("before setup stack\n");
  if (!setup_stack (esp, dup_filename)){
    // printf("setup stack\n");
    goto done;
}
  // printf("hehehe3\n");


  /* Start address. */
  // printf("All done\n");
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;
  file->inode->open_cnt++;
  file_deny_write(file);
  file->inode->deny_write_cnt += 1;
  t->exec_inode = file->inode;

 done:
  // free(dup_filename);
  /* We arrive here whether the load is successful or not. */
  // printf("hehehe5\n");


  // printf("%d\n", t->exec_inode->deny_write_cnt);
  file_close (file);
  // printf("done\n");
  // printf("%d\n", t->exec_inode->deny_write_cnt);
  // printf("hehehe5\n");
  // t->code_write = 1;
  return success;
}

/* load() helpers. */

bool install_page (int tid, void *upage, void *kpage, bool writable, bool fifo, struct invalid_struct * target_struct);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  // printf("load_segment\n");

  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);
  int i = 0;

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {

      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;






      // printf("load %x\n", upage);
      struct invalid_struct * target_struct = (struct invalid_struct *)malloc(sizeof(struct invalid_struct));
      // printf("load upage : %x %d\n", upage, page_read_bytes);
      target_struct->vpage = upage;
      thread_current()->last_load = upage;
      target_struct->sector = NULL;
      target_struct->lazy = 1;
      target_struct->lazy_inode = file->inode;
      target_struct->lazy_offset = ofs;
      target_struct->lazy_writable = writable;
      target_struct->is_mmap = 0;
      // printf("process offset : %d %d\n", ofs, page_read_bytes);

      target_struct->lazy_write_byte = page_read_bytes;

      list_push_back(&thread_current()->invalid_list, &target_struct->invalid_elem);





      /* Get a page of memory. */
      // uint8_t *kpage = palloc_get_page (PAL_USER);
      // uint8_t *kpage = frame_allocate();
      // if (kpage == NULL){
      //   printf("1\n");
      //   return false;
      // }
      // /* Load this page. */
      // if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
      //   {
      //     palloc_free_page (kpage);
      //     // printf("2\n");
      //     return false; 
      //   }
      // memset (kpage + page_read_bytes, 0, page_zero_bytes);

      // /* Add the page to the process's address space. */
      // if (!install_page (thread_current()->tid, upage, kpage, writable, true)) 
      //   {
      //     palloc_free_page (kpage);
      //     // printf("3\n");
      //     return false; 
      //   }

      // hex_dump(kpage, kpage, PGSIZE, true);

      // frame_insert(kpage, upage);



      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      ofs += page_read_bytes;
    }
  // printf("load segment done\n");
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, char * file_name) 
{ 
  char dup_argv[MAX_ARGV];
  strlcpy(dup_argv, file_name, MAX_ARGV);
  char * temp;
  char * next;
  int index = 0;
  int argc = 0;
  int total_argv_len = 0;
  int stack_off;
  int word_align;
  
  // printf("file_name for setup_stack : %s, dup : %s\n", file_name, dup_argv);
  temp = strtok_r(dup_argv, " ", &next);

  while(temp){
     
    // printf("argv : %s, strlen : %d\n", temp, strlen(temp));
    total_argv_len += strlen(temp) + 1;
    temp = strtok_r(NULL, " ", &next);
    argc += 1;
  }
  
  word_align = (4 - (total_argv_len & 3))%4;
  // printf("word_align : %d, argc : %d\n", word_align, argc);

  stack_off = total_argv_len + word_align + (argc+4) * 4;
  // printf("stack_offset : %d\n", stack_off);

  uint8_t *kpage;
  bool success = false;

  // kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  kpage = frame_allocate ();
  if (kpage != NULL) 
    {
      success = install_page (thread_current()->tid, ((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true, true, NULL);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }

  *esp -= stack_off;
  thread_current()->last_esp = *esp;

  // printf("%c, %s, %x", (char **)esp, (char *)*esp, (char)**esp);
  // printf("stack_offset : %s\n", stack_offset);
  
  // memset(*esp, 0, 4);
  *(*(int **)esp+1) = argc;
  *(*(int **)esp+2) = *esp + 12;
  *(*(int **)esp+argc+3) = 0;
  memset(*esp+(argc+4)*4, 0, word_align);
  temp = strtok_r(file_name, " ", &next);
  
  char * while_pointer;
  while_pointer = *esp + (argc+4)*4 + word_align; 
    while(temp){

      strlcpy(while_pointer, temp, strlen(temp)+1);
      *(*(int **)esp + 3 + index) = while_pointer;
      index++;
      while_pointer += (strlen(temp) + 1);
      temp = strtok_r(NULL, " ", &next);
  }

  // free(temp);
  // char buf[1024];
  // hex_dump(0, *esp, stack_off, true);
  // *esp -= 8;
  // printf("esp : %x\n", *esp);

  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool
install_page (int tid, void *upage, void *kpage, bool writable, bool fifo, struct invalid_struct * target_struct)
{
  // printf("install page : %x %x\n", upage, kpage);

  struct thread *t = search_by_tid (tid);

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  // hex_dump(upage, upage, 30, true);
  bool result = pagedir_get_page (t->pagedir, upage) == NULL && pagedir_set_page (t->pagedir, upage, kpage, writable);
  // hex_dump(upage, upage, 4096, true);
  // printf("result : %d\n", result);
  if(result){

    struct reference_struct * reference = (struct reference_struct * )malloc(sizeof(struct reference_struct));
    reference->tid = t->tid;
    reference->vpage = (uint8_t *) ((uint32_t)upage & BITMASK(12, 32));


    
    if (fifo){

      struct frame_struct * target_frame = (struct frame_struct * ) malloc(sizeof(struct frame_struct));
      list_init(&target_frame->references);
      target_frame->kpage = (uint8_t *) ((uint32_t)kpage & BITMASK(12, 32));

      if(target_struct){
        if(target_struct->is_mmap == 1){
          // printf("upage : %x\n", upage);
          target_frame->mmap_inode = target_struct->lazy_inode;
          target_frame->mmap_offset = target_struct->lazy_offset;
          target_frame->is_mmap = 1;
          target_frame->mmap_write_byte = target_struct->lazy_write_byte;
          target_frame->mmapid = target_struct->mmapid;
          target_frame->mmap_tid = t->tid;
          target_frame->dirty_bit = 0;
        }

        else
          target_frame->is_mmap = 0;
      }
    // printf("1\n");
    list_push_back(&target_frame->references, &reference->reference_elem);
    list_push_back(&FIFO_list, &target_frame->FIFO_elem);
    }

    else{
      struct list_elem * target_elem = list_end(&FIFO_list);
      struct frame_struct * target_frame = list_entry(target_elem, struct frame_struct, FIFO_elem);
      list_push_back(&target_frame->references, &reference->reference_elem);

    }
    
  }

  // printf("3\n");

  // printf("install done\n");
  return result;
}
