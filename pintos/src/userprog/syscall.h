#include <list.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/synch.h"
#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H


void syscall_init (void);
bool check_invalid_pointer(void * addr);
void exit(int status);

struct fd_struct {
	int fd;
	struct file* the_file;
	struct list_elem fd_elem;
	struct semaphore file_sema;
};

struct mmap_struct{
	int mapid;
	int inode_num;
	struct list_elem mmap_elem;
};

#endif /* userprog/syscall.h */
