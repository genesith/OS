#include <stdio.h>
#include <stdint.h>
#include "vm/frame.h"
#include "threads/vaddr.h"
#include "vm/invalidlist.h"
#include "threads/palloc.h"


uint8_t * frame_allocate(void){

    uint8_t * kpage = palloc_get_page(PAL_USER | PAL_ZERO);
    
    // When pysical memory is full
    if (!(kpage)){
      struct frame_struct * evict_frame;
      int sector_num = get_free_swap();
      evict_frame = get_evict_frame();

      swap_out(sector_num, &evict_frame->references, kpage);

      //Update Invalid page table;
      invalid_list_insert(&evict_frame->references, sector_num);

      /*
      //Update Present bit to 0 for every references of evict frame
      */

      
      kpage = (uint8_t *) evict_frame->kpage;
    
    }

    // When there is available pysical memory
    
    return kpage;
    

}

struct frame_struct * get_evict_frame(void){

    struct list_elem * evict_elem = list_pop_front(&FIFO_list);
    struct frame_struct * evict_frame = list_entry(evict_elem, struct frame_struct, FIFO_elem);

    return evict_frame; 
}

