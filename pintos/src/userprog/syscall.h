#include <list.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"

#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H


void syscall_init (void);
bool check_invalid_pointer(void * addr);

struct fd_struct {
	int fd;
	struct file* the_file;
	struct list_elem fd_elem;
};

#endif /* userprog/syscall.h */
