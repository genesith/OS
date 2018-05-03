#include <list.h>
#include <stdint.h>
#include "devices/block.h"

struct swap_struct {
   bool empty;
   struct list * references;
};

block_sector_t total_sector_number;

uint8_t * global_swap_table;

void init_swap_table(void);

block_sector_t get_free_swap(void);

void swap_out(block_sector_t sector_num, struct list * refs, uint8_t * kaddr);
struct list * swap_in(uint8_t *kaddr, int sector_num);