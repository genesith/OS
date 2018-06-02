#include <list.h>
#include <stdio.h>
#include "filesys/inode.h"
#include "threads/synch.h"
#include "devices/block.h"
#include "threads/thread.h"

// #define BLOCKSIZE 4*1024*1024
#define SECTORNUM 64
#define SECTORSIZE 512
#define FLUSHERFREQ 500
struct block * file_block;
struct lock buffer_cache_lock;
struct list buffer_cache;
struct semaphore read_aheader_sema;
bool is_read_aheader;
block_sector_t read_aheader_sector;

struct cache_struct{
	bool is_dirty;
	bool is_empty;
	int count;
	void * data;
	block_sector_t sector;
	struct lock cache_lock;
	struct list_elem cache_elem;
};

struct aux_struct{
	int sector;
	struct cache_struct * free_cache;
};

struct cache_struct * cache_search(block_sector_t sector);
struct cache_struct * cache_get_free(void);
void cache_read(block_sector_t sector, void *buffer);
void cache_write(block_sector_t sector, void *buffer);
void cache_init();
void flusher();
void cache_read_aheader(void * sector);
