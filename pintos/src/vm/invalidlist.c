#include "vm/invalidlist.h"
#include "threads/thread.h"
#include "vm/frame.h"


// U
void invalid_list_insert(struct list * target_list, int sector_num){
	struct list_elem * target_elem;

	for (target_elem = list_begin(target_list); target_elem != list_end(target_list) ; target_elem = list_next(target_elem)){
		struct reference_struct * target_frame = list_entry(target_elem, struct reference_struct, reference_elem);
		struct thread * target_thread = search_by_tid(target_frame->tid);
		struct invalid_struct * target_struct = (struct invalid_struct *)malloc(sizeof(struct invalid_struct));
		target_struct->vpage = target_frame->vpage;
		target_struct->sector = sector_num;
		target_struct->lazy = 0;
		list_push_back(&target_thread->invalid_list, &target_struct->invalid_elem);
	}

	return;
}

void invalid_list_remove(struct list * target_list){
	struct list_elem * target_elem, *target_invalid_elem;
	// printf("1\n");
	for (target_elem = list_begin(target_list); target_elem != list_end(target_list) ; target_elem = list_next(target_elem)){
		struct reference_struct * target_frame = list_entry(target_elem, struct reference_struct, reference_elem);
		struct thread * target_thread = search_by_tid(target_frame->tid);
		// printf("2\n");
		for (target_invalid_elem = list_begin(&target_thread->invalid_list); target_invalid_elem != list_end(&target_thread->invalid_list); target_invalid_elem = list_next(target_invalid_elem)){

			struct invalid_struct * target_struct = list_entry(target_invalid_elem, struct invalid_struct, invalid_elem);

			if (target_struct->vpage == target_frame->vpage){
				list_remove(target_invalid_elem);
				break;
			} 

		}
	}
	return;
}

struct invalid_struct * invalid_list_check(uint8_t * kpage, bool delete){

	struct list_elem * target_elem;
	struct list * target_list = &thread_current()->invalid_list; 

	for (target_elem = list_begin(target_list) ; target_elem != list_end(target_list) ; target_elem = list_next(target_elem)){

		struct invalid_struct * target_struct = list_entry(target_elem, struct invalid_struct, invalid_elem);
        // printf("invalid : %x %x %d\n", target_struct->vpage, kpage, delete);
		if(target_struct->vpage == kpage){
			if ((target_struct->lazy == 1) && (delete) && (!(target_struct->is_mmap))){
				// printf("why?");
				list_remove(target_elem);
			}

			return target_struct;
		}
	}

	return NULL;

}

void delete_from_swap(){
	struct list_elem * target_elem;
    struct list * target_list = &thread_current()->invalid_list;
        
    for (target_elem = list_begin(target_list) ; target_elem != list_end(target_list) ; target_elem = list_next(target_elem)){
        struct invalid_struct * target_struct = list_entry(target_elem, struct invalid_struct, invalid_elem);
        if (!(target_struct->lazy)){
              
        	if ( remove_references(thread_current() -> tid, target_struct -> sector) < 1)
            	PANIC("Nothing was removed from delet_from_swap?\n");

        }
    }
}

void do_munmap(int mapid, int tid){
	// printf("DOUNMAP\n");
	int i = 0;
    struct list_elem * target_elem;
    struct list * target_list = &thread_current()->invalid_list;
    struct thread * target_thread = search_by_tid(tid);

    bool remove_last = false;
    struct list_elem * last_elem;

    for (target_elem = list_begin(target_list) ; target_elem != list_end(target_list) ; target_elem = list_next(target_elem)){
    	struct invalid_struct * target_struct = list_entry(target_elem, struct invalid_struct, invalid_elem);

      	if (remove_last){
      		// printf("removed %x\n", list_entry(last_elem, struct invalid_struct, invalid_elem)->vpage);
        	list_remove(last_elem);
      	}

      	remove_last = false;

      	if(target_struct->is_mmap){
        	if(target_struct->mmapid == mapid){
          		remove_last = true;
        	}
      	}
     	last_elem = target_elem;
    }
    
    if (remove_last){
    	// printf("removed %x\n", list_entry(last_elem, struct invalid_struct, invalid_elem)->vpage);

      	list_remove(last_elem);
    }

    target_list = &FIFO_list;

    //now check fifolist now
    // printf("DOUNMAP\n");
    remove_last = false;
    last_elem = NULL;

    for (target_elem = list_begin(target_list) ; target_elem!= list_end(target_list) ; target_elem =  list_next(target_elem)){
    	// printf("FIFO\n");
        struct frame_struct * target_struct_two = list_entry(target_elem, struct frame_struct, FIFO_elem);
    	

        if (remove_last)
          	list_remove(last_elem);
        
        remove_last = false;
        // printf("is_mmap : %d\n", target_struct_two->is_mmap);
        if (target_struct_two->is_mmap){
        	// printf("%d %d\n", target_struct_two->mmapid, mapid);
          	if(target_struct_two->mmapid == mapid){
            	remove_last = true;
            	struct list_elem * mmap_elem;
            	for (mmap_elem = list_begin(&target_struct_two->references); mmap_elem != list_end(&target_struct_two->references); mmap_elem = list_next(mmap_elem)){
            		struct reference_struct * mmap_reference = list_entry(mmap_elem, struct reference_struct, reference_elem);
                    if (pagedir_is_dirty(target_thread->pagedir, mmap_reference->vpage)){
                        inode_write_at(target_struct_two->mmap_inode, target_struct_two->kpage, target_struct_two->mmap_write_byte, target_struct_two -> mmap_offset);
                    }
                    // pagedir_clear_page(target_thread->pagedir, mmap_reference->vpage);
                    break;


            		// if (tid == mmap_reference->tid){
            		// 	if (i == 0){
              //                   inode_write_at(target_struct_two->mmap_inode, target_struct_two->kpage, target_struct_two->mmap_write_byte, target_struct_two -> mmap_offset);
              //                   pagedir_clear_page(target_thread->pagedir, mmap_reference->vpage);
              //                   i++;
            		// 		}
            		// 	}

            			// printf("clear page : %x\n", mmap_reference->vpage);

            			// Remove reference_elem in list of frame_struct

            		
            		
            	}

          	}	
        }

        last_elem = target_elem;
    }

    if(remove_last)
      list_remove(last_elem);
}