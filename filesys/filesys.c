#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;
static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  bc_init();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();

  thread_current()->dir = dir_open_root(); //2.26 add ryoung sub-directories, save directory as root directory at first
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *path, off_t initial_size) 
{
  char *path_cp = malloc(strlen(path)+1);
  strlcpy(path_cp, path, strlen(path)+1);
  
  char file_name[NAME_MAX+1]; 
  struct dir *dir = parse_path(path_cp, file_name);
  free(path_cp);

  if(dir == NULL)
    return false;

  block_sector_t inode_sector = 0;
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, false)
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

//add create directory
bool
filesys_create_dir(const char *name)
{
  block_sector_t inode_sector = 0;
  char *name_cp = malloc(strlen(name)+1);
  strlcpy(name_cp, name, strlen(name)+1);
  
  char dir_name[NAME_MAX+1]; 
  struct dir *dir = parse_path(name_cp, dir_name);
  free(name_cp);

  if(dir == NULL)
    return false;

  struct dir *dir_child = NULL;
  bool success = (dir != NULL
      && free_map_allocate (1, &inode_sector)
      && dir_create (inode_sector, 16)
      && dir_add (dir, dir_name, inode_sector)
      && (dir_child = dir_open (inode_open (inode_sector)))
      && dir_add (dir_child, ".", inode_sector)
      && dir_add (dir_child, "..", inode_get_inumber (dir_get_inode (dir))));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir_child);
  dir_close(dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  char *name_cp = malloc(strlen(name)+1);
  strlcpy(name_cp, name, strlen(name)+1);

  char file_name[NAME_MAX+1];
  struct dir *dir = parse_path(name_cp, file_name);
  free(name_cp);
  if(dir == NULL)
    return false;
  
  struct inode *inode = NULL; 
  dir_lookup (dir, file_name, &inode);
  dir_close (dir);

  return file_open (inode);
}

bool
filesys_remove (const char *name) 
{
  char *name_cp = malloc(strlen(name)+1);
  strlcpy(name_cp, name, strlen(name)+1);
  char file_name[NAME_MAX+1];
  struct dir *dir = parse_path(name_cp, file_name);
  free(name_cp);

  if(dir == NULL)
    return false;

  struct inode *inode = NULL;
  if(!dir_lookup(dir, file_name, &inode))  
    return false;	
  if(inode_is_dir(inode)) //directory
  {
    struct dir *dir_child = dir_open(inode);
    bool bExist = false; //check directory has file
    char dir_name[NAME_MAX +1];
    while(dir_readdir(dir_child, dir_name))
    {
      if(strcmp(dir_name, ".") != 0 && strcmp(dir_name, "..") != 0)
      {
        bExist = true;
        break;
      }
    }
    dir_close(dir_child);
    if(!bExist)
      dir_remove(dir, file_name);
    else
      return false;
  }
  else //file
  dir_remove(dir, file_name);

  inode_close(inode);
  dir_close(dir);
  return true; 
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  
  struct dir *dir = dir_open_root();
  if(!dir_add(dir, ".", ROOT_DIR_SECTOR))
    PANIC ("root directory add '.' failed");
  if(!dir_add(dir, "..", ROOT_DIR_SECTOR))
    PANIC("root directory add '..' failed");
 
  free_map_close ();
  printf ("done.\n");
}

struct dir*
parse_path(char *path_name, char *file_name)
{
  if(path_name == NULL || strlen(path_name) == 0 )
    return NULL;
  
  //{ if path name start with '/', it is absolute path, relative path otherwise 
  struct dir *dir = NULL;
  if(path_name[0] == '/')
    dir = dir_open_root();
  else
    dir = dir_reopen(thread_current()->dir);
  //}

  char *token1, *token2, *save_ptr = NULL;

  token1 = strtok_r(path_name, "/", &save_ptr);
  if(token1 != NULL)
  {

    token2 = strtok_r(NULL, "/", &save_ptr);
    while( token2 != NULL)
    {
      struct inode *inode = NULL;
      if(!dir_lookup(dir, token1, &inode))
        return NULL;
      if(!inode_is_dir(inode))
        return NULL;
      dir_close(dir);
      dir = dir_open(inode);
      token1 = token2;
      token2 = strtok_r(NULL, "/", &save_ptr);
    }
  } 
  if(token1 == NULL)
    token1 = "."; 
  if(strlcpy(file_name, token1, NAME_MAX+1)> NAME_MAX +1)
    return NULL;
 if(inode_is_removed(dir_get_inode(dir)))
    return NULL;
  
  return dir;  
}
