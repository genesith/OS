#include "threads/malloc.h"
#include "vm/swap.h"
#include <stdint.h>

/* At initialization, create a swap table by getting the number of total sectors used as swap*/

void init_swap_table (int total_sectors) {
   int total_sector_number = total_sectors;
   int temp_int = 0;
   uint8_t * swap_table = (uint8_t *)malloc(sizeof(struct swap_struct) * total_sector_number);
   while (temp_int < total_sectors){
      struct swap_struct * temp_struct= (struct swap_struct *) malloc(sizeof (struct swap_struct));
      temp_struct->empty = true;
      list_init(&temp_struct->references);
      memcpy((void *) (swap_table + temp_int * sizeof(struct swap_struct)), temp_struct, sizeof(struct swap_struct));
      free (temp_struct);
      temp_int ++;
   }
   
   global_swap_table = swap_table;
}


int get_free_swap () {
   static int last_search = 0;
   int temp2, temp;
   for (temp = 0; temp < total_sector_number; temp ++){
      temp2 = (last_search + temp) % total_sector_number;
      struct swap_struct * temp_struct = (struct swap_struct *)(global_swap_table + temp2 * sizeof(struct swap_struct));
      if (temp_struct -> empty == true){
         last_search = temp2;
         return temp2;
      }
   }
   PANIC ("SWAP IS FULL\n");
   return -1;
}

void swap_out(int sector_num, struct list * references){
	return;
}
