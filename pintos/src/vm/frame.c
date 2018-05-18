#include <stdio.h>
#include <stdint.h>
#include "threads/thread.h"
#include "vm/frame.h"
#include "threads/vaddr.h"
#include "vm/invalidlist.h"
#include "threads/palloc.h"


uint8_t * frame_allocate(void){

  uint8_t * kpage = palloc_get_page(PAL_USER | PAL_ZERO);
  // printf("origin : %x\n", kpage);
    // When pysical memory is full
  if (!(kpage)){
    // printf("kpage is NULL\n");
    struct frame_struct * evict_frame;
    struct list_elem * target_elem;
    int sector_num = get_free_swap();
    evict_frame = get_evict_frame();
    kpage = (uint8_t *) evict_frame->kpage;

    // hex_dump(evict_frame->kpage, evict_frame->kpage, 4096, true);
    if (!(evict_frame->is_mmap)){
      swap_out(sector_num, &evict_frame->references, evict_frame->kpage);
    
    // memset(evict_frame->kpage, 0, 30);
    // hex_dump(evict_frame->kpage, evict_frame->kpage, 30, true);
    // swap_in(evict_frame->kpage, sector_num);
    // hex_dump(evict_frame->kpage, evict_frame->kpage, 30, true);

      //Update Invalid page table;
      invalid_list_insert(&evict_frame->references, sector_num);
    }


    else{
      inode_write_at(evict_frame->mmap_inode, kpage, evict_frame->mmap_write_byte, evict_frame->mmap_offset);
    }
     

    for (target_elem = list_begin(&evict_frame->references); target_elem != list_end(&evict_frame->references); target_elem = list_next(target_elem)){
      struct reference_struct * target_struct = list_entry(target_elem, struct reference_struct, reference_elem);
      struct thread * target_thread = search_by_tid(target_struct->tid);
      pagedir_clear_page(target_thread->pagedir, target_struct->vpage);
    }
  
  }

    // When there is available pysical memory
    
  return kpage;
    

}

struct frame_struct * get_evict_frame(void){

    struct list_elem * evict_elem = list_pop_front(&FIFO_list);
    struct frame_struct * evict_frame = list_entry(evict_elem, struct frame_struct, FIFO_elem);

    return evict_frame; 
}

void frame_delete(tid_t tid, uint8_t * vpage){
  struct list_elem * target_elem;
  struct list_elem * ref_elem;

  for (target_elem = list_begin(&FIFO_list); target_elem != list_end(&FIFO_list) ; target_elem = list_next(target_elem)){
    struct frame_struct * target_struct = list_entry(target_elem, struct frame_struct, FIFO_elem);
    
    for (ref_elem = list_begin(&target_struct->references); ref_elem != list_end(&target_struct->references); ref_elem = list_next(ref_elem)){
      struct reference_struct * ref_struct = list_entry(ref_elem, struct reference_struct, reference_elem);

      if ((ref_struct->vpage == vpage) && (ref_struct->tid == tid)){
        list_remove(ref_elem);
        
        if (list_empty(&target_struct->references)){
          list_remove(target_elem);
        }
        
        break;
      }
    }
  }
}
