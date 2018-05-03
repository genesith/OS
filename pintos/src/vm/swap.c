#include "threads/malloc.h"
#include "vm/swap.h"
#include <stdint.h>
#include <list.h>
#include "devices/block.h"

/* At initialization, create a swap table by getting the number of total sectors used as swap*/

void init_swap_table () {
   total_sector_number = block_size (block_get_role(3));
   printf("here\n");
   int temp_int = 0;
   uint8_t * swap_table = (uint8_t *)malloc(sizeof(struct swap_struct) * total_sector_number);
   while (temp_int < total_sector_number){
      struct swap_struct * temp_struct= (struct swap_struct *) (swap_table + temp_int * sizeof(struct swap_struct));
      temp_struct->empty = true;
      list_init(&temp_struct->references);
      temp_int ++;
   }
   
   global_swap_table = swap_table;
}

// Look at the swap table, and find an empty swap sector and return its number
block_sector_t get_free_swap () {
   static int last_search = 0;
   int temp2, temp;
   for (temp = 0; temp < total_sector_number; temp ++){
      temp2 = (last_search + temp) % total_sector_number;
      struct swap_struct * temp_struct = (struct swap_struct *)(global_swap_table + temp2 * sizeof(struct swap_struct));
      if (temp_struct->empty == true){
         last_search = temp2;
         return temp2;
      }
   }
   PANIC ("SWAP IS FULL\n");
   return -1;
}

void swap_out(block_sector_t sector_num, struct list * refs, uint8_t * kaddr){
	struct swap_struct * this_struct = (struct swap_struct *)(global_swap_table + sector_num * sizeof (struct swap_struct));
	ASSERT (this_struct->empty);
	block_write(block_get_role(3), sector_num, kaddr);
	this_struct -> empty =false;
	this_struct -> references = refs;
}

struct list * swap_in (uint8_t * kaddr, int sector_num){
        struct swap_struct * this_struct = (struct swap_struct *)(global_swap_table + sector_num * sizeof(struct swap_struct));
        ASSERT (!(this_struct ->empty));
        block_read(block_get_role(3), (block_sector_t) sector_num, kaddr);
        this_struct -> empty = true;
        struct list * return_list = this_struct -> references;
        this_struct -> references = NULL;
        return return_list;
}