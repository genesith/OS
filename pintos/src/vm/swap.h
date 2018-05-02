#include <list.h>
#include <stdint.h>
#include "devices/block.h"

struct swap_struct {
   bool empty;
   struct list * references;
};

extern block_sector_t total_sector_number;

uint8_t * global_swap_table;
