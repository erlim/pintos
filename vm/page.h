#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <list.h>
#include <hash.h>
#include "threads/palloc.h"
#include "vm/frame.h"
#include "threads/thread.h"

void page_init(void);

//2.11 add ryoung swapping
struct page
{
  void *kaddr;
  struct vm_entry *vme;
  struct thread *thread;
  struct list_elem lru_elem;
};
struct page* page_alloc(enum palloc_flags flags);
void page_free(void *kaddr);

struct vm_entry
{
  int id;
  uint8_t type;
  void *vaddr;
  bool writable;
  bool bLoad;
  bool bPin;  //whether selected as victim 
  struct file *file;
  struct list_elem mmap_elem; //map_file
  size_t offset;
  size_t read_bytes;
  size_t zero_bytes;
  size_t swap_slot;
  struct hash_elem elem;
};
void vm_init(struct hash *vm);
void vm_destory(struct hash *vm);
struct vm_entry* vme_find(void*);
bool vme_insert(struct hash*, struct vm_entry*);
bool vme_remove(struct hash*, struct vm_entry*);

#endif  
