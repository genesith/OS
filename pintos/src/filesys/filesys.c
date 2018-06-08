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
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  printf("create\n");

  struct dir * target_dir = (struct dir *) malloc(sizeof(struct dir));
  char target_file_name[50];
    // struct dir target_dir2;
  // char target_file_name2[16];

  // printf("%x %x %x %x\n", target_dir, target_file_name, &target_dir2, &target_file_name2);
  parse_into_parts(name, target_dir, target_file_name);
  // printf("target_dir : %x\n", target_dir1);
  // printf("thread_Current : %s file_name : %s, directory sector : %u\n", thread_current()->name, target_file_name, dir_get_inode(target_dir)->sector); 
 
  block_sector_t inode_sector = 0;
  bool success = (target_dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
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
filesys_open (const char *name)
{
  // printf("%s\n", name);
  // printf("0\n");
  struct dir *dir = dir_open_root ();
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
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  
  free_map_create ();

  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");

  free_map_close ();
  printf ("done.\n");
}


void parse_into_parts(char * path_string, struct dir * output_dir, char* output_name){
    char begin[50];
    char second[50];
    struct dir* temp_dir;
    int terms = 0;
    struct inode ** inode_holder;
    memcpy(begin, path_string, strlen(path_string) + 1);
    bool error = false;

    if ((path_string[0]=='/') || (!(thread_current()->directory))){
      // printf("root");
      temp_dir = dir_open_root();
    }
    else{
      // printf("here %s\n", thread_current()->name);
      temp_dir = dir_reopen(thread_current()->directory);
      // printf("parse dir : %x\n", temp_dir);
    }

    char * token, *save_ptr, *prev;

    for (token = strtok_r(begin, "/", &save_ptr); token!=NULL; token = strtok_r (NULL, "/", &save_ptr) ){
        terms += 1;
        // printf("term #: %d, token : %s\n", terms, token);

    }
    // printf ("Terms read : %d\n", terms);
    memcpy(second, path_string, strlen(path_string) + 1);
    // printf("0 %s\n", begin);
    if (terms >1){
      for (token = strtok_r(second, "/", &save_ptr); (token!=NULL) && (terms > 1) ; token = strtok_r (NULL, "/", &save_ptr), terms-=1 ){
        // printf("find %s\n", token);
        // prev = token;
        if (dir_lookup(temp_dir, prev, inode_holder)){
          // printf("success to find\n");
          if ((*inode_holder)->is_dir){
            // printf("b\n");
            temp_dir = dir_open(*inode_holder);
          }
        }
        else {
          printf("Potential error 1");
          error = true;
        }
      }

      token = strtok_r(NULL, "/", &save_ptr);
      // printf("token : %x\n", prev);
    }

    else
      token = strtok_r(begin, "/", &save_ptr);

    // printf("1 %s\n", token);
    memcpy (output_name, token, strlen(token) +1);
    // printf("2\n");
    memcpy(output_dir, temp_dir, sizeof(struct dir));
    // printf("3\n");
    if (error){
      output_dir = NULL;
    }
}