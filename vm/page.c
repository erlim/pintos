#include "vm/page.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/process.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

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
  {
    printf("failed to vm init\n");
    thread_exit();
  }
}

void
vm_destroy(struct hash *vm)
{
  hash_destroy(vm, NULL);
}
//}

struct vm_entry*
find_vme(void *vaddr)
{
  struct vm_entry vme;
  vme.vaddr = pg_round_down(vaddr);
  struct hash_elem *e= hash_find(&thread_current()->vm, &vme.elem);
  if(!e)  
    return NULL;

 return hash_entry(e, struct vm_entry, elem);
}

bool
insert_vme(struct hash *vm, struct vm_entry *vme)
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

/*
bool
create_vme(uint8_t type, void* vaddr)
{
}
*/

bool
delete_vme(struct hash *vm, struct vm_entry *vme)
{
  return hash_delete(vm, &vme->elem);
}

int 
mmap(int fd, void *addr)
{
  check_address(addr);
  
  struct file* file = progess_get_file(fd);
  if(file !=  NULL)
  {
    file = reopen(file);
    struct mmap_file *mmapf = malloc(sizeof(struct mmap_file));
    mmapf->id = fd; //? 
    mmapf->file = file;
    list_init(&mmapf->vmes);
    struct vm_entry *vme = find_vme(addr);
    while(addr/*read_bytes*/ > (void*)0)
    {
      struct vm_entry *vme;
      list_puch_back(&mmapf->vmes, &vme->mmap_elem);
      //insert_vme(&thread_current()->vm, &vme->elem);  //?
    }
    list_push_back(&thread_current()->mmap_files, &mmapf->elem);
  
    return mmapf->id;
  }
  return 0;
}
void
munmap(int mapping)
{
  struct thread* t = thread_current();
  struct list *list = &t->mmap_files;
  struct list_elem *next, *e= list_begin(list);
  while(e != list_end(list))
  {
    next = list_next(e);
    struct mmap_file* mmapf = list_entry(e, struct mmap_file, elem);
    if(mmapf->id == mapping)
    {
      //{ munmap map_files' vme_entry
      do_munmap(mmapf);
      //}
      list_remove(&mmapf->elem); 
    }
    e = next;
  }
}
void
do_munmap(struct mmap_file *mmapf)
{
  struct lock lock_file;
  lock_init(&lock_file);

  struct thread* t = thread_current();
  struct list* list = &mmapf->vmes;
  struct list_elem *next, *e = list_begin(list);
  while(e != list_end(list))
  {
    next = list_next(e);
    struct vm_entry *vme = list_entry(e, struct vm_entry, mmap_elem);
    if(vme->bLoad)
    {
      if(pagedir_is_dirty(t->pagedir, vme->vaddr))
      {
        lock_acquire(&lock_file);
        file_write_at(vme->file, vme->vaddr, vme->read_bytes, vme->offset);
        lock_release(&lock_file);
      }
      palloc_free_page(pagedir_get_page(t->pagedir, vme->vaddr));
      pagedir_clear_page(t->pagedir, vme->vaddr);
    }
    list_remove(&vme->mmap_elem);
    delete_vme(&t->vm, &vme->elem);
    e = next;
  }
}
