#ifndef THREADS_COMMON_H_USER_DEFINE
#define THREADS_COMMON_H_USER_DEFINE

#include "threads/thread.h"

/* +Ryoung sort, thread1)priority_schedling) */
static bool
high_priority_cmp(const struct list_elem *_lhs, const struct list_elem *_rhs, void* aux UNUSED)
{
  const struct thread *l = list_entry(_lhs, struct thread, elem);
  const struct thread *r = list_entry(_rhs, struct thread, elem);
   
  return l->priority >= r->priority;
}


static bool
less_ticks_cmp(const struct list_elem *_lhs, const struct list_elem *_rhs, void* aux UNUSED)
{
  const struct thread *l = list_entry(_lhs, struct thread, elem);
  const struct thread *r = list_entry(_rhs, struct thread, elem);
   
  return l->wakeup_ticks <= r->wakeup_ticks;
}

#endif
