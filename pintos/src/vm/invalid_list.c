#include "vm/invalid_list.h"
#include "threads/thread.h"


// U
void invalid_list_insert(struct list * target_list, int sector_num){
	struct list_elem * temp_elem;

	for (temp_elem = list_begin(target_list); temp_elem != list_end(target_list) ; temp_elem = list_next(temp_elem)){


	}
	
	return;
}

void invalid_list_remove(struct list * target_list){
	return;
}
