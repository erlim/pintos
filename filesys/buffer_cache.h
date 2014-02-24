#ifndef FILESYS_H
#define FILESYS_H

#include <stdbool.h>
#include "threads/synch.h"
#include "devices/block.h"
#include "filesys/off_t.h"
  

struct buffer_head
{
  //data refer file
  bool dirty;
  bool access;
  block_sector_t sector;
  void* data;            //buffer cache addr
  
  //meta
  bool clock;
  struct lock lock;   //lock each file for therir own file(inode, disk, block)
};

//init & destroy
void bc_init();
void bc_destroy();
void bc_flush();
void bc_flush_entry();

//api(insert,remove,, )
bool bc_read(block_sector_t sector_idx, void* buffer, off_t bytes_read, int chunk_size, int sector_ofs);
bool bc_write(block_sector_t sector_idx, void* buffer, off_t bytes_write, int chunk_size, int sector_ofs);
struct buffer_head* bc_select_victim();
struct buffer_head* bc_find(block_sector_t sector);

#endif
