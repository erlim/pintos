#ifndef VM_SWAP_H
#define VM_SAWP_H

#include <list.h>
#include <hash.h>
#include <bitmap.h>

void swap_init(void);
void swap_in(size_t user_idx, void *kaddr);
size_t swap_out(void* kaddr);

#endif  
