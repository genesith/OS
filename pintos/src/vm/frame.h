#include <stdio.h>
#include <list.h>
#include <stdint.h>

struct frame_struct{
	uint32_t * kernel_address;
	struct list references;
	struct list_elem FIFO_elem;
};

struct reference_struct{

	int tid;
	uint32_t * virtual_address;
	struct list_elem reference_elem;

};


uint32_t * frame_table;
uint32_t * userpool;

struct list FIFO_list;

struct frame_struct * get_evict_frame(void);
uint8_t * frame_allocate(void);