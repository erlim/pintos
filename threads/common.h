#ifndef THREADS_COMMON_H_USER_DEFINE
#define THREADS_COMMON_H_USER_DEFINE

#include "threads/thread.h"
#include "threads/synch.h"

//threads priority
static bool
high_priority_cmp(const struct list_elem *lhs, const struct list_elem *rhs, void* aux UNUSED)
{
  const struct thread *l = list_entry(lhs, struct thread, elem);
  const struct thread *r = list_entry(rhs, struct thread, elem);
  
  return l->priority > r->priority;
}

//semaphore's waiters priority
static bool
high_priority_sema_cmp(const struct list_elem *lhs, const struct list_elem *rhs, void* aud UNUSED)
{
  //left side
  const struct semaphore_elem *l = list_entry(lhs, const struct semaphore_elem, elem);
  const struct list_elem *l_elem = list_begin(&l->semaphore.waiters);
  const struct thread *l_t = list_entry(l_elem, const struct thread, elem);
  
  //right side
  const struct semaphore_elem *r = list_entry(rhs, const struct semaphore_elem, elem);
  const struct list_elem *r_elem = list_begin(&r->semaphore.waiters);
  const struct thread *r_t = list_entry(r_elem, const struct thread, elem);
  
  return l_t->priority > r_t->priority;
}

//ticks
static bool
less_ticks_cmp(const struct list_elem *lhs, const struct list_elem *rhs, void* aux UNUSED)
{
  const struct thread *l = list_entry(lhs, struct thread, elem);
  const struct thread *r = list_entry(rhs, struct thread, elem);
   
  return l->wakeup_ticks <= r->wakeup_ticks;
}

#endif
