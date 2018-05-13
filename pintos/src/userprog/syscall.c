#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
// #include "userprog/syscall.h"
#include "threads/vaddr.h"
#include <user/syscall.h>
#include "userprog/process.h"
#include "userprog/exception.h"
#include "devices/shutdown.h"
#include "threads/malloc.h"
#include <list.h>

// #define PHYS_BASE 0xC0000000;

static void syscall_handler (struct intr_frame *);

struct fd_struct * find_by_fd(int fd);

int allocate_fd(void);

void exit(int status);
bool check_invalid_pointer(void * addr);
bool check_invalid_pointer2(void * addr, void * f_esp);
bool is_code_segment(void * addr);
bool is_there_or_should_be(void * addr);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // printf("HERE!!!\n");

  // printf("esp : %x\n", f->esp);

  if (!(is_there_or_should_be((void *) f->esp))){
    f->eax = -1;
    exit(-1);
  }
  // if (check_invalid_pointer(f->esp)){
  //   f->eax = -1;
  //   exit(-1);

  // }

  if (f->esp >= PHYS_BASE - 12){
    f->eax = -1;
    exit(-1);
  }



  int syscall_num = *(int *)(f->esp);

  // printf("syscall_num : %d\n", syscall_num);

  int param1 = *((int *)(f->esp)+ 1);
  int param2 = *((int *)(f->esp)+ 2);
  int param3 = *((int *)(f->esp)+ 3);


  


  switch(syscall_num){
  	case SYS_HALT:

  	{
      // printf("halt\n");
  		shutdown_power_off();

  		break;
  	}
  	
  	case SYS_EXIT:
  	{ 

      // printf(thread_current()->name);

      int status = param1;
      f->eax = status;
  		exit(status);

  		break;
  	}
  	
  	case SYS_EXEC:
  	{

      char * temp, *next;
      char command[50];
      // printf("param1 : %s\n", (ch`[ar *)param1);
      // printf("will now print param1 : %s, and esp\n", param1);

      if ((check_invalid_pointer((void *) param1)) || (!(is_there_or_should_be((void *) param1)))){
        f->eax = -1;
        exit(-1);
        break;
      }

      strlcpy(command, (char *)param1, strlen((char *)param1)+1);
  		sema_init(&thread_current()->exec_sema, 0);
      int pid = process_execute(command);
      // free(command);
      // printf("1\n");
      sema_down(&thread_current()->exec_sema);
      // printf("2\n");
  		if (!(pid)){
        f->eax = -1;
      }

      else if (thread_current()->exec_normal == -1){
        f->eax = -1;
        thread_current()->exec_normal = 0;
      }

      else{
        // thread_current()->exec_inode->deny_write_cnt++;
        f->eax = pid;      
      
      }

      break;
  	}
  	
  	case SYS_WAIT:
  	{
      // printf("thread_name : %s, tid : %d, syscall : %d\n", thread_current()->name, thread_current()->tid, syscall_num);

      // printf("hhhahahah\n");
      thread_current()->exit_normal = 0; 
      int pid = param1;
      struct thread * target_thread = search_by_tid(pid);
      // printf("status : %d, name : %s\n", target_thread->tid, target_thread->name);
      


      if (!(target_thread)){
        // printf("hhhahahah\n");  
        f->eax = -1;
        break;

      }

      thread_unblock(target_thread);
      list_remove(&target_thread->child_elem);
      // printf("hhhahahah\n");
      process_wait(pid);
      // printf("hhhahahah\n");
      // printf("HERE!!, exit:/ %d\n", thread_current()->exit_normal);
      if (thread_current()->exit_normal == 1){
        // printf("here!");
        f->eax = thread_current()->child_return;
      }

      else
        f->eax = -1;
      
      break;

    }
  	
  	case SYS_CREATE:
    {
      if ((check_invalid_pointer((void *) param1))|| (!(is_there_or_should_be((void *) param1)))){
        f->eax = -1;
        exit(-1);
        break;
      }


      bool result = filesys_create((char *)param1, (off_t) param2);
      f->eax = result;
      break;
    
    }
  	
  	case SYS_REMOVE:
  	{
      if (check_invalid_pointer((void *) param1)){
        f->eax = -1;
        exit(-1);
        break;
      }

      bool result = filesys_remove((char *)param1);
      f->eax = result;
      break;
      
    }
  	
  	case SYS_OPEN:
  	{
      
      if ((check_invalid_pointer((void *) param1)) || (!(is_there_or_should_be((void *) param1)))){
        f->eax = -1;
        // printf("open failed due to invalid pointer : %x\n", param1);
        exit(-1);
        break;
      }


      struct file * target_file;
      if ((target_file = filesys_open((char *)param1))){
        struct fd_struct * file_fd = malloc(sizeof(struct fd_struct));

        file_fd->fd = allocate_fd();

        file_fd->the_file = target_file;
        if (target_file->inode->deny_write_cnt > 0){
          file_deny_write(target_file);
          // target_file->inode->deny_write_cnt++;
        }
        sema_init(&file_fd->file_sema, 1);
        list_push_back(&thread_current()->fd_list, &file_fd->fd_elem);

        f->eax = file_fd->fd; 
      }

      else
        f->eax = -1;

      break;
    }
  	
  	case SYS_FILESIZE:
  	
    {

      struct fd_struct * target_struct = find_by_fd(param1);
      
      if (!(target_struct)){
        f->eax = -1;
        break;
      }

      f->eax = file_length(target_struct->the_file);

      break;    
    }
  	
    case SYS_READ:
  	{
      // printf("sysread");
      if (param1 == 0){
        input_getc();
        // f->eax = 0;
        // exit(0);
      }

      else{
        
        struct fd_struct * target_struct = find_by_fd(param1);
        
        if (!(target_struct)){
          f->eax = -1;
          break;
        }


        // printf("Check address %x %x %x\n", param2, f->esp, f->esp - param2);
        void * new_esp = (void *)((uintptr_t)f->esp & (uintptr_t)0xfffff000);
 

        // if ((check_invalid_pointer((void *) param2)) || (!(is_there_or_should_be((void *) param2)))){

        if (check_invalid_pointer((void *) param2)) {  
          // printf("GG\n");
          f->eax = -1;
          exit(-1);
          break;
        }

        sema_down(&target_struct->file_sema);  
        int size = file_read(target_struct->the_file, (void *)param2, (unsigned) param3);
        f->eax = size;
        file_deny_write(target_struct->the_file);
        sema_up(&target_struct->file_sema);

      }
      break;
    }
  	
  	case SYS_WRITE:
  	{
      // printf("SysWrite has been called\n");
      // printf("Check address %x %x\n", param2, f->esp);

      // if (is_code_segment((void * ) param2)){
      //   f->eax = -1;
      //   exit(-1);
      //   break;
      // }

      if ((check_invalid_pointer((void *) param2)) || (!(is_there_or_should_be((void *) param2)))){
        f->eax = -1;
        exit(-1);
        break;
      }

      if (param1 == 1){
        putbuf((char *)param2, param3);
        f->eax = param3;
      }

      else{
        struct fd_struct * target_struct = find_by_fd(param1);
        if (target_struct->the_file->inode->deny_write_cnt > 0)
          target_struct->the_file->deny_write = 1;
        else
          target_struct->the_file->deny_write = 0;


        if (!(target_struct)){
          f->eax = -1;
          break;
        }


        // printf("Deny? : %d\n", target_struct->the_file->deny_write);
        if (target_struct->the_file->deny_write){
          f->eax = 0;
          break;
        }

        // if(strcmp(thread_current()->parent_thread->name, "main")){
        //   f->eax = 0;
        //   break;
        // }
        
        sema_down(&target_struct->file_sema);  
        int size = file_write(target_struct->the_file, (void *)param2, (unsigned) param3);
        // printf("size : %d\n", size);
        f->eax = size;

        sema_up(&target_struct->file_sema);

        // thread_current()->code_write = 1;

      }

  		break;
  	}
  	
  	case SYS_SEEK:
    {
      struct fd_struct * target_struct = find_by_fd(param1);
      if (!(target_struct)){
        f->eax = -1;
        break;
        }
      file_seek(target_struct->the_file, param2);

      if (param2 == 0){
        file_allow_write(target_struct->the_file);
      }
      

  		break;
    }
  	
  	case SYS_TELL:
    {         

      struct fd_struct * target_struct = find_by_fd(param1);
      if (!(target_struct)){
        f->eax = -1;
        break;
        }
        
      f->eax = file_tell(target_struct->the_file);
  		break;
  	}
  	
    case SYS_CLOSE:
  	{
      struct fd_struct * target_file = find_by_fd(param1);
      if (target_file){
        list_remove(&target_file->fd_elem);
        file_close(target_file->the_file);
        free(target_file);
      }
      
      break;
    }

    default:
    {
      f->eax = -1;
      break;
    }
  
  }
  
}


