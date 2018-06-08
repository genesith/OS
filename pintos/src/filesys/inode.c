#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"
/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */


/*
  Return appropriate sector

*/


static block_sector_t byte_to_sector (const struct inode *inode, off_t pos)
{
    ASSERT (inode != NULL);
    ASSERT (pos > -1);
    block_sector_t result;
    pos = pos / BLOCK_SECTOR_SIZE;
    // printf("sector : %u, idx : %u %u %u %u\n", inode->sector, pos, inode->direct_idx, inode->indirect_idx, inode->doubly_indirect_idx);
    if (pos < 10){ 
        if (pos < inode->direct_idx)
            return inode->blocks[pos];
        else
            return -1;
    }
    else if (pos < 10 + INDIRECT_PTRS){
        pos = pos - 10;
        if (inode->direct_idx == 11 || (inode->direct_idx == 10 && inode ->indirect_idx > pos)){
            void * buf = malloc (INDIRECT_PTRS * sizeof(block_sector_t));
            // printf("cache_read buffer %x\n", buf);
            cache_read(inode->blocks[10], buf);
            result = ((block_sector_t *)buf)[pos];
            free(buf);
            return result;
        }
        else
            return -1;
    }
    else if (pos < 10 + INDIRECT_PTRS + INDIRECT_PTRS * INDIRECT_PTRS){
        if (inode -> direct_idx == 11){
            pos = (pos - 10 - INDIRECT_PTRS);
            off_t first_pos = pos/INDIRECT_PTRS;
            off_t second_pos = pos%INDIRECT_PTRS;
            if (first_pos < inode->indirect_idx || ((first_pos == inode->indirect_idx) && (second_pos < inode->doubly_indirect_idx))){
                void * buf = malloc (INDIRECT_PTRS * sizeof(block_sector_t));
                cache_read(inode->blocks[11], buf);
                block_sector_t doubly_pos = ((block_sector_t *) buf)[first_pos];
                cache_read(doubly_pos, buf);
                result = ((block_sector_t *)buf)[second_pos];
                free(buf);
                return result;
            }
            else
              return -1;
        }
        else return -1;
    }
    else
           PANIC("byte_to_sector got number too large %d", pos);
}


