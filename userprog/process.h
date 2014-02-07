#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "vm/page.h"

tid_t process_execute (const char *file_name);
// 12.31 add Ryoung
void argument_stack(char **argv, int argc, void **esp);

int process_wait (pid_t);
void process_exit (void);
void process_activate (void);

// 1.3 add Ryoung, utils for process descriptor
struct thread* process_get_child(pid_t);
void process_remove_child(struct thread*);

// 1.10 add ryoung utils for file descriptor for each process
int process_add_file(struct file*);
struct file* process_get_file(int fd);
void process_close_file(int fd);

// 2.7 add ryoung page faule handlerr
bool handle_mm_fault(struct vm_entry *vme);
#endif /* userprog/process.h */
