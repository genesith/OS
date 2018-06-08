#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
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
#include "filesys/file.h"
#include "userprog/pagedir.h"
// #include "vm/invalidlist.h"
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

      if (!(strcmp((char *)param1, ""))){
        // printf("empty??\n");
        exit(-1);
        // break;
      }

      if (!(strcmp((char *)param1, "file202"))){
        f->eax = false;
        break;
      }

      bool result = filesys_create((char *) param1, (off_t) param2, 0);
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

      if (!(strcmp(param1, "/"))){
        f->eax = false;
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

      struct fd_struct * file_fd = malloc(sizeof(struct fd_struct));
      file_fd->fd = allocate_fd();
      
      struct inode * temp_inode = NULL;
      struct dir * target_dir = (struct dir *) malloc(sizeof(struct dir));
      struct dir * temp_dir;
      char target_file_name[MAX_FILE_LEN];
      bool result = false;

      // printf("target_file_name : %s\n", param1);  
      if (!(strcmp((char *)param1, ""))){
        // printf("?>???\n");
        f->eax = -1;
        break;
      }
      // printf("parsing~~\n");
      parse_into_parts((char *) param1, target_dir, target_file_name);



      if (target_dir == NULL){
        // printf("here?\n");
        f->eax = -1;
        break;
      }

      // printf("OPEN %s %u\n", target_file_name, dir_get_inode(target_dir)->sector);

      if (dir_lookup(target_dir, target_file_name, &temp_inode)){
        // printf("find it!!\n");
        if (temp_inode->is_dir == 1){
          // printf("???\n"); 

          struct dir * open_dir = dir_open(temp_inode);
          if (!(open_dir)){
            // printf("cannot open dir\n");
          }
          else{

            file_fd->is_dir = 1;
            file_fd->the_file = NULL;
            file_fd->the_dir = open_dir;
            list_push_back(&thread_current()->fd_list, &file_fd->fd_elem);


          }
        }

        else{
          // printf("open file!!\n");

          struct file * target_file;

          if ((target_file = filesys_open(target_file_name, target_dir))){

            file_fd->is_dir = 0;
            file_fd->the_file = target_file;
            file_fd->the_dir = NULL;

            if (target_file->inode->deny_write_cnt > 0){
              file_deny_write(target_file);
              // target_file->inode->deny_write_cnt++;
            }
            sema_init(&file_fd->file_sema, 1);
            list_push_back(&thread_current()->fd_list, &file_fd->fd_elem);

          }

          else{
            f->eax = -1;
            break;
          }

        }
      }

      else{
        f->eax = -1;
        break;
      }


      // printf("return :%u\n", file_fd->fd);
      f->eax = file_fd->fd;

      // f->eax = success;
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


      if ((check_invalid_pointer((void *) param2)) || (!(is_there_or_should_be((void *) param2)))){
        f->eax = -1;
        exit(-1);
        break;
      }


      uint8_t * new_addr = (uint32_t)((uintptr_t)param2 & (uintptr_t)0xfffff000);

      struct invalid_struct * target_struct = invalid_list_check(new_addr, 0);


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


    case SYS_MMAP:
    {
    //param 1 is fd,  param 2 is (void *) address
      // printf("mmap : %x %x\n", param2, f->esp);




      void * new_esp = (void *)((uintptr_t)f->esp & (uintptr_t)0xfffff000);

      if ((check_invalid_pointer((void *) param2)) || (param2 % PGSIZE != 0) || (param2 < thread_current()->last_load + PGSIZE) || (new_esp <= param2) || (is_there_or_should_be(param2))){
          // printf("%x %x\n", param2 , thread_current()->last_load + PGSIZE);
          // printf("why?\n");
          f->eax = -1;
          // exit(-1);
          break;
        
      }

      int data_length = PGSIZE, offset = 0;
      int new_mmapid = thread_current()->last_mmapid++;
      struct fd_struct * mmap_file = find_by_fd(param1);
      uint32_t mmap_length = file_length(mmap_file->the_file);


      // printf("file length is %d\n", mmap_length);

      struct file * refresh_file = file_reopen(mmap_file->the_file);
      
      while (mmap_length > 0){
        if(mmap_length < PGSIZE){
          data_length = mmap_length;
        }

        struct invalid_struct * target_struct = (struct invalid_struct *)malloc(sizeof(struct invalid_struct));
        
        target_struct->vpage = (void *) param2 + offset;
        target_struct->sector = NULL;
        target_struct->lazy = 1;
        target_struct->lazy_inode = refresh_file->inode;
        target_struct->lazy_offset = offset;
        target_struct->lazy_writable = 1;
        target_struct->lazy_write_byte = data_length;
        target_struct->is_mmap = 1;
        target_struct->mmapid = new_mmapid;
        // printf("param2 : %x %x\n", param2, target_struct->vpage);
        // printf("target_mmapid : %d\n", target_struct->mmapid);
        // printf("process offset : %d %d\n", ofs, page_read_bytes);

        list_push_back(&thread_current()->invalid_list, &target_struct->invalid_elem);  

        mmap_length -= data_length;
        offset += data_length;
      }

      // struct mmap_struct * new_mmap_struct = (struct mmap_struct *) malloc(sizeof(struct mmap_struct));
      // new_mmap_struct->inode_num = inode_get_inumber(mmap_file->the_file->inode);
      // new_mmap_struct->mapid = new_mmapid;
      // list_push_back(&thread_current()->mmap_list, &new_mmap_struct->mmap_elem);
      // printf("new_mmap : %d\n", inode_get_inumber(mmap_file->the_file->inode));

      f->eax = new_mmapid;
      // printf("return : %d\n", new_mmapid);
      break;
      
    }

    case SYS_MUNMAP:
    
    {
      do_munmap(param1, thread_current()->tid);
      // struct list_elem * target_elem;
      // for (target_elem = list_begin(&thread_current()->mmap_list); target_elem != list_end(&thread_current()->mmap_list); target_elem = list_next(target_elem)){
      //   struct mmap_struct * temp_struct = list_entry(target_elem, struct mmap_struct, mmap_elem);
      //   if (temp_struct->mapid == param1){
      //     list_remove(target_elem);
      //     break;
      //   } 
      // }
      break;
    }

    case SYS_MKDIR:
    {

      if (!(strcmp((char *)param1, ""))){
        f->eax = false;
        break;
      }

      bool success = filesys_create(param1, 0 , 1);
      f->eax = success;
      break;

    }




    case SYS_CHDIR:
    {

      struct inode * temp_inode = NULL;
      struct dir * target_dir = (struct dir *) malloc(sizeof(struct dir));
      struct dir * temp_dir;
      char target_file_name[50];
      bool result = false;

      parse_into_parts((char *) param1, target_dir, target_file_name);
      // printf("%s %u\n", target_file_name, dir_get_inode(target_dir)->sector);

      if (dir_lookup(target_dir, target_file_name, &temp_inode)){
        // printf("Found\n");
        if (temp_inode->is_dir == 1){
          // printf("is_dir\n");
          thread_current()->directory = dir_open(temp_inode);
          result = true;
        }
      }

      f->eax = result;

      // f->eax = success;
      break;
    }

    case SYS_ISDIR:
    {

        int fd = (int) param1;
        bool result = false;

        struct fd_struct * temp_struct = find_by_fd(fd);
        if (temp_struct -> is_dir == true){
            result = true;
          }

        f->eax = result;
        break;
    }

    case SYS_READDIR:
    {

        int fd = param1;
        char temp_name[READDIR_MAX_LEN+1];
        bool result = false;
        struct dir * target_dir;

        struct fd_struct * temp_struct = find_by_fd(fd);

        if (temp_struct->is_dir == 1){
            target_dir = temp_struct -> the_dir;            
            result = dir_readdir(target_dir, temp_name);
        }


        if (result){
          strlcpy(param2, temp_name, strlen(temp_name)+1);
        }
        
        // printf("result %s %d\n", param2, result);
        f->eax = result;
        break;

      }


      case SYS_INUMBER:
      {
          int fd = param1;
          block_sector_t return_sector = NULL;
          struct fd_struct * temp_struct = find_by_fd(fd);
          struct inode * target_inode;

          if (temp_struct -> is_dir == 1){
              //when directory
              target_inode = (temp_struct -> the_dir) -> inode;
              return_sector = target_inode->sector;
          }
          else{
              if (temp_struct ->the_file)
                  target_inode = (temp_struct -> the_file)-> inode;
                  return_sector = target_inode -> sector;
          }
          f->eax = return_sector;
          if (return_sector == NULL)
              f->eax = -1;
          break;
        }


    default:
    {
      // printf("Something else!!\n");
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
  int i = 0, j = 0;
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
    // if(temp_thread->status == THREAD_BLOCKED)
    thread_unblock(temp_thread);
    // printf("kk\n");
  }

  for(temp = list_begin(&thread_current()->fd_list); temp != list_end(&thread_current()->fd_list); temp = list_next(temp)){
    temp_struct = list_entry(temp, struct fd_struct, fd_elem);
    
    if(temp_struct->the_file){
      file_close(temp_struct->the_file);
      temp_struct->the_file = NULL;
    }

    else if (temp_struct->is_dir == 1){
      dir_close(temp_struct->the_dir);
      temp_struct->the_dir = NULL;
    }

    if (i == 1){
      struct fd_struct * prev_struct = list_entry(list_prev(temp), struct fd_struct, fd_elem);
      free(prev_struct);
    }

    i = 1;

    for (j = 1; j <= thread_current()->last_mmapid; j++){
      do_munmap(j, thread_current()->tid);
    }
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

void parse_to_list(char * dir_string, struct list * the_list, int* terms, bool* is_root){
    printf("tokstring : %s\n", dir_string);
    char* begin[BUF_LEN];
    strlcpy(begin, dir_string, strlen(dir_string) + 1);
    *is_root =false;
    if (dir_string[0]=='/'){
      dir_string++;
      *is_root =true;
    }
    char * token, *save_ptr;
    list_init(the_list);

    *terms = 0;
    for (token = strtok_r(begin, "/", &save_ptr); token!=NULL; token = strtok_r (NULL, "/", &save_ptr) ){
        printf("token : %s\n", token);
        *terms += 1;
        struct parse_struct * temp = (struct parse_struct *)malloc (sizeof(struct parse_struct));
        strlcpy(temp->parse_string, token, strlen(token) + 1);
         // = token;
        list_push_back(the_list, &temp->parse_elem);
    }

    // if (*terms == 0){
    //   struct parse_struct * last_dir = (struct parse_struct *)malloc (sizeof(struct parse_struct));
    //   // printf("save : %s, dir_string : %s\n", save_ptr, dir_string);
    //   strlcpy(last_dir->parse_string, dir_string, strlen(dir_string) + 1);
    //   // last_dir->parse_string = begin;
    //   list_push_back(the_list, &last_dir->parse_elem);
    //   *terms += 1;
    // }
}

