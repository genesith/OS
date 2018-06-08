#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include <list.h>

#define TOTAL_BLOCK_NUM 12
#define DIRECT_BLOCK_NUM 10
#define INDIRECT_BLOCK_NUM 1
#define DOUBLY_INDIRECT_BLOCK_NUM 1
#define INDIRECT_PTRS 128

struct bitmap;

struct inode_disk
  {

    block_sector_t blocks[TOTAL_BLOCK_NUM];
    int direct_idx;                     /* 0 ~ 11 */
    int indirect_idx;                   /* 0 ~ 127 */
    int doubly_indirect_idx;            /* 0 ~ 127 */
    int is_dir;

    struct dir * parent_dir;

    off_t current_length;
    off_t max_length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[108];               /* Not used. */
    
  };


struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    int is_dir;

    struct dir * parent_dir;


    off_t max_length;
    off_t current_length;
    block_sector_t blocks[TOTAL_BLOCK_NUM];
    int direct_idx;                     /* 0 ~ 11 */
    int indirect_idx;                   /* 0 ~ 127 */
    int doubly_indirect_idx;            /* 0 ~ 127 */
  

  };




// struct inode;

void inode_init (void);
bool inode_create (block_sector_t, off_t, int);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);
void expand_block(struct inode * inode, off_t size);


#endif /* filesys/inode.h */
