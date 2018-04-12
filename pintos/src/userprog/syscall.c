#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
// #include "userprog/syscall.h"
#include <user/syscall.h>
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "threads/malloc.h"
#include <list.h>

static void syscall_handler (struct intr_frame *);

struct fd_struct * find_by_fd(int fd);

int allocate_fd(void);

void exit(int status);
// void remove_fd(int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{

  int syscall_num = *(int *)(f->esp);
  int param1 = *((int *)(f->esp)+ 1);
  int param2 = *((int *)(f->esp)+ 2);
  int param3 = *((int *)(f->esp)+ 3);
  
  switch(syscall_num){
  	case SYS_HALT:

  	{
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

      char command[50];
      // printf("param1 : %s\n", (char *)param1);
      strlcpy(command, (char *)param1, strlen((char *)param1)+1);
  		int pid = process_execute(command);
      // printf("param1:%s\n", param1);
  		if (!(pid)){
        f->eax = -1;
      }
      else
        f->eax = pid;

      break;
  	}
  	
  	case SYS_WAIT:
  	{
      thread_current()->exit_normal == 0; 
      int pid = param1;
      struct thread * target_thread = search_by_tid(pid);
      // printf("status : %d, name : %s\n", target_thread->tid, target_thread->name);
      if (!(target_thread)){
        f->eax = -1;
        break;

      }

      process_wait(pid);
      
      // sema_down(&target_thread->parent_sema);
      // printf("HERE!!");
      if (thread_current()->exit_normal == 1){
        f->eax = thread_current()->child_return;

      }
      else
        f->eax = -1;
      break;

    }
  	
  	case SYS_CREATE:
    {
      

      bool result = filesys_create((char *)param1, (off_t) param2);
      f->eax = result;
      break;
    
    }
  	
  	case SYS_REMOVE:
  	{

      bool result = filesys_remove((char *)param1);
      f->eax = result;
      break;
      
    }
  	
  	case SYS_OPEN:
  	{
      
      struct file * target_file;
      if ((target_file = filesys_open((char *)param1))){
        struct fd_struct * file_fd = malloc(sizeof(struct fd_struct));

        file_fd->fd = allocate_fd();
        // if (strstr(param1, "rox"))
        //   target_file->deny_write = true;
        file_fd->the_file = target_file;

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
      if (param1 == 1){
        input_getc();
      }

      else{
        
        struct fd_struct * target_struct = find_by_fd(param1);
        
        if (!(target_struct)){
          f->eax = -1;
          break;
        }
        
        int size = file_read(target_struct->the_file, (void *)param2, (unsigned) param3);
        f->eax = size;

      }
      break;
    }
  	
  	case SYS_WRITE:
  	{
      if (param1 == 1){
        putbuf((char *)param2, param3);
        f->eax = param3;
      }

      else{
        struct fd_struct * target_struct = find_by_fd(param1);

        if (!(target_struct)){
          f->eax = -1;
          break;
        }


        // printf("Deny? : %d\n", target_struct->the_file->deny_write);
        if (target_struct->the_file->deny_write){
          f->eax = 0;
          break;
        }
        int size = file_write(target_struct->the_file, (void *)param2, (unsigned) param3);
        // printf("size : %d\n", size);
        f->eax = size;

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
      
      // target_file->the_file->offset = param2;

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
      }
      
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

  
  thread_current()->parent_thread->child_return = status;
  thread_current()->parent_thread->exit_normal = 1;
  printf("%s: exit(%d)\n", thread_current()->name, status);
  sema_up(&thread_current()->child_sema);
  thread_exit ();
}
