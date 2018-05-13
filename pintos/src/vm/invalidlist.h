#include <list.h>
#include <stdint.h>

void invalid_list_insert(struct list * target_list, int sector_num);
void invalid_list_remove(struct list * target_list);
struct invalid_struct * invalid_list_check(uint8_t * kpage, bool delete);
void delete_from_swap();

struct invalid_struct{
	uint8_t * vpage;
	int sector;
	struct list_elem invalid_elem;
	bool lazy;
	struct inode * lazy_inode;
	int lazy_offset;
	int lazy_write_byte;
	bool lazy_writable;
};




