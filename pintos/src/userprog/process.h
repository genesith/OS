#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "vm/invalidlist.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool install_page (int tid, void *upage, void *kpage, bool writable, bool fifo, struct invalid_struct * target_struct);

#endif /* userprog/process.h */
