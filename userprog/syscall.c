#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "vm/page.h"
#include "vm/mmap.h"

static void syscall_handler (struct intr_frame *);
struct lock lock_file;

void get_argument(void *esp, int*arg, int count);
void check_address(void *addr);
//2.9 add ryoung(vm)
void check_valid_buffer(void *buffer, unsigned size);
void check_valid_string(const void *str);

void sys_halt(void);
void sys_exit(int status);
int  sys_exec(const char *file);
int  sys_wait(int);
bool sys_create(const char* file, unsigned initial_size);
bool sys_remove(const char* file);
int  sys_open(const char *file);
int  sys_filesize (int fd);
int  sys_read(int fd, void *buffer, unsigned length);
int  sys_write(int fd, const void *buffer, unsigned length);
void sys_seek(int fd, unsigned position);
unsigned sys_tell (int fd);
void sys_close (int fd);
//vm
int sys_mmap(int fd, void *addr);
void sys_munmap(int mapping);

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
      sys_halt();                                         break;
    case SYS_EXIT :  
      get_argument(f->esp, &arg,1); 
      sys_exit(arg[0]);                                   break;
    case SYS_EXEC :
      get_argument(f->esp, &arg,1);
      f->eax = sys_exec((const char*)arg[0]);             break;
    case SYS_WAIT :
      get_argument(f->esp, &arg,1);
      f->eax = sys_wait(arg[0]);                          break;
    case SYS_CREATE : 
      get_argument(f->esp, &arg,2);
      f->eax = sys_create((const char*)arg[0], (unsigned)arg[1]);           break;
    case SYS_REMOVE :
      get_argument(f->esp, &arg,1);        
      f->eax = sys_remove((const char*)arg[0]);           break;
    case SYS_OPEN :
      get_argument(f->esp, &arg,1);
      f->eax = sys_open((const char*)arg[0]);             break;
    case SYS_FILESIZE :
      get_argument(f->esp, &arg,1);
      f->eax = sys_filesize(arg[0]);                      break;
    case SYS_READ : 
      get_argument(f->esp, &arg,3);
      f->eax = sys_read(arg[0], (void*)arg[1], (unsigned)arg[2]);           break;
    case SYS_WRITE :
      get_argument(f->esp, &arg,3);
      f->eax = sys_write(arg[0], (const void*)arg[1], (unsigned)arg[2]);    break; 
    case SYS_SEEK :
      get_argument(f->esp, &arg,2);
      sys_seek(arg[0], (unsigned)arg[1]);                                   break;
    case SYS_TELL :  
      get_argument(f->esp, &arg,1);
      f->eax = sys_tell(arg[0]);                          break;
    case SYS_CLOSE :
      get_argument(f->esp, &arg,1);
      sys_close(arg[0]);                                  break;
    case SYS_MMAP : 
      get_argument(f->esp, &arg,2);             
      f->eax = sys_mmap((int)arg[0],arg[1]);                   break;
    case SYS_MUNMAP :
      get_argument(f->esp, &arg,1);
      sys_munmap((int)arg[0]);                                 break;
    default :
      printf("failed to syscall, cuz of invailed syscall\n");
      thread_exit(); 
      break;
  }
}
void 
get_argument(void *esp, int *arg, int argc)
{
  int idx=0, num=1;
  for(; num<=argc; num++,idx++)
  {
    int *pval = (int*)esp +num;
    check_address(pval);
    arg[idx] = *pval;
  }   
}

void 
check_address(void *addr)
{
  if(! (addr>=(void*)0x0000000 && addr<(void*)0xc0000000) )
    sys_exit(-1);
  //if(! (addr>=(void*)0x8048000 && addr<(void*)0xc0000000) )
  //  sys_exit(-1);
}   

//@todo think. why???
void 
check_valid_buffer(void *buffer, unsigned size)
{
  unsigned idx =0;
  char* buffer_ = (char*)buffer;
  for(; idx<size; ++idx)
  {
    check_address((const void*)buffer_);
    buffer_++;
  }
}
void 
check_valid_string(const void *str)
{
  check_address(str);
}

void 
sys_halt(void)
{
  shutdown_power_off();
}

void 
sys_exit(int status)
{
  struct thread *t = thread_current();
  t->exit= true;
  t->status_exit = status;
  printf("%s: exit(%d)\n", t->name, status);
  thread_exit();
}

pid_t 
sys_exec(const char *file)
{
  //printf("syscall exec\n");
  pid_t pid = process_execute(file);
  struct thread *child = process_get_child(pid);
  if(!child)
    return -1;
  if(child->status_load == false)
    sema_down(&child->sema_load);
  if(child->status_load == -1)
    return -1;

  return pid;
}

int 
sys_wait(pid_t pid)
{
  //printf("syscall wait:%d\n", pid);
  return process_wait(pid);
}


bool 
sys_create(const char* file, unsigned initial_size)
{
  if(!file) 
    sys_exit(-1);

  return  filesys_create(file, initial_size);
}

bool 
sys_remove(const char* file)
{
  if(!file)
    sys_exit(-1); 

  return  filesys_remove(file);
}

int 
sys_open(const char *file)
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

int
sys_filesize(int fd)
{
  struct file *f = process_get_file(fd);
  if( !f) 
    return -1;

  return file_length(f);  
}

int 
sys_read(int fd, void *buffer, unsigned length)
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

int 
sys_write(int fd, const void *buffer, unsigned length)
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

void 
sys_seek(int fd, unsigned position)
{ 
  struct file *f = process_get_file(fd);
  if( f)
    file_seek(f, position);
}

unsigned
sys_tell(int fd)
{ 
  struct file *f = process_get_file(fd);
  if( !f)
    return -1;

  return file_tell(f);
}

void 
sys_close(int fd)
{
  process_close_file(fd);
}
int
sys_mmap(int fd, void *addr)
{
  return do_mmap(fd, addr);
}
void 
sys_munmap(int mapping)
{
  do_munmap(mapping);
}
