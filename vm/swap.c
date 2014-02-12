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

struct lock lock_file;
//struct ata_disk *block_swap;
struct block *block_swap;
struct bitmap *bitmap_swap;

#define PG_SECTOR_CNT (PGSIZE/BLOCK_SECTOR_SIZE)

void
swap_init()
{
  lock_init(&lock_file);
  //select_device(block_swap);
  block_swap = block_get_role(BLOCK_SWAP); 
  block_sector_t bk_size = block_size(block_swap);
  size_t bit_cnt = block_size(block_swap) / PG_SECTOR_CNT; 
  bitmap_swap = bitmap_create(bit_cnt);
}

void
swap_in(size_t slot_idx, void *kaddr)
{
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
}


