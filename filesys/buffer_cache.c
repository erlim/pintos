#include "filesys/buffer_cache.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "devices/block.h"
#include "devices/ide.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "filesys/off_t.h"

#define BUFFER_CACHE_CNT 64
//#define BLOCK_SECTOR_SIZE 512 //already define in block.h

extern struct block *fs_device;  //from filesys.h
static void* buffer_cache;      //buffer cache's memory address
static struct buffer_head buffer_head[BUFFER_CACHE_CNT];
static int clock_hand;         //clock for select victim entry 

void
bc_init()
{
  buffer_cache = malloc(BLOCK_SECTOR_SIZE * BUFFER_CACHE_CNT);
  if(buffer_cache == NULL)
    return ;
  void *data = buffer_cache;
  int idx =0;
  for(; idx< BUFFER_CACHE_CNT; ++idx)
  {
    buffer_head[idx].dirty = false;
    buffer_head[idx].access = false;
    buffer_head[idx].sector = -1; //not define // sector idx is start from 0
    lock_init(&buffer_head[idx].lock);
    buffer_head[idx].data = data;    
    buffer_head[idx].clock = false ;
    data += BLOCK_SECTOR_SIZE;
  }
  clock_hand = 0;
}

void
bc_destroy()
{
  //printf("destroy");
  bc_flush();
  free(buffer_cache);
}

void 
bc_flush()
{
  //printf("bc_flush\n");
  int idx;
  for(idx = 0; idx < BUFFER_CACHE_CNT; ++idx)
  {
    if(buffer_head[idx].dirty == true && buffer_head[idx].access == true)
      bc_flush_entry(&buffer_head[idx]);
  }
}

void 
bc_flush_entry (struct buffer_head *bufe)
{
  //printf("bc_flush_entry\n");
  lock_acquire(&bufe->lock);
  block_write(fs_device, bufe->sector, bufe->data);
  lock_release(&bufe->lock);
  bufe->dirty = false;
}

bool 
bc_read (block_sector_t sector_idx, void* buffer, off_t bytes_read, int chunk_size, int sector_ofs)
{
  //printf("bc_read\n");
  struct buffer_head *bufe = bc_find(sector_idx);
  if(bufe == NULL) 
  {
    bufe = bc_select_victim();
    if(bufe == NULL)
      return false;
    lock_acquire(&bufe->lock);
    block_read(fs_device, sector_idx, bufe->data);
    lock_release(&bufe->lock);
    bufe->dirty = false;
    bufe->access = true;
    bufe->sector = sector_idx;
  }
  bufe->clock = true;
  memcpy(buffer+bytes_read, bufe->data+sector_ofs, chunk_size);
  
  return true;
}

bool 
bc_write (block_sector_t sector_idx, void* buffer, off_t bytes_written, int chunk_size, int sector_ofs)
{
  //printf("bc_write\n");
  struct buffer_head *bufe = bc_find(sector_idx);
  if(bufe == NULL) 
  {
    bufe = bc_select_victim();
    if (bufe == NULL)
      return false;
    lock_acquire(&bufe->lock);
    block_read(fs_device, sector_idx, bufe->data);
    lock_release(&bufe->lock);
  }
  bufe->dirty = true;
  bufe->access = true;
  bufe->sector = sector_idx;
  bufe->clock = true;
  memcpy(bufe->data+sector_ofs, buffer+bytes_written, chunk_size);
 
  return true;
}

struct buffer_head* 
bc_select_victim()
{
  //printf("select victim\n");
  while(true) 
  {
    if(clock_hand >= BUFFER_CACHE_CNT)  
      clock_hand = 0; //frist entry
    if(buffer_head[clock_hand].clock)
      buffer_head[clock_hand].clock = false;
    else
    {
      buffer_head[clock_hand].clock = true;
      if(buffer_head[clock_hand].dirty == true) 
        bc_flush_entry(&buffer_head[clock_hand]);       
      buffer_head[clock_hand].dirty = false;
      buffer_head[clock_hand].access = false; 
      buffer_head[clock_hand].sector = -1;
      return &buffer_head[clock_hand];
      break;
     }
    clock_hand += 1;
  }
}

struct buffer_head*
bc_find(block_sector_t sector)
{ 
  int idx;
  for(idx = 0; idx < BUFFER_CACHE_CNT; ++idx)
  {
    if(buffer_head[idx].sector == sector)
      return &buffer_head[idx];
  }
  return NULL;
}


