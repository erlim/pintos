#include "vm/frame.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include "userprog/process.h"
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "vm/page.h"

static struct lock lock_file;
static struct list lru_list;
static struct list_elem *lru_clock; //clock_head

void frame_init()
{
  lock_init(&lock_file);
  list_init(&lru_list);
  lru_clock = NULL;
}

void
lru_insert_page(struct page* page)
{
  list_push_back(&lru_list, &page->lru_elem);
}
void
lru_remove_page(struct page* page)
{
  list_remove(&page->lru_elem);
}
struct list_elem*
lru_get_next_clock()
{
  if(!list_empty(&lru_list))
  { 
    if( (lru_clock == NULL) || (lru_clock == list_end(&lru_list)) ) 
      lru_clock = list_begin(&lru_list);
    else
      lru_clock = list_next(lru_clock);
  }
  return lru_clock; 
}


struct page*
lru_find_page(void *kaddr)
{
  struct list *list = &lru_list;
  struct list_elem *e;
  for( e= list_begin(list); e!= list_end(list); e= list_next(e))
  {
    struct page* pg = list_entry(e, struct page, lru_elem);
    if(pg->kaddr == kaddr)
      return pg;
  }
  return NULL;
}

void*
lru_try_victim_page(enum palloc_flags flags)
{
  printf("lru_try_victim_page\n");
  struct page *page = NULL;
  struct vm_entry *vme = NULL;
  struct thread *t; 
  bool bFind = false;
  while(!bFind)
  {
    page = list_entry( lru_get_next_clock(), struct page, lru_elem);
    //printf("%x\n", page->kaddr);
    vme = page->vme;
    t = page->thread;//thread_current(); //page->thread;
    //has been accessed recently,
    if(pagedir_is_accessed(t->pagedir, vme->vaddr))
    {
      pagedir_set_accessed(t->pagedir, vme->vaddr, false);
    }
    else //has not been accessed recently,
    {
      printf("type:%d\n",vme->type);
      if(pagedir_is_dirty(t->pagedir, vme->vaddr) || vme->type == VM_SWAP)
      {
        if(vme->type == VM_FILE) //file
        {
          lock_acquire(&lock_file);
          file_write_at(vme->file, vme->vaddr, vme->read_bytes, vme->offset);
          lock_release(&lock_file);
        } 
        if(vme->type == VM_SWAP || vme->type == VM_BIN)
        {
            vme->type = VM_SWAP;
            vme->swap_slot = swap_out(page->kaddr);
            printf("vme->swap_slot:%d\n", vme->swap_slot);
        }
      }
      bFind = true;
      //page_free(page->kaddr);
      //page->thread = thread_current();
      //pagedir_clear_page(t->pagedir, vme->vaddr);
      //page->thread = thread_current();
      //vme->bLoad = false; //unmemory no!!, just move to swap
      break;
    }
  }

  printf("kpage :%x, swap_slot:%d \n", page->kaddr, vme->swap_slot);
  return page->kaddr; 
}
