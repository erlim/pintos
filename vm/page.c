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

 //struct vm_entry *find = hash_entry(e, struct vm_entry, elem);
 //if(find)
 //find->bPin = false; //unpin_ptr()
 return hash_entry(e, struct vm_entry, elem);
}

//@todo. think inser_vme(type, vaddr,.......);
//thread_current()->vm?????
bool
insert_vme(struct hash *vm, struct vm_entry *vme)
{
  return hash_insert(vm, vme);
}

bool
delete_vme(struct hash *vm, struct vm_entry *vme)
{
  return hash_delete(vm, vme);
}
