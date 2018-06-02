#include "filesys/cache.h"

// When we have to Initialize?????

/*
	Flusher

*/

/*
	write aheader
	 - Check whether next block is already in cache
	 - Run if only hit!
*/


void cache_init(){
	int i;
	struct cache_struct * temp_struct;
	list_init(&buffer_cache);
	for(i = 0; i < SECTORNUM; i++){
		temp_struct = (struct cache_struct *)malloc(sizeof(struct cache_struct));
		temp_struct->is_dirty = 0;
		temp_struct->is_empty = 1;
		temp_struct->count = 0;
		temp_struct->sector = -1;
		temp_struct->data = malloc(SECTORSIZE);
		// printf("malloc address : %x\n", temp_struct->data);
		lock_init(&temp_struct->cache_lock);
		list_push_back(&buffer_cache, &temp_struct->cache_elem);
	}
	file_block = block_get_role(1);
	lock_init(&buffer_lock);

	return;
}



void cache_read(block_sector_t sector, void *buffer){
	struct cache_struct * target_struct;
	// printf("read address, %d %x\n", sector, buffer);
	// if (buffer > 0xc0000000){
	// 	// printf("read kernel, %x\n", buffer);
	// 	block_read(file_block, sector, buffer);
	// 	return;
	// }
	// printf("thread_current : %s\n", thread_current()->name);
	// lock_acquire(&buffer_lock);
	if ((target_struct = cache_search(sector)) == NULL){
		// printf("read1\n");
		target_struct = cache_get_free();
		lock_acquire(&target_struct->cache_lock);
		block_read(file_block, sector, target_struct->data);
		lock_release(&target_struct->cache_lock);
		target_struct->sector = sector;
		target_struct->is_empty = 0;
	}
	// printf("read2\n");
	target_struct->count += 1;
	memcpy(buffer, target_struct->data, SECTORSIZE);
	// lock_release(&buffer_lock);
	return;
}


void cache_write(block_sector_t sector, void *buffer){
	// printf("write address, %d %x\n", sector, buffer);
	// if (buffer > 0xc0000000){
	// 	// printf("write kernel%x\n", buffer);
	// 	block_write(file_block, sector, buffer);
	// 	return;
	// }
	// printf("thread_current : %s\n", thread_current()->name);
	// lock_acquire(&buffer_lock);
	struct cache_struct * target_struct;
	if ((target_struct = cache_search(sector)) == NULL){
		// printf("write1??\n");
		target_struct = cache_get_free();
		lock_acquire(&target_struct->cache_lock);
		block_read(file_block, sector, target_struct->data);
		lock_release(&target_struct->cache_lock);		
		target_struct->sector = sector;
		target_struct->is_empty = 0;
	}

	// printf("write2??\n");

	target_struct->count += 1;
	target_struct->is_dirty = 1;
	memcpy(target_struct->data, buffer, SECTORSIZE);
	// lock_release(&buffer_lock);
	return;
}


struct cache_struct * cache_search(int sector){
	struct list_elem * target_elem;
	struct cache_struct * temp_struct;
	for (target_elem = list_begin(&buffer_cache); target_elem != list_end(&buffer_cache); target_elem = list_next(target_elem)){
		temp_struct = list_entry(target_elem, struct cache_struct, cache_elem);
		if ((temp_struct->sector == sector)){
			return temp_struct;
		}
	}

	return NULL;
}



// Checks the  cache if there is a free slot. If there is one, return the data address. If not, return NULL.
struct cache_struct * cache_get_empty(void){

    struct list_elem * temp_elem;

    for (temp_elem = list_begin(&buffer_cache) ; temp_elem != list_end (&buffer_cache) ; temp_elem = list_next(temp_elem)){
        struct cache_struct * temp_struct = list_entry(temp_elem, struct cache_struct, cache_elem);
        if (temp_struct -> is_empty)
          return temp_struct;
    }

    //if not empty, simply return null
    return NULL;
}


// Finds the cache_struct for the cache block that should be evicted via LFU.
struct cache_struct  * cache_to_evict(void){
    struct list_elem * temp_elem;
    int min_count = 0;
    struct cache_struct * target_struct;

    for (temp_elem  = list_begin(&buffer_cache) ; temp_elem != list_end (&buffer_cache) ; temp_elem = list_next(temp_elem)){
        struct cache_struct * temp_struct = list_entry(temp_elem, struct cache_struct, cache_elem);
        if (min_count == 0){
            min_count = temp_struct->count;
            target_struct = temp_struct;
        }
        else if (min_count > temp_struct->count){
            min_count = temp_struct->count;
            target_struct = temp_struct;
        }
    }
    // printf("replace will be : %d\n", target_struct->sector);
    return target_struct;


}

// The high level function finding the cache to allocate.
struct cache_struct * cache_get_free(void){
    struct cache_struct * temp_struct = cache_get_empty();
    // if there is a free slot, return it
    if (temp_struct)
        return temp_struct;
    //else, we need to evict one
    temp_struct = cache_to_evict();
    if (temp_struct->is_dirty){
        block_write(file_block, temp_struct->sector, temp_struct->data);
    }

    temp_struct->is_dirty = 0;
    temp_struct->is_empty = 1;
    temp_struct->count = 0;
    // memset(temp_struct->data, 0, SECTORSIZE);

    return temp_struct;
}


