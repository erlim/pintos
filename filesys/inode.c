#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <stdio.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/buffer_cache.h"
#include "filesys/directory.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

//{ 2.19 add ryoung filesystem(extensible file)
#define INDIRECT_BLOCK_CNT (BLOCK_SECTOR_SIZE / sizeof (block_sector_t))
#define DIRECT_BLOCK_CNT 123
#define NOT_DEFINE 0 //sector idx to be define(not defined) 

enum direct_t
{
  DIRECT,
  INDIRECT,
  DOUBLE_INDIRECT,
  OUT_LIMIT
};

struct sector_location{
  enum direct_t type;
  int idx1;
  int idx2;
};

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
  block_sector_t direct_tbl[DIRECT_BLOCK_CNT];
  block_sector_t indirect1;
  block_sector_t indirect2;
  off_t length;                       /* File size in bytes. */
  unsigned magic;                     /* Magic number. */
  uint32_t bDir;
};

/* indirect inode. */
struct indirect_block
{
  block_sector_t tbl[INDIRECT_BLOCK_CNT];
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
  static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
{
  struct list_elem elem;              /* Element in inode list. */
  block_sector_t sector;              /* Sector number of disk location. */
  int open_cnt;                       /* Number of openers. */
  bool removed;                       /* True if deleted, false otherwise. */
  int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
  struct lock extend_lock;
};

static bool get_inode_disk(const struct inode* inode, struct inode_disk *inode_disk);
static void get_sector_location (off_t pos, struct sector_location *sec_loc);
static bool inode_disk_set_sector (struct inode_disk *inode_disk, block_sector_t new_sector, struct sector_location sec_loc);
static block_sector_t byte_to_sector (const struct inode_disk *inode_disk, off_t pos);
bool inode_disk_update_by_filesize(struct inode_disk *, off_t, off_t);
static void inode_disk_free_sector (struct inode_disk *inode_disk);

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
  static block_sector_t
byte_to_sector (const struct inode_disk *inode_disk, off_t pos)
{
  block_sector_t result_sec;
  ASSERT (inode_disk != NULL);
  if (pos < inode_disk->length)
  {
    struct indirect_block *ind_block;
    struct sector_location sec_loc;
    get_sector_location (pos, &sec_loc);
    switch (sec_loc.type)
    {
      case DIRECT:
        result_sec = inode_disk->direct_tbl[sec_loc.idx1];
        break;

      case INDIRECT:
        ASSERT (inode_disk->indirect1);
        ind_block = (struct indirect_block *) malloc (BLOCK_SECTOR_SIZE);
        if (ind_block)
        {
          bc_read(inode_disk->indirect1, ind_block, 0, BLOCK_SECTOR_SIZE, 0);
          result_sec = ind_block->tbl[sec_loc.idx1];
        }
        else
          result_sec = 0;
        free (ind_block);
        break;

      case DOUBLE_INDIRECT:
        ASSERT (inode_disk->indirect1);
        ind_block = (struct indirect_block *) malloc (BLOCK_SECTOR_SIZE);
        if (ind_block)
        {
          /* read in the first level indirect block */
          bc_read(inode_disk->indirect2, ind_block, 0, BLOCK_SECTOR_SIZE, 0);
          ASSERT (ind_block->tbl[sec_loc.idx1]);
          /* read in the second level indirect block */
          bc_read(ind_block->tbl[sec_loc.idx1], ind_block, 0, BLOCK_SECTOR_SIZE, 0);
          result_sec = ind_block->tbl[sec_loc.idx2];
        }
        else
          result_sec = 0;
        free (ind_block);
        break;

      default:
        result_sec = 0;
    }
  }
  else
    result_sec = 0;      // pos > inode length

  return result_sec;
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
inode_create (block_sector_t sector, off_t length, uint32_t is_dir)
{
  struct inode_disk *inode_disk = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *inode_disk == BLOCK_SECTOR_SIZE);

  inode_disk = calloc (1, sizeof *inode_disk);
  if (inode_disk != NULL)
  {
    inode_disk->length = length;
    inode_disk->magic = INODE_MAGIC;
    inode_disk->bDir = is_dir;

    if(length > 0)
      if(!inode_disk_update_by_filesize(inode_disk, 0, length-1))
        return false;

    bc_write(sector, inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
    free(inode_disk);
    success = true;
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

  lock_init(&inode->extend_lock);

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
  void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
  {
    /* Remove from inode list and release lock. */
    list_remove (&inode->elem);

    /* Deallocate blocks if removed. */
    if (inode->removed)
    {
      struct inode_disk *inode_disk = malloc (sizeof (struct inode_disk));
      if (inode_disk == NULL)
        PANIC ("Failed to close inode. Could not allocate inode_disk.\n");
      get_inode_disk (inode, inode_disk);
      inode_disk_free_sector (inode_disk);
      free_map_release (inode->sector, 1);
      free (inode_disk);
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

  struct inode_disk *inode_disk = malloc(sizeof(struct inode_disk));
  if(inode_disk == NULL)
    return 0;
  get_inode_disk(inode, inode_disk);

  while (size > 0)
  {
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_disk->length - offset;

    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually copy out of this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector (inode_disk, offset);
    if(sector_idx == 0){
      break;
    }

    bc_read(sector_idx, buffer, bytes_read, chunk_size, sector_ofs);

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_read += chunk_size;
  }

  free(inode_disk);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
  off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
    off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  struct inode_disk *inode_disk = malloc(sizeof(struct inode_disk));
  if(inode_disk == NULL)
    return 0;
  get_inode_disk(inode, inode_disk);

  if(inode->deny_write_cnt){
    free(inode_disk);
    return 0;
  }

  lock_acquire(&inode->extend_lock);
  int old_length = inode_disk->length;
  int write_end = offset +  size - 1;
  if(write_end > old_length -1 ){
    inode_disk->length = write_end + 1;
    if(!inode_disk_update_by_filesize(inode_disk, old_length, write_end)){
      free(inode_disk);
      return 0;
    }
  }
  lock_release(&inode->extend_lock);

  while (size > 0)
  {
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_disk->length - offset;

    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    /* Sector to write, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector (inode_disk, offset);
    if(sector_idx == 0)
      break;

    bc_write(sector_idx, (void*)buffer, bytes_written, chunk_size, sector_ofs);

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_written += chunk_size;
  }

  bc_write(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
  free(inode_disk);

  return bytes_written;
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
  struct inode_disk *inode_disk = malloc (sizeof (struct inode_disk));
  if (inode_disk == NULL)
    return 0;
  get_inode_disk (inode, inode_disk);
  off_t length = inode_disk->length;
  free (inode_disk);
  return length;
}

bool inode_disk_update_by_filesize(struct inode_disk* inode_disk, off_t start_pos, off_t end_pos)
{
  block_sector_t sector_idx;
  off_t offset = start_pos;
  int size = end_pos - start_pos + 1;
  uint8_t *zeroes = calloc(sizeof(uint8_t), BLOCK_SECTOR_SIZE);
  if(zeroes == NULL)
    return false;

  while(size > 0)
  {
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;
     int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int chunk_size = size < sector_left ? size : sector_left;
    if (chunk_size <= 0)
      break;
    if(sector_ofs > 0)
    {
      sector_idx = byte_to_sector(inode_disk, offset);
      bc_write(sector_idx, zeroes, 0, sector_left, sector_ofs);
    }
    else
    {
      if(free_map_allocate(1, &sector_idx))
      {
        struct sector_location set_loc;
        get_sector_location(offset, &set_loc);
        if(!inode_disk_set_sector(inode_disk, sector_idx, set_loc))
        {
          free_map_release(sector_idx, 1);
          free(zeroes);
          return false;
        }
      }
      bc_write(sector_idx, zeroes, 0, BLOCK_SECTOR_SIZE, 0);
    }
    size -= chunk_size;
    offset += chunk_size;
  }
  free(zeroes);

  return true;
}
static void 
inode_disk_free_sector(struct inode_disk* inode_disk)
{
  int i, j =0; 
  //double indirect block
  if(inode_disk->indirect2 > NOT_DEFINE)
  {
    struct indirect_block *ind_block1 = malloc(sizeof(struct indirect_block));
    struct indirect_block *ind_block2 = malloc(sizeof(struct indirect_block));
    bc_read(inode_disk->indirect2, ind_block1, 0, BLOCK_SECTOR_SIZE,  0);
    for(i=0; i<INDIRECT_BLOCK_CNT; ++i)
    {
      if(ind_block1->tbl[i] < NOT_DEFINE) 
        break;
      bc_read(ind_block1->tbl[i], ind_block2, 0, BLOCK_SECTOR_SIZE, 0);
      for(j=0; j<INDIRECT_BLOCK_CNT; ++j)
      {
        if(ind_block2->tbl[j] < NOT_DEFINE)
          break;
        free_map_release(ind_block2->tbl[j],1);
      }
      free_map_release(ind_block1->tbl[i],1);
    }
    free_map_release(inode_disk->indirect2,1);
    free(ind_block1);
    free(ind_block2);
  }    
  //indirect block
  if(inode_disk->indirect1 > NOT_DEFINE )
  {
    struct indirect_block *ind_block1 = malloc(sizeof(struct indirect_block));
    bc_read(inode_disk->indirect1, ind_block1, 0, BLOCK_SECTOR_SIZE, 0); 
    for(i=0; i<INDIRECT_BLOCK_CNT; ++i)
    {
      if(ind_block1->tbl[i] < NOT_DEFINE)
        break;
      free_map_release(ind_block1->tbl[i],1);
    }
    free_map_release(inode_disk->indirect1,1);
    free(ind_block1);
  }
  //direct block
  i = 0;
  while (inode_disk->direct_tbl[i] > NOT_DEFINE)
  {
    free_map_release (inode_disk->direct_tbl[i], 1);
    i++;
  }  
}

static bool
get_inode_disk(const struct inode *inode, struct inode_disk *inode_disk)
{
  bc_read(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
  return true;
}

static void
get_sector_location (off_t pos, struct sector_location *sec_loc)
{
  off_t pos_sector = pos / BLOCK_SECTOR_SIZE;
  if (pos_sector < DIRECT_BLOCK_CNT)
  {
    sec_loc->type = DIRECT;
    sec_loc->idx1 = pos_sector;
  }
  else if (pos_sector < (off_t) (DIRECT_BLOCK_CNT + INDIRECT_BLOCK_CNT))
  {
    sec_loc->type = INDIRECT;
    sec_loc->idx1 = pos_sector - DIRECT_BLOCK_CNT;
  }
  else if (pos_sector < (off_t) (DIRECT_BLOCK_CNT + INDIRECT_BLOCK_CNT * INDIRECT_BLOCK_CNT))
  {
    sec_loc->type = DOUBLE_INDIRECT;
    pos_sector = pos_sector - DIRECT_BLOCK_CNT - INDIRECT_BLOCK_CNT;
    sec_loc->idx1 = pos_sector / INDIRECT_BLOCK_CNT;
    sec_loc->idx2 = pos_sector % INDIRECT_BLOCK_CNT;

  }
  else
    sec_loc->type = OUT_LIMIT;
}

static bool
inode_disk_set_sector (struct inode_disk *inode_disk, block_sector_t new_sector, struct sector_location sec_loc)
{
  struct indirect_block *indirect_block  = NULL;
  switch (sec_loc.type)
  {
    case DIRECT:
      inode_disk->direct_tbl[sec_loc.idx1] = new_sector;
      break;
    case INDIRECT:
      indirect_block  = malloc(sizeof( struct indirect_block));
      if (inode_disk->indirect1 == NOT_DEFINE)
      {      
        if (free_map_allocate (1, &(inode_disk->indirect1)))
        {
          memset (indirect_block , 0, BLOCK_SECTOR_SIZE);
          indirect_block ->tbl[sec_loc.idx1] = new_sector;
          bc_write(inode_disk->indirect1, indirect_block , 0, BLOCK_SECTOR_SIZE, 0);
        }
      }
      else   
      {
        bc_write(inode_disk->indirect1, &new_sector, 0, sizeof(block_sector_t), 4 * (sec_loc.idx1));
      }
      break;
    case DOUBLE_INDIRECT:
      indirect_block  = malloc(sizeof(struct indirect_block));
      block_sector_t indirect2;
      if (inode_disk->indirect2 == NOT_DEFINE)
      {
        if (free_map_allocate (1, &(inode_disk->indirect2)))
        {
          memset (indirect_block , 0, BLOCK_SECTOR_SIZE);
          bc_write(inode_disk->indirect2, indirect_block , 0, BLOCK_SECTOR_SIZE, 0);
        }
      }
      bc_read(inode_disk->indirect2, &indirect2, 0, sizeof(block_sector_t), 4 * sec_loc.idx1);
      if (indirect2 == NOT_DEFINE)
      {
        if (free_map_allocate (1, &indirect2))
        { 
          bc_write(inode_disk->indirect2, &indirect2, 0, sizeof(block_sector_t), 4 * sec_loc.idx1);
          memset (indirect_block , 0, BLOCK_SECTOR_SIZE);
          indirect_block ->tbl[sec_loc.idx2] = new_sector;
          bc_write(indirect2, indirect_block , 0, BLOCK_SECTOR_SIZE, 0);
        }
      }
      else
      {  
        bc_write(indirect2, &new_sector, 0, sizeof(block_sector_t), 4 * sec_loc.idx2);
      }
      break;

    default:
      return false;
  }

  free (indirect_block );
  return true;
}

bool 
inode_is_dir(const struct inode *inode)
{
  struct inode_disk *inode_disk = malloc(sizeof(struct inode_disk));
  if(inode_disk == NULL)
    return 0;

  bc_read(inode->sector, (void *)inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
  bool bDir = inode_disk->bDir;
  free(inode_disk);

  return bDir;
}

bool inode_is_removed(const struct inode *inode)
{
  return inode->removed;
}

