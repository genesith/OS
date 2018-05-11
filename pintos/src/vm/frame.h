#include <stdio.h>
#include <list.h>
#include <stdint.h>
#include "threads/thread.h"

struct frame_struct{
	uint8_t * kpage;
	struct list references;
	struct list_elem FIFO_elem;
};

struct reference_struct{

	int tid;
	uint8_t * vpage;
	struct list_elem reference_elem;

};


uint8_t * frame_table;
uint8_t * userpool;

struct list FIFO_list;

struct frame_struct * get_evict_frame(void);
uint8_t * frame_allocate(void);
void frame_delete(tid_t tid, uint8_t * vpage);