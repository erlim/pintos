#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "vm/mmap.h"

static int vme_id = 1;
static thread_func process_start NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

struct lock lock_file; //syscall.c (lock_init()<-syscall.c<- init.c)
void process_init();

void
process_init()
{
  lock_init(&lock_file);
}

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  //printf("process_execute file: %s\n", file_name);
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  char *fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  // 12.27 modify Ryoung
  char *parse_name, save_ptr; 
  parse_name = palloc_get_page(0);
  strlcpy(parse_name, file_name, PGSIZE);
  parse_name = strtok_r(parse_name, " ",&save_ptr);       

  /* Create a new thread to execute FILE_NAME. */
  tid_t tid = thread_create (parse_name, PRI_DEFAULT, process_start, fn_copy);

  palloc_free_page(parse_name);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 
  
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
  static void
process_start (void *file_name_)
{
  //printf("start process\n");
  char *file_name = file_name_;
  struct intr_frame if_;

  //{ 12.27 +Ryoung token, count token
  char **argv = palloc_get_page(0);
  int argc = 0;
  char *token, *save_ptr=NULL;

  for( token = strtok_r(file_name, " ", &save_ptr); token!= NULL;
      token = strtok_r(NULL, " ",&save_ptr))
  {
    argv[argc] = malloc(strlen(token));
    strlcpy(argv[argc], token, PGSIZE);
    argc++;  
  } //}

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  // 1.4 modify Ryoung
  struct thread *cur = thread_current(); 
  bool bLoad = load(argv[0], &if_.eip, &if_.esp);
  palloc_free_page(file_name);
  sema_up(&cur->sema_load);
  if(bLoad)
  {
    cur->status_load = true;
    argument_stack(argv, argc, &if_.esp);
  }
  else
  {
    cur->status_load = -1;
    thread_exit();
  }
  
  int idx = 0;
  for(; idx<argc; idx++)
  { 
    free(argv[idx]);
  }
  palloc_free_page(argv);
 
  /* Start the user process by simulating a return from an
   interrupt, implemented by intr_exit (in
   threads/intr-stubs.S).  Because intr_exit takes all of its
   arguments on the stack in the form of a `struct intr_frame',
   we just point the stack pointer (%esp) to our stack frame
   and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

// 12.27 add Ryoung
void 
argument_stack(char **argv, int argc, void **esp)
{  
  char** argv_addr = palloc_get_page(0);

  int slen, buffer_size = 0;
  int num =0;
  for( num = argc-1; num >=0; --num)
  {
    //stack arguments
    slen = strlen(argv[num])+1;
    *esp -= slen; 
    memcpy(*esp, argv[num], slen);

    //save arguments_address
    argv_addr[num] = *esp; 
    buffer_size += slen;
  }

  //word-align(1Byte = 8bit, word=4Byte)
  if( !(buffer_size % sizeof(uint32_t)== 0) )
  {
    *esp -=1;
    memset(*esp, 0,1);
  }

  //stack argv address
  const int ptr_size = 4, zero_val =0;
  for(num = argc; num>=0; --num)
  {
    *esp -=ptr_size; 
    if(num == argc)
      memset(*esp, zero_val, ptr_size); //argv[*]
    else
      memcpy(*esp, &argv_addr[num], ptr_size); //argv[*-1.....0]  
  }

  //stack argv
  memcpy(*esp -ptr_size, esp, ptr_size);
  *esp -=ptr_size;

  //stack argc
  *esp -=ptr_size;
  memcpy(*esp, &argc, ptr_size); 

  //stack fake address
  *esp -=ptr_size;
  memset(*esp, zero_val ,ptr_size);

  palloc_free_page(argv_addr);
}
 
/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
  int
process_wait (pid_t child_pid UNUSED) 
{
  //printf("cur:%d, process_wait %d\n", thread_current()->tid, child_pid);
  struct thread *child = process_get_child(child_pid);
  if(!child)  
    return -1;
  if(child->wait)
    return -1;
  child->wait = true;   
  if(!child->exit)
    sema_down(&child->sema_exit);

  int status_exit = child->status_exit;
  process_remove_child(child);
  
  return status_exit; 
}

/* Free the current process's resources. */
  void
process_exit (void)
{
  struct thread *t = thread_current ();
  t->exit = true;

  //2.11 add ryoung (munmap)
  struct list *list = &t->mmap_files;
  struct list_elem *next, *e=list_begin(list);
  while(e != list_end(list))
  {
    struct mmap_file* mmapf = list_entry(e, struct mmap_file, elem);
    do_munmap(mmapf->id);
    if(list_empty(list))
      break;
    else
      e = list_begin(list);   
  }
  vm_destroy(&thread_current()->vm);

  //1.10 add ryoung (file descriptor)
  if(t->file_exec != NULL)
  {
    file_close(t->file_exec);
  }
  while(t->last_fd > FD_DEFINE)
  {
    t->last_fd--;
    process_close_file(t->last_fd);
  }
  palloc_free_page(t->fd_tbl);

  uint32_t *pd;
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = t->pagedir;
  if (pd != NULL) 
  {
    /* Correct ordering here is crucial.  We must set
       cur->pagedir to NULL before switching page directories,
       so that a timer interrupt can't switch back to the
       process page directory.  We must activate the base page
       directory before destroying the process's page
       directory, or our active page directory will be one
       that's been freed (and cleared). */
    pd = NULL;
    pagedir_activate (NULL);
    pagedir_destroy (pd);
  }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
  void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

// 1.3 add Ryoung, utils for process descriptor
struct thread*
process_get_child(pid_t pid)
{
  struct thread *cur = thread_current();
  struct list *child = &cur->child;
  if(!list_empty(child))
  {
    struct list_elem *e;
    for( e = list_begin(child); e != list_end(child); e = list_next(e) )
    {
      struct thread *t = list_entry(e, struct thread, child_elem);
      if(pid == t->tid)
        return t;
    }
  }
  return NULL;
  
}
void
process_remove_child(struct thread *t)
{
  list_remove(&t->child_elem);
  palloc_free_page(t);
}

int 
process_add_file(struct file *f)
{
  struct thread *t = thread_current();
  int fd = t->last_fd++;
  t->fd_tbl[fd] =f;
  return fd; 
}

  struct file*
process_get_file(int fd)
{
  struct thread *t= thread_current();
  if( t->fd_tbl[fd] != NULL)
    return t->fd_tbl[fd];
  return  NULL;
}

  void
process_close_file(int fd)
{
  struct thread *t = thread_current();
  if(t->fd_tbl[fd] != NULL)
    file_close(t->fd_tbl[fd]);
  t->fd_tbl[fd]= NULL; 
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
{
  unsigned char e_ident[16];
  Elf32_Half    e_type;
  Elf32_Half    e_machine;
  Elf32_Word    e_version;
  Elf32_Addr    e_entry;
  Elf32_Off     e_phoff;
  Elf32_Off     e_shoff;
  Elf32_Word    e_flags;
  Elf32_Half    e_ehsize;
  Elf32_Half    e_phentsize;
  Elf32_Half    e_phnum;
  Elf32_Half    e_shentsize;
  Elf32_Half    e_shnum;
  Elf32_Half    e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
{
  Elf32_Word p_type;
  Elf32_Off  p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

//2.6 add ryoung
//#define VM_BIN  1
//#define VM_FILE 2
//#define VM_SWAP 3 

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
    uint32_t read_bytes, uint32_t zero_bytes,
    bool writable);
static bool load_file(void *kaddr, struct vm_entry *vme);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  //printf("load file name: %s, tid:%d\n", file_name, t->tid);
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
  {
    printf ("load: %s: open failed\n", file_name);
    goto done; 
  }

  // 1.13 add ryoung(denying write to executables)  
  file_deny_write(file);
  t->file_exec = file;

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2   //executable file
      || ehdr.e_machine != 3  
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
  {
    printf ("load: %s: error loading executable\n", file_name);
    goto done; 
  }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
  {
    struct Elf32_Phdr phdr;

    if (file_ofs < 0 || file_ofs > file_length (file))
      goto done;
    file_seek (file, file_ofs);

    if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
      goto done;
    file_ofs += sizeof phdr;
    switch (phdr.p_type) 
    {
      case PT_NULL:
      case PT_NOTE:
      case PT_PHDR:
      case PT_STACK:
      default:
        /* Ignore this segment. */
        break;
      case PT_DYNAMIC:
      case PT_INTERP:
      case PT_SHLIB:
        goto done;
      case PT_LOAD: //loaded program segment
        if (validate_segment (&phdr, file)) 
        {
          bool writable = (phdr.p_flags & PF_W) != 0;
          uint32_t file_page = phdr.p_offset & ~PGMASK;
          uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
          uint32_t page_offset = phdr.p_vaddr & PGMASK;
          uint32_t read_bytes, zero_bytes;
          if (phdr.p_filesz > 0)
          {
            /* Normal segment.
               Read initial part from disk and zero the rest. */
            read_bytes = page_offset + phdr.p_filesz;
            zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                - read_bytes);
          }
          else 
          {
            /* Entirely zero.
               Don't read anything from disk. */
            read_bytes = 0;
            zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
          }
          if (!load_segment (file, file_page, (void *) mem_page,  
                read_bytes, zero_bytes, writable))
            goto done;
        }
        else
          goto done;
        break;
    }
  }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

  done:
  /* We arrive here whether the load is successful or not. */
  //file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);
/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
  static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

   - READ_BYTES bytes at UPAGE must be read from FILE
   starting at offset OFS.

   - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
  static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
    uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
  {
    /* Calculate how to fill this page.
       We will read PAGE_READ_BYTES bytes from FILE
       and zero the final PAGE_ZERO_BYTES bytes. */
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    //2.6 modify ryoung vm(demand page)
    struct vm_entry *vme = malloc(sizeof(struct vm_entry));
    vme->id = vme_id++;  
    vme->type = VM_BIN;
    vme->vaddr = upage;
    vme->writable = writable;
    vme->bLoad = false;
    vme->file = file;
    vme->offset = ofs; 
    vme->read_bytes = read_bytes;
    vme->zero_bytes = zero_bytes;
    vme_insert(&thread_current()->vm, vme);
    //ofs+=read_bytes; //add ryoung
//{originam code
    /* 
    // Get a page of memory. 
    uint8_t *kpage = palloc_get_page (PAL_USER);
    if (kpage == NULL)
      return false;
    // Load this page. 
    if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
    {
      palloc_free_page (kpage);
      return false; 
    }
    memset (kpage + page_read_bytes, 0, page_zero_bytes);
    // Add the page to the process's address space. 
    if (!install_page (upage, kpage, writable)) 
    {
      palloc_free_page (kpage);
      return false; 
    }
    */
//}
    /* Advance. */
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    upage += PGSIZE;
    ofs += page_read_bytes; //add ryoung 
  }
  return true;
}

//2.7 add ryoung 
static bool
load_file(void* kaddr, struct vm_entry *vme)
{
  //enum intr_level old_level;
  //old_level = intr_disable();
  struct lock lock_file;
  lock_init(&lock_file);
  if(vme->read_bytes > 0)
  {
    lock_acquire(&lock_file);
    if((int)vme->read_bytes != file_read_at(vme->file, kaddr, vme->read_bytes, vme->offset))
    {
      lock_release(&lock_file);
      return false;
    }
    lock_release(&lock_file);
    memset(kaddr+vme->read_bytes,0,vme->zero_bytes);
  }
  else
  {
    memset(kaddr,0,PGSIZE);
  }
  //intr_set_level(old_level);
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
  static bool
setup_stack (void **esp) 
{
  bool success = false;

  //{ 2.12 modify ryoung swap
  struct page /*uint8_t*/ *kpage = page_alloc(PAL_USER);
  //uint8_t *kpage = palloc_get_page (PAL_USER /*| PAL_ZERO*/);
  //}
  if (kpage != NULL) 
  {
    //*esp = PHYS_BASE;
    //2.6 add ryoung vm(demand paging)
    struct vm_entry *vme = malloc(sizeof(struct vm_entry));
    vme->id = vme_id ++;
    vme->type = VM_SWAP;
    vme->vaddr = ((uint8_t*)PHYS_BASE)-PGSIZE;
    vme->writable = true;
    vme->bLoad = true;
    vme_insert(&thread_current()->vm,vme);
    success = install_page(vme->vaddr, kpage, vme->writable);  
    if(success)
      *esp = PHYS_BASE;
    else
      page_free(kpage->kaddr); //2.12 modify ryoung swap
      //palloc_free_page (kpage);
  }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
      && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

//2.6 add ryoung 
bool 
handle_mm_fault(struct vm_entry *vme)
{
  //uint8_t *kpage;
  bool success = false;
  //{ 2.12 modify ryoung swap
  struct page *kpage = page_alloc(PAL_USER);
  kpage->vme = vme;
  //kpage = palloc_get_page( PAL_USER /*| PAL_ZERO*/);
  //}
  switch((uint8_t)vme->type)
  {
    case VM_BIN:
      load_file(kpage, vme);
      break;
    case VM_FILE:
      load_file(kpage, vme);
      break;
    case VM_SWAP: 
      swap_in(vme->swap_slot, kpage->kaddr /*vme->vaddr*/);
      break;
    default:
      break;
  }
  if(kpage != NULL)
  {
    success = install_page(vme->vaddr, kpage, vme->writable);
    vme->bLoad = true;
    if(!success)
    { 
      //2.12 modify ryoung swap
      page_free(kpage->kaddr);
      //printf("handle_mm_fault, failed to install_page\n");
      //palloc_free_page(kpage);
      return false;
    }
    return true;
  }
  //printf("handle_mm_fault failed to get page at kpage, %d \n", thread_current()->tid); 
  return false;  
}