struct fd_struct * find_by_fd(int fd){

  struct list * current_list = &thread_current()->fd_list;
  struct list_elem * temp;

  for (temp = list_begin(current_list); temp != list_end(current_list); temp = list_next(temp)){
    struct fd_struct * temp_struct = list_entry(temp, struct fd_struct, fd_elem);
    
    if (temp_struct->fd == fd){
      return temp_struct;
    }

  }
  // printf("HERE???\n");
  return NULL; 
}


int allocate_fd(void){
  
  sema_down(&thread_current()->fd_list_lock);
  int fd = ++(thread_current()->last_fd);
  sema_up(&thread_current()->fd_list_lock);
  
  return fd;
}



void exit(int status){
  int i = 0;
  struct fd_struct * temp_struct;
  thread_current()->parent_thread->child_return = status;
  thread_current()->parent_thread->exit_normal = 1;
  
  if (thread_current()->exec_inode->deny_write_cnt > 0)
    thread_current()->exec_inode->deny_write_cnt--;
  
  printf("%s: exit(%d)\n", thread_current()->name, status);
  sema_up(&thread_current()->child_sema);

  struct list_elem * temp;
  for(temp = list_begin(&thread_current()->child_list); temp != list_end(&thread_current()->child_list); temp = list_next(temp)){
    struct thread * temp_thread = list_entry(temp, struct thread, child_elem);
    thread_unblock(temp_thread);
    // printf("kk\n");
  }

  for(temp = list_begin(&thread_current()->fd_list); temp != list_end(&thread_current()->fd_list); temp = list_next(temp)){
    temp_struct = list_entry(temp, struct fd_struct, fd_elem);
    
    if(temp_struct->the_file){
      file_close(temp_struct->the_file);
      temp_struct->the_file = NULL;
    }

    if (i == 1){
      struct fd_struct * prev_struct = list_entry(list_prev(temp), struct fd_struct, fd_elem);
      free(prev_struct);
    }
    i = 1;
    // printf("hh\n");
  }
  // free(temp_struct);

  thread_exit ();
}

