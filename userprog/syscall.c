#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);

void check_address(void* addr);
void get_argument(void* esp, int* arg, int count);
void halt(void);
void exit(int status);
int  exec(const char *file);
int  wait(int);
bool create(const char* file, unsigned initial_size);
bool remove(const char* file);
int  open(const char *file);
int  filesize (int fd);
int  read(int fd, void *buffer, unsigned length);
int  write(int fd, const void *buffer, unsigned length);
void seek(int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);


  void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&lock_file);
}

  static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //hex_dump(f->esp, f->esp, f->esp, true);
  int arg[3]; //max count:3, ex) arg0, arg1, arg3    
  int sys_num = *(int*)f->esp;
  //printf("sys_num:%d\n", sys_num);

  switch(sys_num)
  {
    case SYS_HALT :
      halt();                                         break;
    case SYS_EXIT :  
      get_argument(f->esp, &arg,1); 
      exit(arg[0]);                                   break;
    case SYS_EXEC :
      get_argument(f->esp, &arg,1);
      f->eax = exec((const char*)arg[0]);             break;
    case SYS_WAIT :
      get_argument(f->esp, &arg,1);
      f->eax = wait(arg[0]);                          break;
    case SYS_CREATE : 
      get_argument(f->esp, &arg,2);
      f->eax = create((const char*)arg[0], (unsigned)arg[1]);           break;
    case SYS_REMOVE :
      get_argument(f->esp, &arg,1);        
      f->eax = remove((const char*)arg[0]);           break;
    case SYS_OPEN :
      get_argument(f->esp, &arg,1);
      f->eax = open((const char*)arg[0]);             break;
    case SYS_FILESIZE :
      get_argument(f->esp, &arg,1);
      f->eax = filesize(arg[0]);                      break;
    case SYS_READ : 
      get_argument(f->esp, &arg,3);
      f->eax = read(arg[0], (void*)arg[1], (unsigned)arg[2]);           break;
    case SYS_WRITE :
      get_argument(f->esp, &arg,3);
      f->eax = write(arg[0], (const void*)arg[1], (unsigned)arg[2]);    break; 
    case SYS_SEEK :
      get_argument(f->esp, &arg,2);
      seek(arg[0], (unsigned)arg[1]);                                   break;
    case SYS_TELL :  
      get_argument(f->esp, &arg,1);
      f->eax = tell(arg[0]);                          break;
    case SYS_CLOSE :
      get_argument(f->esp, &arg,1);
      close(arg[0]);                                  break;
    default :
      printf("failed to syscall, cuz of invailed syscall\n");
      thread_exit(); 
      break;
  }
}

void check_address(void *addr)
{
  if(! (addr>=(void*)0x8048000 && addr<(void*)0xc0000000) )
  {
    exit(-1);
  }
}   

void get_argument(void *esp, int *arg, int argc)
{
  int idx=0, num=1;
  for(; num<=argc; num++,idx++)
  {
    int *pval = (int*)esp +num;
    check_address(pval);
    arg[idx] = *pval;
  }   
}

void halt(void)
{
  shutdown_power_off();
}

void exit(int status)
{
  struct thread *t = thread_current();
  t->exit= true;
  t->status_exit = status;
  printf("%s: exit(%d)\n", t->name, status);
  thread_exit();
}

pid_t exec(const char *file)
{
  //printf("syscall exec\n");
  pid_t pid = process_execute(file);
  struct thread *child = process_get_child(pid);
  if(!child)
    return -1;

  if(child->status_load == false)
  {
    sema_down(&child->sema_proc); //thread_current() ->parent push waiters
  }

  if(child->status_load == -1)
  {
    return -1;
  }

  return pid;
}

int wait(pid_t pid)
{
//  printf("syscall wait:%d\n", pid);
  return process_wait(pid);
}


bool create(const char* file, unsigned initial_size)
{
  if(!file) 
    exit(-1);

  return  filesys_create(file, initial_size);
}

bool remove(const char* file)
{
  if(!file)
    exit(-1); 

  return  filesys_remove(file);
}

int open(const char *file)
{
  if(!file)
    return false;

  lock_acquire(&lock_file);
  struct file  *f = filesys_open(file);
  if(f == NULL)
  {
    lock_release(&lock_file);
    return -1;
  }
  int fd = process_add_file(f);
  lock_release(&lock_file);

  return fd;
}  

int filesize(int fd)
{
  struct file *f = process_get_file(fd);
  if( !f) 
    return -1;

  return file_length(f);  
}

int read(int fd, void *buffer, unsigned length)
{
  check_address(buffer);
  if( 0 == fd)
  {
    int idx =0;
    for(; idx<length; ++idx)
    {
      *(unsigned char*)(buffer +idx) = input_getc();
    }
    return length;
  } 

  lock_acquire(&lock_file);
  int ret =0;
  struct file *f = process_get_file(fd);
  if( !f)
    ret = -1;
  else
    ret = file_read(f,buffer,length);
  lock_release(&lock_file);

  return ret;  
}

int write(int fd, const void *buffer, unsigned length)
{
  check_address(buffer);
  if(1 == fd) 
  {
    putbuf(buffer, length);
    return length;
  }

  lock_acquire(&lock_file);
  int ret = 0;
  struct file *f = process_get_file(fd);
  if( !f)
    ret = -1;
  else
    ret = file_write(f, buffer, length); 
  lock_release(&lock_file);

  return ret;
}

void seek(int fd, unsigned position)
{ 
  struct file *f = process_get_file(fd);
  if( f)
    file_seek(f, position);
}

  unsigned
tell(int fd)
{ 
  struct file *f = process_get_file(fd);
  if( !f)
    return -1;

  return file_tell(f);
}

void close(int fd)
{
  process_close_file(fd);
}

