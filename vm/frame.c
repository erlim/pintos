#include "vm/frame.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include "userprog/process.h"
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"

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
lru_try_victim_page(int flags) //enum palloc_falgs flags
{
  struct thread *t = thread_current();
  struct list_elem *e = lru_get_next_clock();
  struct page *page = list_entry(e, struct page, lru_elem);
  struct vm_entry *vme = page->vme;

  if(pagedir_is_accessed(t->pagedir, vme->vaddr))
  {
    pagedir_set_accessed(t->pagedir, vme->vaddr, false);
  }
  else
  {
    if(pagedir_is_dirty(t->pagedir, vme->vaddr) || vme->type == VM_SWAP)
    {
      if(vme->type == VM_FILE)
      {
        lock_acquire(&lock_file);
        file_write_at(vme->file, vme->vaddr, vme->read_bytes, vme->offset);
        lock_release(&lock_file);
      } 
      else if(vme->type == VM_SWAP)
      {
        vme->type = VM_SWAP;
        vme->swap_slot = swap_out(page->kaddr);
      }
    }
  }
  page_free(page->kaddr);
  return page->kaddr; 
}

