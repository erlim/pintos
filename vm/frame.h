#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include <hash.h>
#include "vm/page.h"
#include "threads/palloc.h"

void frame_init(void);

void frame_insert_page(struct page* page);
void frame_remove_page(struct page* page);
void frame_destroy(struct thread* thread);

struct list_elem* frame_get_next_clock();
struct list_elem* frame_get_clock();

struct page* frame_get_next_page();
struct page* frame_get_page();

struct page* frame_find_page(void *kaddr);
struct list_elem* frame_get_next_clock(); 
void* frame_select_victim(enum palloc_flags flags);

#endif
