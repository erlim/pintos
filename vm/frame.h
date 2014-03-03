#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include <hash.h>
#include "vm/page.h"
#include "threads/palloc.h"

void frame_init(void);

<<<<<<< HEAD
void frame_insert_page(struct page* page);
void frame_remove_page(struct page* page);
void frame_destroy(struct thread* thread);

struct list_elem* frame_get_next_clock();
struct list_elem* frame_get_clock();

struct page* frame_get_next_page();
struct page* frame_get_page();

struct page* frame_find_page(void *kaddr);
void* frame_select_victim(enum palloc_flags flags);
=======
void lru_insert_page(struct page* page);
void lru_remove_page(struct page* page);
struct page* lru_find_page(void *kaddr);
struct list_elem* lru_get_next_clock(); 
void* lru_try_victim_page(int flags);
>>>>>>> addee61a7ad2e1fb60b8c1a4a44d7509ec5ef61d

#endif
