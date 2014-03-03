#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"

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

//2.25 add ryoung
//project 4(filesystem)
bool chdir (const char *dir);
bool mkdir (const char *dir);
#ifndef READDIR_MAX_LEN
#define READDIR_MAX_LEN 14
#endif
bool readdir (int fd, char* name);
bool isdir (int fd);
int inumber (int fd);

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
    case SYS_CHDIR :
      get_argument(f->esp, &arg,1);     
      f->eax = chdir((char*)arg[0]);                           break;
    case SYS_MKDIR :
      get_argument(f->esp, &arg,1);
      f->eax = mkdir((char*)arg[0]);                           break;
    case SYS_READDIR :
      get_argument(f->esp, &arg,2); 
      f->eax = readdir(arg[0], (char*)arg[1]);                  break;
    case SYS_ISDIR :
      get_argument(f->esp, &arg,1);
      f->eax = isdir(arg[0]);                                  break;
    case SYS_INUMBER :
      get_argument(f->esp, &arg, 1);
      f->eax = inumber(arg[0]);                                break;
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
  printf("system halt\n");
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

int wait(pid_t pid)
{
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
  if(!f)
    ret = -1;
  if(inode_is_dir(file_get_inode(f))) //add ryoung
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

bool chdir(const char *dir_name) //change directory
{
  struct file *f = filesys_open(dir_name); //file open with name
  if(f == NULL) 
    return false;
  struct inode *inode = inode_reopen(file_get_inode(f));
  file_close(f);

  struct dir *dir = dir_open(inode);
  if(dir == NULL)
    return false;

  dir_close(thread_current()->dir);
  thread_current()->dir = dir;

  return true;
}

bool mkdir(const char *dir)
{
  return filesys_create_dir(dir); 
}

bool readdir(int fd, char *name)
{
  struct dir *dir = NULL;
  if(isdir(fd))
  {
    struct dir *dir = process_get_file(fd);
    return dir_readdir(dir, name);
  } 
}

bool isdir(int fd)
{
  struct file *f = process_get_file(fd);
  if(!f)
    return -1;
 
  return inode_is_dir(file_get_inode(f));
}
int inumber(int fd)
{
  struct file* f =process_get_file(fd);
  if(!f)
    return -1;
 
  return inode_get_inumber(file_get_inode(f));
}
