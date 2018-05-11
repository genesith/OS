#include "threads/malloc.h"
#include "vm/swap.h"
#include <stdint.h>
#include <list.h>
#include "devices/block.h"
#include "vm/invalidlist.h"
#include "vm/frame.h"

/* At initialization, create a swap table by getting the number of total sectors used as swap*/

void init_swap_table () {
   total_sector_blocks = block_size (block_get_role(3)) / 8;
   //printf("here\n");
   int temp_int = 0;
   uint8_t * swap_table = (uint8_t *)malloc(sizeof(struct swap_struct) * total_sector_blocks);
   while (temp_int < total_sector_blocks){
      struct swap_struct * temp_struct= (struct swap_struct *) (swap_table + temp_int * sizeof(struct swap_struct));
      temp_struct->empty = true;
      list_init(&temp_struct->references);
      temp_int ++;
   }
   lock_init(&swap_lock);
   global_swap_table = swap_table;
}

// Look at the swap table, and find an empty swap sector and return its number
block_sector_t get_free_swap () {
    lock_acquire(&swap_lock);

   static int last_search = 0;
   int temp2, temp;
   for (temp = 0; temp < total_sector_blocks; temp ++){
      temp2 = (last_search + temp) % total_sector_blocks;
      struct swap_struct * temp_struct = (struct swap_struct *)(global_swap_table + temp2 * sizeof(struct swap_struct));
      if (temp_struct->empty == true){
        last_search = temp2;
        lock_release(&swap_lock);

        return temp2;
      }
   }
   PANIC ("SWAP IS FULL\n");
   return -1;
}

void swap_out(block_sector_t sector_num, struct list * refs, uint8_t * kaddr){
	int i;

  lock_acquire(&swap_lock);

  struct swap_struct * this_struct = (struct swap_struct *)(global_swap_table + sector_num * sizeof (struct swap_struct));
	ASSERT (this_struct->empty);
  for(i = 0; i < SECTORS_PER_PAGE; i++){
    // break;
    block_write(block_get_role(3), 8*sector_num + i, kaddr + i * BLOCK_SECTOR_SIZE);
  }
  lock_release(&swap_lock);
	this_struct -> empty =false;
	this_struct -> references = refs;
}

struct list * swap_in (uint8_t * kaddr, int sector_num){
        int i;
        lock_acquire(&swap_lock);

        struct swap_struct * this_struct = (struct swap_struct *)(global_swap_table + sector_num * sizeof(struct swap_struct));
        ASSERT (!(this_struct ->empty));
        // printf("here4");
        for(i = 0; i < SECTORS_PER_PAGE; i++){
          // printf("b");
          // printf("%s\n", block_get_role(3));
          block_read(block_get_role(3), (block_sector_t) 8*sector_num + i, kaddr + i * BLOCK_SECTOR_SIZE);
        }
        // printf("here3");
        // block_read(block_get_role(3), (block_sector_t) sector_num, kaddr);
        this_struct -> empty = true;
        struct list * return_list = this_struct -> references;
        this_struct -> references = NULL;
        lock_release(&swap_lock);
        return return_list;
}

int remove_references(int remove_tid, block_sector_t sector){
        int count = 0;
        struct list_elem * target_elem;
        struct swap_struct * this_struct = (struct swap_struct *)(global_swap_table + sector * sizeof(struct swap_struct));
        ASSERT(!(this_struct ->empty));
        struct list * remove_from_here = this_struct -> references;

        for (target_elem = list_begin(remove_from_here) ; target_elem != list_end(remove_from_here) ; target_elem = list_next(target_elem)){
                struct reference_struct * target_struct = list_entry(target_elem, struct reference_struct, reference_elem);
                if (target_struct -> tid == remove_tid){
                        list_remove(target_elem);
                        count ++;
                        if (list_empty(remove_from_here)){
                          this_struct->empty = true;
                          return count;
                        }
                }

        }

        return count;
}