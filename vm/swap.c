#include "vm/swap.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include "userprog/process.h"
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/block.h"
#include "devices/ide.h"

<<<<<<< HEAD
static struct lock lock_file;
static struct block *block_swap;
static struct bitmap *bitmap_swap;

#define BLOCK_SIZE        PGSIZE /*4096*/
#define SECTOR_SIZE       BLOCK_SECTOR_SIZE  /*512*/
#define BLOCK_SECTOR_CNT  (BLOCK_SIZE/SECTOR_SIZE)
=======
struct lock lock_file;
//struct ata_disk *block_swap;
struct block *block_swap;
struct bitmap *bitmap_swap;

#define PG_SECTOR_CNT (PGSIZE/BLOCK_SECTOR_SIZE)
>>>>>>> addee61a7ad2e1fb60b8c1a4a44d7509ec5ef61d

void
swap_init()
{
  lock_init(&lock_file);
<<<<<<< HEAD
  block_swap = block_get_role(BLOCK_SWAP); 
  size_t bit_cnt = block_size(block_swap) / BLOCK_SECTOR_CNT; 
=======
  //select_device(block_swap);
  block_swap = block_get_role(BLOCK_SWAP); 
  block_sector_t bk_size = block_size(block_swap);
  size_t bit_cnt = block_size(block_swap) / PG_SECTOR_CNT; 
>>>>>>> addee61a7ad2e1fb60b8c1a4a44d7509ec5ef61d
  bitmap_swap = bitmap_create(bit_cnt);
}

void
swap_in(size_t slot_idx, void *kaddr)
{
<<<<<<< HEAD
  /********************************************
  ex) slot_idx = 2
  sector: 16 ~ 23
  kaddr : kaddr ~ kaddr +(block_sector_size *8)
  *********************************************/
  block_sector_t sector = slot_idx * BLOCK_SECTOR_CNT;
  lock_acquire(&lock_file);
  int i =0; 
  for(; i< BLOCK_SECTOR_CNT; ++i)
  {
    sector += i; 
    block_read(block_swap, sector, kaddr);
    kaddr  += BLOCK_SECTOR_SIZE;
  }
  lock_release(&lock_file);
  bitmap_flip(bitmap_swap, slot_idx);
=======
  block_sector_t sector = slot_idx;
  int i =0; 
  for(; i< PG_SECTOR_CNT; ++i)
  {
    sector += i;
    block_read(block_swap, sector, kaddr);
    kaddr += PG_SECTOR_CNT;
  }
  bitmap_set_multiple(bitmap_swap, slot_idx, PG_SECTOR_CNT, true);
}

size_t
swap_out(void*kaddr)
{
  size_t slot_idx = bitmap_scan(bitmap_swap, 0, PG_SECTOR_CNT, false);
  block_sector_t sector = slot_idx;
  int i=0; i<PG_SECTOR_CNT;
  for(; i<PG_SECTOR_CNT; ++i)
  {
    sector += i;
    block_write(block_swap, sector, kaddr);
    kaddr+=BLOCK_SECTOR_SIZE;
  }
  bitmap_set_multiple(bitmap_swap, slot_idx, PG_SECTOR_CNT, true);
  
  return slot_idx;
}

swap_destroy(void)
{
  bitmap_destroy(bitmap_swap);
>>>>>>> addee61a7ad2e1fb60b8c1a4a44d7509ec5ef61d
}

size_t
swap_out(void*kaddr)
{
  /********************************************
  ex) slot_idx = 2
  sector: 16 ~ 23
  kaddr : kaddr ~ kaddr +(block_sector_size *8)
  *********************************************/
  size_t slot_idx = bitmap_scan_and_flip(bitmap_swap, 0, 1, false);
  block_sector_t sector = slot_idx * BLOCK_SECTOR_CNT;
  lock_acquire(&lock_file);
  int i=0;
  for(; i<BLOCK_SECTOR_CNT; ++i)
  {
    sector += i;
    block_write(block_swap, sector, kaddr);
    kaddr += BLOCK_SECTOR_SIZE;
  }
  lock_release(&lock_file);
  
  return slot_idx;
}

void
swap_destroy(void)
{
  bitmap_destroy(bitmap_swap);
}

