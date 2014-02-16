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

static struct lock lock_file;
static struct block *block_swap;
static struct bitmap *bitmap_swap;

#define BLOCK_SIZE        PGSIZE /*4096*/
#define SECTOR_SIZE       BLOCK_SECTOR_SIZE  /*512*/
#define BLOCK_SECTOR_CNT  (BLOCK_SIZE/SECTOR_SIZE)

void
swap_init()
{
  lock_init(&lock_file);
  block_swap = block_get_role(BLOCK_SWAP); 
  size_t bit_cnt = block_size(block_swap) / BLOCK_SECTOR_CNT; 
  bitmap_swap = bitmap_create(bit_cnt);
}

void
swap_in(size_t slot_idx, void *kaddr)
{
  //printf("swap_in, %x %d\n",kaddr, slot_idx);
  block_sector_t sector = slot_idx;

  lock_acquire(&lock_file);
  int i =0; 
  for(; i< BLOCK_SECTOR_CNT; ++i)
  {
    //block_read(block_swap, (slot_idx * BLOCK_SECTOR_CNT)+i, kaddr +(BLOCK_SECTOR_SIZE* i));    
    sector += i;
    block_read(block_swap, sector, kaddr);
    kaddr += BLOCK_SECTOR_CNT;
  }
  lock_release(&lock_file);

  bitmap_flip(bitmap_swap, slot_idx);
  //bitmap_set_multiple(bitmap_swap, slot_idx, 1, true);
}

size_t
swap_out(void*kaddr)
{
  size_t slot_idx = bitmap_scan_and_flip(bitmap_swap, 0, 1, false);
  //printf("swap_out, %x %d\n", kaddr, slot_idx);

  block_sector_t sector = slot_idx;
  
  lock_acquire(&lock_file);
  int i=0;
  for(; i<BLOCK_SECTOR_CNT; ++i)
  {
    //block_write(block_swap, (slot_idx * BLOCK_SECTOR_CNT)+i, kaddr +(BLOCK_SECTOR_SIZE* i));    
    sector += i;
    block_write(block_swap, sector, kaddr);
    kaddr += BLOCK_SECTOR_SIZE;
  }
  lock_release(&lock_file);
  
  //bitmap_set_multiple(bitmap_swap, slot_idx, PG_SECTOR_CNT, true);
  
  return slot_idx;
}

swap_destroy(void)
{
  bitmap_destroy(bitmap_swap);
}

