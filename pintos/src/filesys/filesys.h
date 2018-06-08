#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include <list.h>
#include "filesys/directory.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */
#define MAX_FILE_LEN 512

/* Block device that contains the file system. */
struct block *fs_device;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size, int is_dir);
struct file* filesys_open (const char *name, struct dir * dir);
bool filesys_remove (const char *name);
void parse_into_parts(char * path_string, struct dir * output_dir, char * output_name);



#endif /* filesys/filesys.h */
