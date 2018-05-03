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
		list_push_back(&target_thread->invalid_list, &target_struct->invalid_elem);
	}

	return;
}

void invalid_list_remove(struct list * target_list){
	struct list_elem * target_elem, *target_invalid_elem;

	for (target_elem = list_begin(target_list); target_elem != list_end(target_list) ; target_elem = list_next(target_elem)){
		struct reference_struct * target_frame = list_entry(target_elem, struct reference_struct, reference_elem);
		struct thread * target_thread = search_by_tid(target_frame->tid);
		
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

int invalid_list_check(uint8_t * kpage){
	struct list_elem * target_elem;
	struct list * target_list = &thread_current()->invalid_list; 

	for (target_elem = list_begin(target_list) ; target_elem != list_end(target_list) ; target_elem = list_next(target_elem)){

		struct invalid_struct * target_struct = list_entry(target_elem, struct invalid_struct, invalid_elem);
		if(target_struct->vpage == kpage)
			return target_struct->sector;
	}

	return -1;

}

