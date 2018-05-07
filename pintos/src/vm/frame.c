#include <stdio.h>
#include <stdint.h>
#include "threads/thread.h"
#include "vm/frame.h"
#include "threads/vaddr.h"
#include "vm/invalidlist.h"
#include "threads/palloc.h"


uint8_t * frame_allocate(void){

  uint8_t * kpage = palloc_get_page(PAL_USER | PAL_ZERO);
    
    // When pysical memory is full
  if (!(kpage)){
    // printf("kpage is NULL\n");
    struct frame_struct * evict_frame;
    struct list_elem * target_elem;
    int sector_num = get_free_swap();
    evict_frame = get_evict_frame();

    swap_out(sector_num, &evict_frame->references, evict_frame->kpage);

      //Update Invalid page table;
    invalid_list_insert(&evict_frame->references, sector_num);

      /*
      //Update Present bit to 0 for every references of evict frame
      */

    for (target_elem = list_begin(&evict_frame->references); target_elem != list_end(&evict_frame->references); target_elem = list_next(target_elem)){
      struct reference_struct * target_struct = list_entry(target_elem, struct reference_struct, reference_elem);
      struct thread * target_thread = search_by_tid(target_struct->tid);
      pagedir_clear_page(target_thread->pagedir, target_struct->vpage);
    }
  
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

