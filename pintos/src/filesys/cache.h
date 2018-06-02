#include <list.h>
#include "filesys/inode.h"
#include "threads/synch.h"
#include "devices/block.h"
#include "threads/thread.h"

// #define BLOCKSIZE 4*1024*1024
#define SECTORNUM 64
#define SECTORSIZE 512

struct block * file_block;

struct list buffer_cache;
struct lock buffer_lock;
struct cache_struct{
	bool is_dirty;
	bool is_empty;
	int count;
	void * data;
	int sector;
	struct lock cache_lock;
	struct list_elem cache_elem;
};

struct cache_struct * cache_search(int sector);
struct cache_struct * cache_get_free(void);
void cache_read(block_sector_t sector, void *buffer);
void cache_write(block_sector_t sector, void *buffer);
void cache_init();