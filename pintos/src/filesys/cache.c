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
	lock_init(&buffer_cache_lock);
	file_block = block_get_role(1);
	is_read_aheader = 0;
	read_aheader_sector = -1;
	sema_init(&read_aheader_sema, 1);
	// thread_create("Flusher", PRI_DEFAULT, flusher, NULL);

	return;
}



void cache_read(block_sector_t sector, void *buffer){
	// printf("read sector : %x\n", sector);
	lock_acquire(&buffer_cache_lock);
	// if (sector == read_aheader_sector){
	// 	sema_down(&read_aheader_sema);
	// 	sema_up(&read_aheader_sema);
	// }


	struct cache_struct * target_struct;
	if ((target_struct = cache_search(sector)) == NULL){
		// printf("read1\n");
		target_struct = cache_get_free();
		// lock_acquire(&target_struct->cache_lock);
		block_read(file_block, sector, target_struct->data);
		// printf("block_read %x done\n", sector);
		// lock_release(&target_struct->cache_lock);
		target_struct->sector = sector;
		target_struct->is_empty = 0;
	}
	// printf("read2\n");
	target_struct->count += 1;

	memcpy(buffer, target_struct->data, SECTORSIZE);
	lock_release(&buffer_cache_lock);
	// lock_release(&buffer_lock);
	return;
}


void cache_write(block_sector_t sector, void *buffer){
	// printf("write sector : %x\n", sector);
	lock_acquire(&buffer_cache_lock);

	// if (sector == read_aheader_sector){
	// 	sema_down(&read_aheader_sema);
	// 	sema_up(&read_aheader_sema);
	// }

	struct cache_struct * target_struct;
	if ((target_struct = cache_search(sector)) == NULL){
		// printf("write1??\n");
		target_struct = cache_get_free();
		// lock_acquire(&target_struct->cache_lock);
		block_read(file_block, sector, target_struct->data);
		// printf("block_read %x done\n", sector);

		// lock_release(&target_struct->cache_lock);		
		target_struct->sector = sector;
		target_struct->is_empty = 0;
	}

	// printf("write2??\n");

	target_struct->count += 1;
	target_struct->is_dirty = 1;
	// if (!(cache_search(sector+1))){
	// 	thread_create("Read_aheader", PRI_DEFAULT, cache_read_aheader, sector+1);
	// }
	memcpy(target_struct->data, buffer, SECTORSIZE);
	// lock_release(&buffer_lock);
	lock_release(&buffer_cache_lock);

	return;
}


struct cache_struct * cache_search(block_sector_t sector){
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
    	// printf("start_write");
        block_write(file_block, temp_struct->sector, temp_struct->data);
    	// printf("end_write");
    }

    temp_struct->is_dirty = 0;
    temp_struct->is_empty = 1;
    temp_struct->count = 0;
    // memset(temp_struct->data, 0, SECTORSIZE);

    return temp_struct;
}

void flusher(void){

	for(;;){
		// timer_sleep(FLUSHERFREQ);
		timer_sleep(500);
		lock_acquire(&buffer_cache_lock);
		// printf("FLUSHER!!!!!\n");
		struct list_elem * target_elem;
		struct cache_struct * target_struct;
		for (target_elem = list_begin(&buffer_cache); target_elem != list_end(&buffer_cache); target_elem = list_next(target_elem)){
			target_struct = list_entry(target_elem, struct cache_struct, cache_elem);
			if(target_struct->is_dirty = 1){
				// lock_acquire(&target_struct->cache_lock);
				block_write(file_block, target_struct->sector, target_struct->data);
				// lock_release(&target_struct->cache_lock);
				target_struct->is_dirty = 0;
			}
		}
		lock_release(&buffer_cache_lock);
	}
}


void cache_read_aheader(void * sector){
	// printf("start read aheader\n");
	struct cache_struct * target_cache = cache_get_free();
	// printf("read_aheader start??? %x\n",  (block_sector_t *)sector);
	// lock_acquire(&target_cache->cache_lock);
	// printf("read_aheader start %x\n",  (block_sector_t *)sector);
	block_read(file_block, (block_sector_t *)sector, target_cache->data);
	// lock_release(&target_cache->cache_lock);
	target_cache->sector = (block_sector_t *)sector;
	target_cache->is_empty = 0;
	is_read_aheader = 0;
	// printf("read aheader done\n");
	sema_up(&read_aheader_sema);
	thread_exit();

/*
	struct aux_struct * target_struct = (struct aux_struct *) aux;
	lock_acquire(&target_struct->free_cache->cache_lock);
	block_read(file_block, target_struct->sector, target_struct->free_cache->data);
	lock_release(&target_struct->free_cache->cache_lock);
	target_struct->free_cache->sector = target_struct->sector;
	target_struct->free_cache->is_empty = 0;
	thread_exit();
*/
}