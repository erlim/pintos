#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/buffer_cache.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

//{ 2.19 add ryoung filesystem(extensible file)
#define INDIRECT_BLOCK_CNT BLOCK_SECTOR_SIZE / sizeof(block_sector_t)
/*
struct inode_disk's size must be 512 bytes, 
struct inode_dist
{
  off_t length                    //4bytes
  unsigned magic                  //2bytes
  block_sector_t indirect         //4bytes
  block_sector_t double_indirect  //4bytes
  block_sector_t direct[123]      //4bytes *123  , total 512kb
*/
#define DIRECT_BLOCK_CNT 123

enum direct_t
{
  NORMAL_DIRECT =0, 
  INDIRECT,
  DOUBLE_INDIRECT,
  OUT_LIMIT
};

struct sector_location
{
   enum direct_t directness;
   off_t index1;   
   off_t index2;  
};

struct inode_indirect_block
{
  block_sector_t map_table[INDIRECT_BLOCK_CNT];
};
//}  
  
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk // 512 Bytes
  {
    //will be remove
    block_sector_t start;               /* First data sector. */  
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */       
    //will be remove               
    uint32_t unused[125];               /* Not used. */     
    //unsigned int 32_t ->block_sector_t
    /*
    block_sector_t direct_map_table[DIRECT_BLOCK_CNT];             
    block_sector_t indirect_block_sec;                            
    block_sector_t double_indirect_block_sec;                    
    */ 
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
    //will be remove
    struct inode_disk data;             /* Inode content. */
    struct lock extend_lock;            /* semaphore lock */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  else
    return -1;
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
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true; 
        } 
      free (disk_inode);
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
  block_read (fs_device, inode->sector, &inode->data);
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
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
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
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          bc_read(sector_idx, (void*)buffer, bytes_read, chunk_size, sector_ofs);
          //2.19 ryoung modify
          //block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          //Read sector into bounce buffer, then partially copy
          //into caller's buffer.
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          //bc_read(sector_idx, (void*)buffer, bytes_read, chunk_size, sector_ofs);
          block_read (fs_device, sector_idx, bounce);
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
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          //2.19 modify ryoung
          bc_write(sector_idx, (void*)buffer, bytes_written, chunk_size, sector_ofs);
          //block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          // We need a bounce buffer.
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          // If the sector contains data before or after the chunk
          //   we're writing, then we need to read in the sector
          //   first.  Otherwise we start with a sector of all zeros.
          if (sector_ofs > 0 || chunk_size < sector_left) 
            bc_read(sector_idx, (void*)buffer, bytes_written, chunk_size, sector_ofs);//block_read (fs_device, sector_idx, bounce);
          else
            memset(buffer, 0 ,BLOCK_SECTOR_SIZE);//memset (bounce, 0, BLOCK_SECTOR_SIZE);
          //bc_write(sector_idx, (void*)buffer, bytes_written, chunk_size, sector_ofs);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

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
  return inode->data.length;
}


//2.19 add ryoung
/*
static bool get_disk_inode(const struct inode *inode, struct inode_disk *inode_disk)
{
  bc_read(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
  return true;
}

static void locate_byte(off_t pos, struct sector_locaion *sec_loc)
{
  off_t pos_sector = pos / BLOCK_SECTOR_SIZE;
  if(pos_sector < DIRECT_BLOCK_CNT)
  {
    sec_loc->directness = NORMAL_DIRECT;
    sec_loc->index1 = pos_sector;
  }
  else if(pos_sector< (off_t)DIRECT_BLOCK_CNT + INDIRECT_BLOCK_CNT)
  {
    sec_loc->directness = INDIRECCT;
    sec_loc->index1 = pos_sector - DIRECT_BLOCK_CNT;
  }
  else if(pos_sector < (off_t)(DIRECT_BLOCK_CNT + INDIRECT_BLOCK_CNT * (INDIRECT_BLOCK_CNT +1)))
  {
    sec_loc->directness = DOUBLE_INDIRECT;
    pos_sector = pos_sector - DIRECT_BLOCK_CNT - INDIRECT_BLOCK_CNT;
    sec_loc->index1 = pos_sector / INDIRECT_BLOCK_CNT;
    sec_loc->index2 = pos_sector % INDIRECT_BLOCK_CNT; /
  }
  else
  {
    sec_loc->directness = OUT_LIMIT;
  }
}

static inline off_t map_table_offset(int idx)
{
  return idx * sizeof(off_t);
}

static bool register_sector(struct inode_disk *inode_disk, block_sector_t new_sector, struct sector_locaion sec_loc)
{
  struct inode_indirect_block *new_block = NULL;
  switch(sec_loc.directness)
  {
    case NORMAL_DIRECT :  
      inode_disk->direct_map_table[sec_loc.index1] = new_sector;
      break;
    case INDIRECT:
      new_block= malloc(BLOCK_SECTOR_SIZE);
      if (new_block== NULL)
        return false;

      //save new allocated block number to index block
      //write index block to buffer cache     
      if(inode_disk->indirect_block_sec == 0)
      {
        if (free_map_allocate (1, &(inode_disk->indirect_block_sec)))
        {
          memset (new_block, 0, BLOCK_SECTOR_SIZE);
          new_block->map_table[sec_loc.index1] = new_sector;
          bc_write(inode_disk->indirect_block_sec, new_block, 0, BLOCK_SECTOR_SIZE, 0);
        }
        else
        {
          free (new_block);
          return false;
        }
      }
      else    // indirect block sector already exists
      {
        bc_write(inode_disk->indirect_block_sec, &new_sector, 0, sizeof(block_sector_t), map_table_offset(sec_loc.index1));
      }
      break;
    case DOUBLE_INDIRECT:
      new_block = malloc(BLOCK_SECTOR_SIZE);
      if (new_block== NULL)
        return false;

      if(inode_disk->double_indirect_block_sec == 0)
      {
        if (free_map_allocate (1, &(inode_disk-> double_indirect_block_sec)))
        {
          memset (new_block, 0, BLOCK_SECTOR_SIZE);
          bc_write(inode_disk->double_indirect_block_sec, new_block, 0, BLOCK_SECTOR_SIZE, 0);
        }
        else
        {
          free (new_block);
          returnfalse;
        }
      }
      // Read in ind_block_2nd_level
      bc_read(inode_disk->double_indirect_block_sec, &ind_block_2nd_level, 0, sizeof(block_sector_t),  map_table_offset(sec_loc.index1));

      // 2nd level indirect block does not exist
      if(ind_block_2nd_level == 0)
      {
        if(free_map_allocate (1, &ind_block_2nd_level))
        {
          // Update 1st level
          bc_write(inode_disk->double_indirect_block_sec,  &ind_block_2nd_level, 0,  sizeof(block_sector_t), map_table_offset(sec_loc.index1));
          memset(new_block, 0, BLOCK_SECTOR_SIZE);
          new_block->map_table[sec_loc.index2] = new_sector;
          bc_write(ind_block_2nd_level, new_block, 0, BLOCK_SECTOR_SIZE, 0);
        }
        else
        {
          free (new_block);
          returnfalse;
        }
      }
      else
      {
        // Write new entry into 2nd level block.
        bc_write(ind_block_2nd_level, &new_sector, 0, sizeof(block_sector_t), map_table_offset(sec_loc.index2));
      }
      break;
    default:
      return false;
  }
  free(new_block);
  return true;
}
*/
