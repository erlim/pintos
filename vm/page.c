#include "vm/page.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include "userprog/process.h"
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

struct lock lock_file;

void
page_init()
{
  lock_init(&lock_file);
}

struct page* 
page_alloc(int flags)//enum palloc_flags flags)
{
  uint8_t *kpage = palloc_get_page( flags);
  if(kpage == NULL)
  {
    kpage = lru_try_victim_page(flags);
  }
  struct page *page = malloc(sizeof(struct page));
  page->kaddr = kpage;
  //page>vme = caller 
  page->thread = thread_current();
  lru_insert_page(page); 
  return page;
}

void
page_free(void *kaddr)
{
  struct page *pg = lru_find_page(kaddr);
  lru_remove_page(pg);

  struct thread *t = thread_current();
   
  //vm->type ==VM_FILE
  /*
  if(pagedir_is_dirty(t->pagedir, kaddr))
  {
    lock_acquire(&lock_file);
    file_write_at(vme->file, vme->vaddr, vme->read_bytes, vme->offset);
    lock_release(&lock_file);
  }
  */
  palloc_free_page(pagedir_get_page(t->pagedir, kaddr));
  pagedir_clear_page(t->pagedir, kaddr);
  
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
  bool sucess;
  if(vm->bInit)
  { 
    sucess = hash_insert(vm, &vme->elem);
  }
  else
  {
    vm_init(vm);
    vm->bInit = true;
    sucess = hash_insert(vm, &vme->elem);
  }
  return sucess;
}

bool
vme_remove(struct hash *vm, struct vm_entry *vme)
{
  return hash_delete(vm, &vme->elem);
}
