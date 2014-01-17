#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

// 1.10 add ryoung file descriptor(denying write together)
struct lock lock_file;

#endif /* userprog/syscall.h */
