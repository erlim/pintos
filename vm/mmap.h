#ifndef VM_MMAP_H
#define VM_MMAP_H

#include <list.h>

//2.8 add ryoung memory mapped file
struct mmap_file
{
  int id;
  bool bUnmap;
  struct file* file;
  struct list_elem elem;
  struct list vmes;
};

void mmap_init(void);
//void mmap_destroy(struct mmap_file *mmapf);
int  do_mmap(int fd, void *addr);
void do_munmap(int mapping);

#endif  
