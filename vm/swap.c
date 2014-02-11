#include "vm/swap.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include "userprog/process.h"
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

struct lock lock_file;

void
swap_init()
{
  lock_init(&lock_file);
}