bool check_invalid_pointer(void * addr){

  if (!(addr))
    return 1;

  if (addr > PHYS_BASE)
    return 1;

  if (addr < USER_VADDR_BOTTOM)
    return 1;


  // printf("check 1. %x\n", addr);
  // fault_addr > USER_VADDR_BOTTOM && is_user_vaddr(fault_addr)
  
  // if (addr < USER_VADDR_BOTTOM)
  //   return 1;
  // if (!(is_user_vaddr(addr)))
  //   return 1;

  // if (addr > PHYS_BASE-12)
  //   return 1;

  // void * ptr = pagedir_get_page(thread_current()->pagedir, addr);
  // if(!ptr){
  //   printf("here!!\n");
  //   return 1;
  // }
  // if (addr < 0x8048000)
  //    return 1;

  // if (!(addr))
  //    return 1;


  return 0;
}

bool check_invalid_pointer2(void * addr, void * f_esp){

  // printf("check 2, %x %x\n", addr, f_esp);
  // fault_addr > USER_VADDR_BOTTOM && is_user_vaddr(fault_addr)
  
  // if (addr < USER_VADDR_BOTTOM)
  //   return 1;
  // if (!(is_user_vaddr(addr)))
  //   return 1;

  // if (addr > PHYS_BASE)
  //   return 1;

  // if (addr >= f_esp)
  //   return 0;

  // if ((addr <= f_esp-0x1000) && (addr >= USER_VADDR_BOTTOM + 0x10000000))
  //   return 1;

  // void * ptr = pagedir_get_page(thread_current()->pagedir, addr);
  // if(!ptr){
  //   // printf("here!!\n");
  //   return 1;
  // }
  // if (addr < 0x8048000)
  //    return 1;

  // if (!(addr))
  //    return 1;


  return 0;
}

bool is_code_segment(void * addr){
  if ((USER_VADDR_BOTTOM <= addr) && (addr <= USER_VADDR_BOTTOM + 0x1000))
    return 1;
}

bool is_there_or_should_be(void * addr){
    void * page_ptr = pagedir_get_page(thread_current()->pagedir, addr);

    void * new_addr = (void *)((uintptr_t)addr & (uintptr_t)0xfffff000);

    void * invalid_ptr = invalid_list_check(new_addr, 0);

    return (page_ptr || invalid_ptr);

}