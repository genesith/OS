#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "threads/synch.h"
#include "userprog/syscall.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  cache_init();


  if (format) 
    do_format ();

  free_map_open ();
}



/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, int is_dir) 
{
  // printf("create\n");


  struct dir * target_dir = (struct dir *) malloc(sizeof(struct dir));
  char target_file_name[MAX_FILE_LEN];
  struct dir * parent_dir = NULL;
  parse_into_parts(name, target_dir, target_file_name);

  if (is_dir == 1){
    parent_dir = dir_reopen(target_dir);
  }

  // printf("create name :%s, %d parent : %x\n", name, is_dir, parent_dir);

  block_sector_t inode_sector = 0;
  bool success = (target_dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir, parent_dir)
                  && dir_add (target_dir, target_file_name, inode_sector));





  // printf("result : %d\n", success);
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (target_dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name, struct dir * dir)
{
  // printf("%s\n", name);
  // printf("0\n");
  if (!(dir)){
    dir = dir_open_root();
  }
  // struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;
  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);


  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  struct dir *target_dir = (struct dir*) malloc (sizeof(struct dir));
  char target_file_name[MAX_FILE_LEN];
  parse_into_parts(name, target_dir, target_file_name);

  // printf("remove %s %u\n", target_file_name, dir_get_inode(target_dir)->sector);

  if (target_dir == NULL){
      target_dir = dir_open_root();
  }

  struct dir * par_dir;
  struct inode * par_inode;

  if(dir_lookup(target_dir, target_file_name, &par_inode)){
    // printf("find it\n");
    if(thread_current()->directory){
      if (dir_get_inode(thread_current()->directory)->parent_dir){
        if ((par_inode->sector == dir_get_inode(dir_get_inode(thread_current()->directory)->parent_dir)->sector)){
        // printf("return false\n");
          return false;
        }
      }
    }

    struct list_elem * temp_elem;
    struct fd_struct * temp_struct;
    for(temp_elem = list_begin(&thread_current()->fd_list); temp_elem != list_end(&thread_current()->fd_list) ; temp_elem = list_next(temp_elem)){
      temp_struct = list_entry(temp_elem, struct fd_struct, fd_elem);
      if ((temp_struct->is_dir == 1) && (dir_get_inode(temp_struct->the_dir)->sector == par_inode->sector))
        return false;
    }


  }





  // printf("start remove\n");
  bool success = dir_remove (target_dir, target_file_name);
  // printf("remove done\n");
  dir_close (target_dir);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  
  free_map_create ();

  if (!dir_create (ROOT_DIR_SECTOR, 0))
    PANIC ("root directory creation failed");

  free_map_close ();
  printf ("done.\n");
}



void parse_into_parts(char * path_string, struct dir * output_dir, char* output_name){
    char begin[MAX_FILE_LEN];
    char second[MAX_FILE_LEN];
    struct dir* temp_dir;
    int terms = 0;
    struct inode * inode_holder = NULL;

    memcpy(begin, path_string, strlen(path_string) + 1);

    bool error = false;
    bool single = 0;

    if ((path_string[0]=='/') || (!(thread_current()->directory))){
      // printf("root");
      temp_dir = dir_open_root();
      // path_string+=1;
    }
    else{
      // printf("here %s\n", thread_current()->name);
      temp_dir = dir_reopen(thread_current()->directory);
      // printf("parse dir : %x\n", temp_dir);
    }

    char * token, *token1, *save_ptr, *save_ptr2;
    char prev[MAX_FILE_LEN];
    char next[MAX_FILE_LEN];

    for (token = strtok_r(begin, "/", &save_ptr); token!=NULL; token = strtok_r (NULL, "/", &save_ptr) ){
      terms += 1;
        // printf("term #: %d, token : %s\n", terms, token);
    }
    // printf ("Terms read : %d\n", terms);
    memcpy(second, path_string, strlen(path_string) + 1);
    // printf("second: %s\n", second);
    if (terms >1){
      for (token1 = strtok_r(second, "/", &save_ptr2); (token1!=NULL) && (terms > 1) ; token1 = strtok_r(NULL, "/", &save_ptr2), terms-=1){
        memcpy(prev, token1, strlen(token1)+1);
        
        if (!strcmp(prev, ".")){
          continue;
        }

        else if (!strcmp(prev, "..")){
          temp_dir = (temp_dir->inode)->parent_dir;
        }

        // printf("first strtok gave token1 as %s, sent to prev: %s. Now there is %s reamining on string\n", prev, token1, save_ptr2);

        else if (dir_lookup(temp_dir, prev, &inode_holder)){
          // printf("success to find using prev: %s\n", prev);
          if ((inode_holder->is_dir) == 1){

            temp_dir = dir_open(inode_holder);
            // printf("after find token : %s prev : %s\n", token1, prev);
          }
        }

        else {
          // printf("Potential error 1");
          error = true;
        }
      }

      // printf("Outside for loop, the end of path should be token1: %s\n", token1);
      // memcpy(prev, token1, strlen(token1) + 1);
      // printf("prev : %s token : %s next : %s\n", prev, token1, next);
    }

    else{

      token = strtok_r(begin, "/", &save_ptr2);

      single = 1;
    }

    if ((single == 1)) {
      if(token){
        memcpy(output_name, token, strlen(token) + 1);
      // printf("single case, output is : %s\n", output_name);
      }
      else{
        memcpy(output_name, ".", 2);
      }
    }
    else{
      
      memcpy (output_name, token1, strlen(token1) +1);
      // printf("Non-single case, final name is %s\n", output_name);
    }
    
    memcpy(output_dir, temp_dir, sizeof(struct dir));
    
    if (error){
      output_dir = NULL;
    }
}