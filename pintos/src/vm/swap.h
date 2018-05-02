#include <list.h>
#include <stdint.h>

struct swap_struct {
   bool empty;
   struct list references;
};

int total_sector_number;

uint8_t * global_swap_table;