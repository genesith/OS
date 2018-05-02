#include <list.h>

void invalid_list_insert(struct list * target_list, int sector_num);

struct invalid_struct{
	uint8_t * vpage_index;
	int sector;
	struct list_elem invalid_elem;
};




