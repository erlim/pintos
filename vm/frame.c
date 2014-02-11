#include "vm/frame.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include "userprog/process.h"
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

struct lock lock_file;
struct list lru_list;
struct list_elem *lru_clock; //clock_head

void frame_init()
{
  list_init(&lru_list);
  lru_clock = NULL;
}

void
lru_insert_page(struct page* page)
{
  list_push_back(&lru_list, page);
}
void
lru_remove_page(struct page* page)
{
  list_remove(page);
}
struct list_elem*
lru_get_next_clock()
{
  if(!list_empty(&lru_list))
    lru_clock = list_next(lru_clock);
  return lru_clock; 
  //if last , ->return NULL;
}

void*
lru_try_victim_page(int flags) //enum palloc_falgs flags
{
  struct list_elem *e = lru_get_next_clock();
  struct page *page = list_entry(e, struct page, lru_elem);
  page_free(page->kaddr);
  return page->kaddr; 
}

