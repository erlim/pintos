#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <list.h>
#include <hash.h>
#include <stdint.h>

//2.6 add ryoung virtual memory
struct vm_entry
{
  int id;
  uint8_t type;
  void *vaddr;
  bool writable;
  bool bLoad;
  bool bPin;  //?????????
  struct file* file;
  struct list_elem mmap_elem; //map_file
  size_t offset;
  size_t read_bytes;
  size_t zero_bytes;
  size_t swap_slot; //????????????
  struct hash_elem elem;
};
void vm_init(struct hash *vm);
void vm_destory(struct hash *vm);
struct vm_entry* find_vme(void*);
bool insert_vme(struct hash*, struct vm_entry*);
bool delete_vme(struct hash*, struct vm_entry*);

//2.8 add ryoung memory mapped file
struct mmap_file
{
  int id;
  struct file* file;
  struct list_elem elem;
  struct list vmes;
};
int  mmap(int fd, void *addr);
void munmap(int mapping);
void do_munmap(struct mmap_file *mmapf);

#endif  
