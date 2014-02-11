#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include <hash.h>
#include "vm/page.h"
void frame_init(void);

void lru_insert_page(struct page* page);
void lru_remove_page(struct page* page);
struct list_elem* lru_get_next_clock(); 
void* lru_try_victim_page(int flags);

#endif
