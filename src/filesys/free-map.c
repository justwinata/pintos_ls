#include "filesys/free-map.h"
#include <bitmap.h>
#include <debug.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"

static struct file *free_map_file;   /* Free map file. */
static struct bitmap *free_map;      /* Free map, one bit per sector. */

/* Initializes the free map. */
void
free_map_init (void) 
{
  DEBUG("free_map_init(): << START >>\n");
  free_map = bitmap_create (block_size (fs_device));
  if (free_map == NULL)
    PANIC ("bitmap creation failed--file system device is too large");
  bitmap_mark (free_map, FREE_MAP_SECTOR);
  bitmap_mark (free_map, ROOT_DIR_SECTOR);
  DEBUG("free_map_init(): << END >>\n");
}

/* Allocates CNT consecutive sectors from the free map and stores
   the first into *SECTORP.
   Returns true if successful, false if not enough consecutive
   sectors were available or if the free_map file could not be
   written. */
bool
free_map_allocate (size_t cnt, block_sector_t *sectorp)
{
  DEBUG ("free_map_allocate(): << START >>\n");
  block_sector_t sector = bitmap_scan_and_flip (free_map, 0, cnt, false);
  DEBUG ("free_map_allocate(): sector = %d\n", sector);
  bool success = sector != BITMAP_ERROR;
  DEBUG ("free_map_allocate(): sector != BITMAP_ERROR : %d\n", success);
  success &= free_map_file != NULL;
  DEBUG ("free_map_allocate(): free_map_file != NULL : %d\n", success);
  success &= !bitmap_write (free_map, free_map_file);
  DEBUG("free_map_allocate(): !bitmap_write (free_map, free_map_file) : %d\n", success);
  // if (sector != BITMAP_ERROR
  //     && free_map_file != NULL
  //     && !bitmap_write (free_map, free_map_file))
  if (success)
    {
      DEBUG ("free_map_allocate(): in first if\n");
      bitmap_set_multiple (free_map, sector, cnt, false); 
      sector = BITMAP_ERROR;
    }
  if (sector != BITMAP_ERROR) {
    DEBUG ("free_map_allocate(): ERROR OCURRED!!\n");
    *sectorp = sector;
  }
  DEBUG ("free_map_allocate(): << END >>\n");
  return sector != BITMAP_ERROR;
}

/* Makes CNT sectors starting at SECTOR available for use. */
void
free_map_release (block_sector_t sector, size_t cnt)
{
  DEBUG ("free_map_release(): << START >>\n");
  ASSERT (bitmap_all (free_map, sector, cnt));
  bitmap_set_multiple (free_map, sector, cnt, false);
  bitmap_write (free_map, free_map_file);
  DEBUG ("free_map_release(): << END >>\n");
}

/* Opens the free map file and reads it from disk. */
void
free_map_open (void) 
{
  DEBUG ("free_map_open(): << START >>\n");
  free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC ("can't open free map");
  if (!bitmap_read (free_map, free_map_file))
    PANIC ("can't read free map");
  DEBUG ("free_map_open(): << END >>\n");
}

/* Writes the free map to disk and closes the free map file. */
void
free_map_close (void) 
{
  DEBUG ("free_map_close(): << START >>\n");
  file_close (free_map_file);
  DEBUG ("free_map_close(): << END >>\n");
}

/* Creates a new free map file on disk and writes the free map to
   it. */
void
free_map_create (void) 
{
  DEBUG ("free_map_create(): << START >>\n");
  /* Create inode. */
  if (!inode_create (FREE_MAP_SECTOR, bitmap_file_size (free_map)))
    PANIC ("free map creation failed");

  /* Write bitmap to file. */
  free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC ("can't open free map");
  if (!bitmap_write (free_map, free_map_file))
    PANIC ("can't write free map");
  DEBUG ("free_map_create(): << END >>\n");
}
