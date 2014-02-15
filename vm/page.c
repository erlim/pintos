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
  struct page *page = malloc(sizeof(struct page));
  uint8_t *kpage = palloc_get_page(flags);
  if(kpage == NULL)
    kpage = lru_try_victim_page(flags);
  page->kaddr = (uint8_t*)kpage;
  //page>vme = caller 
  page->thread = thread_current();
  lru_insert_page(page); 
  return page;
}

void
page_free(void *kaddr)
{
  palloc_free_page(kaddr);
  
  struct page *pg = lru_find_page(kaddr);

  struct thread *t= pg->thread;
  //pagedir_clear_page(t->pagedir, pg->vme->vaddr);
  //lru_get_next_clock(); //error!!! no check one elem 
  lru_remove_page(pg);
  free(pg);
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
