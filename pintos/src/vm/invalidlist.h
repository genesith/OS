#include <list.h>
#include <stdint.h>

void invalid_list_insert(struct list * target_list, int sector_num);
void invalid_list_remove(struct list * target_list);
int invalid_list_check(uint8_t * kpage);

struct invalid_struct{
	uint8_t * vpage;
	int sector;
	struct list_elem invalid_elem;
};




