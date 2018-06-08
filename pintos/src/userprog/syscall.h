#include <list.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/synch.h"
#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#define BUF_LEN 50

void syscall_init (void);
bool check_invalid_pointer(void * addr);
void exit(int status);

struct fd_struct {
	bool is_dir;
	int fd;
	struct file* the_file;
	struct dir* the_dir; 

	struct list_elem fd_elem;
	struct semaphore file_sema;
};

struct mmap_struct{
	int mapid;
	int inode_num;
	struct list_elem mmap_elem;
};


void parse_to_list(char * dir_string, struct list * the_list, int* terms, bool *is_root);

struct parse_struct
{
    struct list_elem parse_elem;
    char parse_string[BUF_LEN];
};


#endif /* userprog/syscall.h */
