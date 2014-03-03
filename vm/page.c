#include "vm/page.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include "userprog/process.h"
#include "filesys/file.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

struct lock lock_file;

void
page_init()
{
  lock_init(&lock_file);
}

struct page* 
page_alloc(enum palloc_flags flags)
{
  uint8_t *kpage = palloc_get_page(flags);
  if(kpage == NULL)
    kpage = frame_select_victim(flags);
  
  struct page *page = malloc(sizeof(struct page));
<<<<<<< HEAD
  page->kaddr = (uint8_t*)kpage;
  //page>vme = caller 
  page->thread = thread_current();
  //printf("page->thread tid:%d\n", page->thread->tid);
  //frame_insert_page(page); 
=======
  page->kaddr = kpage;
  //page>vme = caller 
  page->thread = thread_current();
  lru_insert_page(page); 
>>>>>>> addee61a7ad2e1fb60b8c1a4a44d7509ec5ef61d
  return page;
}

void
page_free(void *kaddr)
{
<<<<<<< HEAD
  struct page *page = frame_find_page(kaddr);
  if(page)
=======
  struct page *pg = lru_find_page(kaddr);
  lru_remove_page(pg);

  struct thread *t = thread_current();
   
  //vm->type ==VM_FILE
  /*
  if(pagedir_is_dirty(t->pagedir, kaddr))
>>>>>>> addee61a7ad2e1fb60b8c1a4a44d7509ec5ef61d
  {
    frame_remove_page(page);
    palloc_free_page(kaddr);
    free(page);
  }
}


//{ vm utils(init,destory) 
static unsigned 
vm_hash_func(const struct hash_elem *e, void *aux UNUSED)
{
  const struct vm_entry *vme = hash_entry(e, struct vm_entry, elem);
  return hash_int(vme->vaddr);
}

static bool 
vm_less_func(const struct hash_elem *lhs, const struct hash_elem *rhs, void *aux UNUSED)
{
  const struct vm_entry *l = hash_entry(lhs, struct vm_entry, elem);
  const struct vm_entry *r = hash_entry(rhs, struct vm_entry, elem);

  return l->vaddr < r->vaddr;
}

static void 
vm_destroy_func(struct hash_elem *e, void *aux UNUSED)
{
  struct vm_entry *vme = hash_entry(e,struct vm_entry, elem);
  free(vme);
}

void
vm_init(struct hash *vm)
{
  if(!hash_init(vm,vm_hash_func, vm_less_func, NULL))
    sys_exit(-1);
}

void
vm_destroy(struct hash *vm)
{
  hash_destroy(vm, NULL);
}
//}

struct vm_entry*
vme_find(void *vaddr)
{
  struct vm_entry vme;
  vme.vaddr = pg_round_down(vaddr);
  struct hash_elem *e= hash_find(&thread_current()->vm, &vme.elem);
  if(!e)  
  {
    return NULL;
  }

 return hash_entry(e, struct vm_entry, elem);
}

bool
vme_insert(struct hash *vm, struct vm_entry *vme)
{
  struct hash_elem* e =  hash_insert(vm, &vme->elem);
  struct vm_entry *temp = hash_entry(e, struct vm_entry, elem);
  if(temp)
    return true;
  return false;
}

bool
vme_remove(struct hash *vm, struct vm_entry *vme)
{
  return hash_delete(vm, &vme->elem);
}
