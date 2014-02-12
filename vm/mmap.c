#include "vm/mmap.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include "userprog/process.h"
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static struct lock lock_file;
static struct lock lock_mapid;
typedef int mapid_t;

static mapid_t allocate_mapid(void);

void
mmap_init(void)
{
  lock_init(&lock_file);
  lock_init(&lock_mapid);
}

static mapid_t
allocate_mapid(void)
{
  static mapid_t next_tid = 1;
  mapid_t mapid;

  lock_acquire(&lock_mapid);
  mapid = next_tid++;
  lock_release(&lock_mapid);
  
  return mapid;
}

static bool
check_is_mmaped(void *addr)
{
  struct vm_entry vme;
  vme.vaddr = pg_round_down(addr);
  if( NULL != hash_find(&thread_current()->vm, &vme.elem))
    return true;
  else
    return false;
}

int 
do_mmap(int fd, void *addr)
{
  //{ verify address
  if(addr == 0 /*NULL*/)
    return -1;
  if((uint32_t)addr % PGSIZE != 0) //mmap-misalign.c
    return -1;
  //if(pg_ofs(addr) != 0)
  //    return -1;
  if(check_is_mmaped(addr)) //code, stk, data
    return -1;
  //if(addr >= 0x08048000) //check_address code, stk, data ...??????????
  //{
    //0x10000000  : user space 
    //0x8048000   : over-code
    //0x804b000   : over-data
    //0xbffff000  : over-stk
    //0xc0000000 : PHY_SIZE (is_user()
    //printf("addr");
  //  return -1;
  //}
  //}

  struct file* file_ = process_get_file(fd);
  if(file_ !=  NULL)
  {
    struct file* file = file_reopen(file_);
    uint32_t ofs = file_tell(file); //0; //sys_tell(fd)
    uint32_t read_bytes, zero_bytes;
    read_bytes  = file_length(file); //sys_filesize(fd);
    if(read_bytes == 0 ) //mmap-zero.c
      return -1;
    zero_bytes = PGSIZE - (read_bytes % PGSIZE);
 
    ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
    //ASSERT (pg_ofs (addr) == 0);
    //ASSERT (ofs % PGSIZE == 0);

    struct mmap_file *mmapf = malloc(sizeof(struct mmap_file));
    mmapf->id = allocate_mapid(); 
    mmapf->bUnmap = false;
    mmapf->file = file;
    list_init(&mmapf->vmes);
    while(read_bytes > 0) //|| zero_bytes > 0)
    {
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      struct vm_entry *vme = malloc(sizeof(struct vm_entry));
      vme->id = 99;
      vme->type = VM_FILE;
      vme->vaddr = addr;
      vme->writable = file_is_writable(file); 
      vme->bLoad = false;
      vme->file = file;
      vme->offset = ofs;
      vme->read_bytes = read_bytes;
      vme->zero_bytes = zero_bytes;
      vme_insert(&thread_current()->vm, vme);
      list_push_back(&mmapf->vmes, &vme->mmap_elem);
      
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      addr += PGSIZE;
      ofs += page_read_bytes;
    }
    list_push_back(&thread_current()->mmap_files, &mmapf->elem);
    return mmapf->id;
  }
  return -1;
}

void
do_munmap(int mapping)
{
  struct thread* t = thread_current();
  struct list *list = &t->mmap_files;
  struct list_elem *next, *e= list_begin(list);
  while(e != list_end(list))
  {
    next = list_next(e);
    struct mmap_file* mmapf = list_entry(e, struct mmap_file, elem);
    if(mmapf->id == mapping)
    {
      mmapf->bUnmap = true;
      struct list* list_vme = &mmapf->vmes;
      struct list_elem *next_vme, *e_vme = list_begin(list_vme);
      while(e_vme != list_end(list_vme))
      {
        next_vme = list_next(e_vme);
        struct vm_entry *vme = list_entry(e_vme, struct vm_entry, mmap_elem);
        if(vme->bLoad)
        {
          if(pagedir_is_dirty(t->pagedir, vme->vaddr))
          {
            lock_acquire(&lock_file);
            file_write_at(vme->file, vme->vaddr, vme->read_bytes, vme->offset);
            lock_release(&lock_file);
          }
          //2.12 modify ryoung swap
          page_free(vme->vaddr);
          //palloc_free_page(pagedir_get_page(t->pagedir, vme->vaddr));
          pagedir_clear_page(t->pagedir, vme->vaddr);
        }
        list_remove(&vme->mmap_elem);
        vme_remove(&t->vm, vme);
        e_vme = next_vme;
      }
      //do_munmap(mmapf);
      //file_close(mmapf->file);
      list_remove(&mmapf->elem);
      free(mmapf); 
    }
    e = next;
  }
}

/*
void
do_munmap(struct mmap_file *mmapf)
{
  //struct lock lock_file;
  //lock_init(&lock_file);

  struct thread* t = thread_current();
  struct list* list = &mmapf->vmes;
  struct list_elem *next, *e = list_begin(list);
  while(e != list_end(list))
  {
    next = list_next(e);
    struct vm_entry *vme = list_entry(e, struct vm_entry, mmap_elem);
    if(vme->bLoad)
    {
      if(pagedir_is_dirty(t->pagedir, vme->vaddr))
      {
        lock_acquire(&lock_file);
        file_write_at(vme->file, vme->vaddr, vme->read_bytes, vme->offset);
        lock_release(&lock_file);
      }
      palloc_free_page(pagedir_get_page(t->pagedir, vme->vaddr));
      pagedir_clear_page(t->pagedir, vme->vaddr);
    }
    list_remove(&vme->mmap_elem);
    delete_vme(&t->vm, &vme->elem);
    e = next;
  }
}
*/
