#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/block.h"

#define SECTORS_PER_PAGE PGSIZE/BLOCK_SECTOR_SIZE

struct swap_struct {
   bool empty;
   struct list * references;
};

block_sector_t total_sector_blocks;

struct lock swap_lock;

uint8_t * global_swap_table;

void init_swap_table(void);

block_sector_t get_free_swap(void);

void swap_out(block_sector_t sector_num, struct list * refs, uint8_t * kaddr);
struct list * swap_in(uint8_t *kaddr, int sector_num);