/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, int is_dir, struct dir * parent_dir)
{
  // printf("create %u %x\n", sector, is_dir);
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  // printf("size of disk_inode : %d\n", sizeof *disk_inode);
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {

      struct inode * temp_inode = (struct inode *) malloc(sizeof (struct inode));

      temp_inode->max_length = 0;
      temp_inode->direct_idx = 0;
      temp_inode->indirect_idx = 0;
      temp_inode->doubly_indirect_idx = 0;
      // temp_inode->blocks = (block_sector_t *) malloc(TOTAL_BLOCK_NUM * sizeof(block_sector_t));

      // printf("Before expand by create : current_length %d expand_size %d, %d, %d, %d\n", temp_inode->max_length, length, temp_inode->direct_idx, temp_inode->indirect_idx, temp_inode->doubly_indirect_idx);
    
      expand_block(temp_inode, length);
      // printf("After expand by create : current_length %d, %d, %d, %d\n", temp_inode->max_length, temp_inode->direct_idx, temp_inode->indirect_idx, temp_inode->doubly_indirect_idx);




      disk_inode->max_length = temp_inode->max_length;
      disk_inode->current_length = 0;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->direct_idx = temp_inode->direct_idx;
      disk_inode->indirect_idx = temp_inode->indirect_idx;
      disk_inode->doubly_indirect_idx = temp_inode->doubly_indirect_idx;

      disk_inode->is_dir = is_dir;
      // printf("disk inode is_dir : %d\n", disk_inode->is_dir);
      disk_inode->parent_dir = NULL;
      if (parent_dir){
        // printf("We have parent_dir \n");
        disk_inode->parent_dir = (struct dir *)malloc(sizeof(struct dir));
        memcpy(disk_inode->parent_dir, parent_dir, sizeof(struct dir));
      }

      // printf("disk_inode is_dir : %x\n", disk_inode->is_dir);
      memcpy(disk_inode->blocks, temp_inode->blocks, TOTAL_BLOCK_NUM * sizeof(block_sector_t));



      // disk_inode->blocks = malloc(TOTAL_BLOCK_NUM * sizeof(block_sector_t));


          // block_write (fs_device, sector, disk_inode);
      cache_write (sector, disk_inode);
      success = true; 
      free(temp_inode);
      free(disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{

  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;

  struct inode_disk * buf = (struct inode_disk *)malloc(sizeof(struct inode_disk));

  // block_read (fs_device, inode->sector, &inode->data);
  cache_read (inode->sector, buf);

  inode->direct_idx = buf->direct_idx;
  inode->indirect_idx = buf->indirect_idx;
  inode->doubly_indirect_idx = buf->doubly_indirect_idx;
  // printf("open_inode : %u %u %u %u\n", inode->sector, inode->direct_idx, inode->indirect_idx, inode->doubly_indirect_idx);
  inode->max_length = buf->max_length;
  inode->current_length = buf->current_length;
  // printf("buf is_dir : %d\n", buf->is_dir);
  inode->is_dir = buf->is_dir;

  // printf("opend ")
  inode->parent_dir = NULL;
  if (inode->is_dir == 1)
    if (buf->parent_dir){
      // printf("gg\n");
      inode->parent_dir = (struct dir *)malloc(sizeof(struct dir));
      memcpy(inode->parent_dir, buf->parent_dir, sizeof(struct dir));

  }

  memcpy(inode->blocks, buf->blocks, TOTAL_BLOCK_NUM * sizeof(block_sector_t));
  free(buf);

  // printf("open %u %x\n", inode->sector, inode->is_dir);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */

/*
  free_map_release, Delete all sectors individually by searching BLOCKS;
  Delete Indirect, Doubly indirect blocks, too;
*/

void inode_close_sectors(struct inode * inode){

  // printf("%x, %x, %x\n", inode->direct_idx, inode->indirect_idx, inode->doubly_indirect_idx);

  bool direct = 0; 

  if (inode->direct_idx == 11){

    block_sector_t indirect_blocks[INDIRECT_PTRS];
    block_sector_t doubly_indirect_blocks[INDIRECT_PTRS];

    if ((inode->indirect_idx != 0) || (inode->doubly_indirect_idx != 0)){
      
      cache_read(inode->blocks[inode->direct_idx], indirect_blocks);

      while (inode->indirect_idx >= 0){
        if (inode->doubly_indirect_idx != 0){
          cache_read(indirect_blocks[inode->indirect_idx], doubly_indirect_blocks);
          inode->doubly_indirect_idx -= 1;
        }

        while (inode->doubly_indirect_idx >= 0){
          free_map_release(doubly_indirect_blocks[inode->doubly_indirect_idx], 1);
          inode->doubly_indirect_idx -= 1;
        }

        inode->doubly_indirect_idx = INDIRECT_PTRS;
        inode->indirect_idx -= 1;
      }

      free_map_release(inode->blocks[inode->direct_idx], 1);
    }

    
    inode->direct_idx -= 1;
    inode->indirect_idx = INDIRECT_PTRS;
    direct = 1;
    // free(indirect_blocks);
    // free(doubly_indirect_blocks);
  }

  if (inode->direct_idx == 10){
    block_sector_t indirect_blocks[INDIRECT_PTRS];

    
    if (inode->indirect_idx != 0){  

      cache_read(inode->blocks[inode->direct_idx], indirect_blocks);
      inode->indirect_idx -= 1;
      
      while(inode->indirect_idx >= 0){
        free_map_release(indirect_blocks[inode->indirect_idx], 1);
        inode->indirect_idx -= 1;
      }

      free_map_release(inode->blocks[inode->direct_idx], 1);
    }

    inode->direct_idx -= 1;
    free(indirect_blocks);
    direct = 1;
  }

  if(direct == 0){
    inode->direct_idx -= 1;
  }

  while(inode->direct_idx >= 0){
    // printf("remove %xu\n", inode->direct_idx);
    free_map_release(inode->blocks[inode->direct_idx], 1);
    inode->direct_idx -= 1;
  }
  

}


void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  // printf("close\n");
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      // printf("Is last opener??\n");


      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          // printf("removed\n");
          inode_close_sectors(inode); 
          free_map_release (inode->sector, 1);
        }
      
      else{
        // printf("save it\n");
        // printf("close_inode : %u %u %u %u\n", inode->sector, inode->direct_idx, inode->indirect_idx, inode->doubly_indirect_idx);

        // Save in inode disk
        struct inode_disk * save_inode = (struct inode_disk *)malloc(BLOCK_SECTOR_SIZE);
        save_inode->direct_idx = inode->direct_idx;
        save_inode->indirect_idx = inode->indirect_idx;
        save_inode->doubly_indirect_idx = inode->doubly_indirect_idx;
        save_inode->max_length = inode->max_length;
        save_inode->current_length = inode->current_length;
        save_inode->is_dir = inode->is_dir;
        memcpy(save_inode->blocks, inode->blocks, TOTAL_BLOCK_NUM * sizeof(block_sector_t));
        save_inode->parent_dir = NULL;

        if(inode->parent_dir){
          save_inode->parent_dir = (struct dir *)malloc(sizeof(struct dir));
          memcpy(save_inode->parent_dir, inode->parent_dir, sizeof(struct dir));
          // printf("kk\n");
        }
        cache_write(inode->sector, save_inode);
        // free(save_inode->blocks);
        free (save_inode);

      }

      free (inode); 
      
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      // printf("read\n");
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      // printf("offset : %d %d\n", offset, sector_idx);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = (inode->max_length + 512 - 1) / 512 * 512 - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          // block_read (fs_device, sector_idx, buffer + bytes_read);
          // printf("read at sector_idx : %u\n", sector_idx);
          cache_read(sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          // block_read (fs_device, sector_idx, bounce);
          // printf("read at sector_idx : %u\n", sector_idx);
          cache_read(sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */

/*
  If byte_to_sector returns -1, expand blocks and write it
*/

off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  // printf("sector : %u, write size %u bytes in offset %u, current_size : %u\n", inode->sector, size, offset, inode->max_length);
  off_t init_offset = offset;
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if (byte_to_sector(inode, offset + size) == -1){
    // printf("Before expand : current_length %d expand_size %d, %d, %d, %d\n", inode->max_length, offset+size, inode->direct_idx, inode->indirect_idx, inode->doubly_indirect_idx);
    expand_block(inode, offset + size);
  }

  if (offset + size > inode->max_length)
    inode->max_length = offset + size;

  // printf("After expand : current_length %d, %d, %d, %d\n", inode->max_length, inode->direct_idx, inode->indirect_idx, inode->doubly_indirect_idx);


  while (size > 0) 
    {
      // printf("write");
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      // printf("offset : %u %u\n", offset, sector_idx);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = (inode->max_length + 512 - 1) / 512 * 512 - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      // printf("chunk_size : %u\n", chunk_size);
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          // block_write (fs_device, sector_idx, buffer + bytes_written);
          cache_write (sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left){
            // block_read (fs_device, sector_idx, bounce);
            cache_read (sector_idx, bounce);
          }

          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          // block_write (fs_device, sector_idx, bounce);
          cache_write (sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  // if (inode->current_length < init_offset + bytes_written)
  
  // inode->current_length += bytes_written;
  // printf("sector : %u, actually written %u, current_length : %u\n", inode->sector, bytes_written, inode->max_length);

  // printf("actually writen size is %u\n", bytes_written);
  return bytes_written;
}


void expand_block(struct inode * inode, off_t size){
  off_t remained_size = size - ((inode->max_length + BLOCK_SECTOR_SIZE - 1) / BLOCK_SECTOR_SIZE ) * BLOCK_SECTOR_SIZE;
  // printf("expand remained_size : %d\n", remained_size);
  // printf("Before expand : current_length %d expand_size %d, %d, %d, %d\n", inode->max_length, size, inode->direct_idx, inode->indirect_idx, inode->doubly_indirect_idx);
    // expand_block(inode, offset + size);
  // }



  static char zeros[BLOCK_SECTOR_SIZE];

/* 10 Direct blocks */

  while ((remained_size > 0) && (inode->direct_idx < DIRECT_BLOCK_NUM)){
    free_map_allocate(1, &inode->blocks[inode->direct_idx]);
    cache_write(inode->blocks[inode->direct_idx], zeros);
    inode->direct_idx += 1;
    remained_size -= BLOCK_SECTOR_SIZE; 
    // inode->max_length += BLOCK_SECTOR_SIZE; 
  }


/* 1 Indirect block */

  if ((remained_size > 0) && (inode->direct_idx < DIRECT_BLOCK_NUM + INDIRECT_BLOCK_NUM)){

    block_sector_t indirect_blocks[INDIRECT_PTRS];

    if(inode->indirect_idx == 0){
      free_map_allocate(1, &inode->blocks[inode->direct_idx]);
    }

    else{
      cache_read(inode->blocks[inode->direct_idx], indirect_blocks);
    }

    while ((remained_size > 0) && (inode->indirect_idx < INDIRECT_PTRS)){

      free_map_allocate(1, &indirect_blocks[inode->indirect_idx]);
      cache_write(indirect_blocks[inode->indirect_idx], zeros);

      inode->indirect_idx += 1;
      remained_size -= BLOCK_SECTOR_SIZE;
      // inode->max_length += BLOCK_SECTOR_SIZE;
    }

    cache_write(inode->blocks[inode->direct_idx], indirect_blocks);
    
    if (inode->indirect_idx == INDIRECT_PTRS){
      inode->direct_idx += 1;
      inode->indirect_idx = 0;
    }

    // free(indirect_blocks);

  
  }


/* 1 Doubly Indirect block */

  if ((remained_size > 0) && (inode->direct_idx < TOTAL_BLOCK_NUM)){
    block_sector_t indirect_blocks[INDIRECT_PTRS];
    block_sector_t doubly_indirect_blocks[INDIRECT_PTRS];

    if ((inode->indirect_idx == 0) && (inode->doubly_indirect_idx == 0)){
      free_map_allocate(1, &inode->blocks[inode->direct_idx]);
    }

    else{ 
      
      cache_read(inode->blocks[inode->direct_idx], indirect_blocks);

      if (inode->doubly_indirect_idx != 0)
        cache_read(indirect_blocks[inode->indirect_idx],  doubly_indirect_blocks);
    }

    while((remained_size > 0) && (inode->indirect_idx < INDIRECT_PTRS) && (inode->doubly_indirect_idx < INDIRECT_PTRS)){
      if(inode->doubly_indirect_idx == 0){
        free_map_allocate(1, &indirect_blocks[inode->indirect_idx]);
      }

      free_map_allocate(1, &doubly_indirect_blocks[inode->doubly_indirect_idx]);
      cache_write(doubly_indirect_blocks[inode->doubly_indirect_idx], zeros);
      
      remained_size -= BLOCK_SECTOR_SIZE;
      inode->doubly_indirect_idx += 1;

      if (inode->doubly_indirect_idx == INDIRECT_PTRS){

        cache_write(indirect_blocks[inode->indirect_idx], doubly_indirect_blocks);
        inode->indirect_idx += 1;
        inode->doubly_indirect_idx = 0;
        }

      }
      // printf("%u %u %u\n", inode->direct_idx, inode->indirect_idx, inode->doubly_indirect_idx);
      if (inode->doubly_indirect_idx != 0){
        // printf("%u\n", indirect_blocks[inode->indirect_idx]);
        cache_write(indirect_blocks[inode->indirect_idx], doubly_indirect_blocks);
      }

      // printf("%u\n", inode->blocks[inode->direct_idx]);

      cache_write(inode->blocks[inode->direct_idx], indirect_blocks);

      // free(indirect_blocks);
      // free(doubly_indirect_blocks);

    }

    
    // cache_write(first_blocks[inode->indirect_idx], second_blocks);

  inode->max_length = size;
  // free(zeros);
  // printf("After expand : current_length %d, %d, %d, %d\n", inode->max_length, inode->direct_idx, inode->indirect_idx, inode->doubly_indirect_idx);


  /* Make PANIC Condition*/

  

  if ((inode->direct_idx == TOTAL_BLOCK_NUM) && (inode->indirect_idx == INDIRECT_PTRS) && (inode->doubly_indirect_idx == INDIRECT_PTRS) && (remained_size > 0)){
    PANIC("We can not expand file anymore...");
  }

  return;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->max_length;
}
