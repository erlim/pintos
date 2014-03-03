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

static int bytes;
void frame_init()
{
  lock_init(&lock_file);
  list_init(&lru_list);
  lru_clock = NULL;
}

void
frame_insert_page(struct page* page)
{
  bytes += 4096;

  //printf("frame_insert_page\n");
  //if(page->thread->tid == 3)
  //if(page->vme->id == 548)
   // printf("%d, ", page->vme->id); 
  //lock_acquire(&lock_file);
  list_push_back(&lru_list, &page->lru_elem);
  //lock_release(&lock_file);
  
  //struct list_elem *temp = list_begin(&lru_list);
  //struct page *temp_p = list_entry(temp, struct page,lru_elem);
  //printf("after page->thread tid:%d\n", temp_p->thread->tid);
}
void
frame_remove_page(struct page* page)
{
  bytes -= 4096;
  lru_clock = frame_get_next_clock();
  list_remove(&page->lru_elem);
  //printf("temp->thread tid: %d\n", temp->thread->tid);
}

void
frame_destroy(struct thread* thread)
{
  if(!list_empty(&lru_list))
  {
    struct list *list = &lru_list;
    struct list_elem *next, *e = list_begin(list);
    while(e != list_end(list))
    {
      next = list_next(e);
      struct page* page = list_entry(e, struct page, lru_elem);
      if(page->thread == thread)
        frame_remove_page(page);
      e = next;
    }
  }
}
 
struct list_elem*
frame_get_next_clock()
{
  //struct page *temp = list_entry(list_end(&lru_list), struct page, lru_elem);
  //printf("end %d", temp->vme->id);
  
  if(!list_empty(&lru_list))
  { 
    //printf("size %d ", list_size(&lru_list));

    if(lru_clock == NULL)
    {
      lru_clock = list_begin(&lru_list);
    }    
    else
    { 
      if(lru_clock != list_end(&lru_list))
      {
        lru_clock = list_next(lru_clock);
      }
      if(lru_clock == list_end(&lru_list))
      {
        lru_clock = list_begin(&lru_list);
      }
      return lru_clock;
    }
  }
  
  return NULL;
}

<<<<<<< HEAD
struct list_elem*
frame_get_clock()
{
  //first time
  if(lru_clock == NULL)
  {
    lru_clock = list_begin(&lru_list);
  }
  return lru_clock;
}

struct page*
frame_get_next_page()
{
  struct page *page = list_entry(frame_get_next_clock(), struct page, lru_elem);
  if(page == NULL)
    return NULL;

  return page;
}

struct page*
frame_get_page()
{
  /*
  struct list_elem *temp = frame_get_clock();
  struct page *page = list_entry(temp, struct page, lru_elem);
  printf("page tid:%d\n", page->thread->tid);
  //printf("ddd %d, %x, %d\n", page->vme->id, page->kaddr, page->thread->tid);
  */
  
  struct page *page = list_entry(frame_get_clock(), struct page, lru_elem);
  if(page == NULL)
    return NULL;
  
  return page;
}

struct page*
frame_find_page(void *kaddr)
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
=======

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
>>>>>>> addee61a7ad2e1fb60b8c1a4a44d7509ec5ef61d
}

void*
frame_select_victim(enum palloc_flags flags)
{
 // printf("total bytes :%d\n", bytes);
  //printf("frame_select_victim\n");
  struct page *page = frame_get_page();
  bool bFind = false;
  while(!bFind)
  {
    struct vm_entry* vme = page->vme;
    struct thread* t = page->thread;
    //printf("t->tid:%d ", t->tid);
  
    //has been accessed recently,
    if(!vme->bPin)
    {
      //printf(" is_access %d\n", vme->id);
      if(pagedir_is_accessed(t->pagedir, vme->vaddr))
      {
        pagedir_set_accessed(t->pagedir, vme->vaddr, false);
      }
      else //has not been accessed recently,
      {
        bFind = true;
        vme->bLoad = false;
        vme->bPin = true;
        //printf("type:%d\n",vme->type);
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
              //printf("vme->swap_slot:%d\n", vme->swap_slot);
          }
       }
        /*
        frame_remove, 
        pagedir_clear_page(pagedir, vaddr);
        palloc_free_page(page->kaddr), 
        free(page);
        */
        //
        
        /*
        frame_remove_page(page->kaddr);
        pagedir_clear_page(t->pagedir, vme->vaddr);
        palloc_free_page(page->kaddr);
        free(page->kaddr);
        */
        //printf("find thread tid:%d\n", t->tid);

        page_free(page->kaddr);
        pagedir_clear_page(t->pagedir, vme->vaddr);
        //page_free(page->kaddr);
        return palloc_get_page(flags);  //find, return new kpage
        break;
      }
    }
    //printf("next page ");
    page = frame_get_next_page();
    //printf("vme %d", page->vme->id);
  }

  //printf("kpage :%x, swap_slot:%d \n", page->kaddr, vme->swap_slot);
}
