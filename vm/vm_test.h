struct pages : 4,096 bytes / vaddr.h
{
  int32_t offset : 12;
  int32_t number : 20;
}

struct frame : has to be same siw with pages
{
  int32_t offset : 12;
  int32_t number : 20;
}

struct page Tables : pagedir.c
{
  map pages and frame
}

struct swap slots
{
  ;
}
struct page table entry format : /pte.h
{
  physical_address
  avl: available for os system
  d: dirty bit 
  a: accessed bit
  u: user process can access or not
  w: writable or not 
  p: present(whether cpu)
}
