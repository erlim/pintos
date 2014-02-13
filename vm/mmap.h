#ifndef VM_MMAP_H
#define VM_MMAP_H

#include <list.h>

//2.8 add ryoung memory mapped file
struct mmap_file
{
  int id;
  struct file* file;
  struct list_elem elem;
  struct list vmes;
};

void mmap_init(void);
int  do_mmap(int fd, void *addr);
void do_munmap(int mapping);

#endif  
