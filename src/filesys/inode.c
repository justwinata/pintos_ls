#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "kernel/malloc.h"
#include "kernel/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define DIR_BLOCK_SIZE 123    /* Number of direct blocks. */
#define INDIR_BLOCK_SIZE 128  /* Each disk block can hold 512 / 4 = 128 addresses 
                                 to other blocks.
                                 http://web.stanford.edu/class/cs140/cgi-bin/section/10sp-proj4.pdf */

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */

#if 0
    uint32_t direct[DIR_BLOCK_SIZE];    /* Direct blocks. */
    block_sector_t *indirect;           /* Indirect blocks. */
    block_sector_t **doubly_indirect;   /* Doubly indirect blocks. */
#endif
#if 1
    uint32_t unused[125];               /* Not used. */
#endif
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */

#if 0
    struct lock write_lock;             /* Lock for synchronization. */
#endif
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  #if 0
  // DEBUG("Calling byte_to_sector(%p, %d)...\n", inode, pos);
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
  {
      size_t sectors = pos / BLOCK_SECTOR_SIZE;
      size_t num_dir = (sectors < DIR_BLOCK_SIZE) ? sectors : DIR_BLOCK_SIZE;

      DEBUG("Move onto direct blocks\n");
      if (sectors <= DIR_BLOCK_SIZE)
      {
        DEBUG("byte at %d (%d) = %d\n", pos, num_dir, inode->data.start + num_dir);
        DEBUG("...byte_to_sector() successful.\n");
        return inode->data.start + num_dir;
      }

      DEBUG("Move onto indirect blocks\n");
      size_t num_indir = sectors - DIR_BLOCK_SIZE;
      DEBUG("num_indir = %d\n", num_indir);
      if (sectors <= INDIR_BLOCK_SIZE + DIR_BLOCK_SIZE)
      {
        DEBUG("byte at %d (%d, %d) = %d\n", pos, num_dir, num_indir, *inode->data.indirect + num_indir);
        DEBUG("...byte_to_sector() successful.\n");
        return *inode->data.indirect + num_indir;
      }

      DEBUG("Move onto doubly indirect blocks\n");
      size_t num_double = num_indir - INDIR_BLOCK_SIZE;
      size_t dbl_indir_index = num_double / INDIR_BLOCK_SIZE;
      size_t dbl_indir_pos = num_double % INDIR_BLOCK_SIZE;
      DEBUG("byte at %d (%d, %d) = %d\n", pos, dbl_indir_index, dbl_indir_pos, *(*inode->data.doubly_indirect + dbl_indir_index) + dbl_indir_pos);
      DEBUG("...byte_to_sector() successful.\n");
      return *(*inode->data.doubly_indirect + dbl_indir_index) + dbl_indir_pos;
  }
  else
  {
      DEBUG("%d out of bounds (%d)\n", pos, inode->data.length);
      // DEBUG("...byte_to_sector() successful.\n");
      return -1;
  }
#endif
#if 1
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  else
    return -1;
#endif
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

#if 0
/* Method Declarations */
bool inode_grow (struct inode_disk *, off_t);
#endif

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
#if 0
  DEBUG("\n\n==============================\n");
  DEBUG("inode_create(): START\n");
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  DEBUG("length = %d\n", length);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
  {
      size_t sectors = bytes_to_sectors (length);
      DEBUG("num sectors = %d\n", sectors);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;

      size_t num_dir = (sectors < DIR_BLOCK_SIZE) ? sectors : DIR_BLOCK_SIZE;
      size_t num_indir = (num_dir < DIR_BLOCK_SIZE) ? 0 : sectors - DIR_BLOCK_SIZE;
      if (num_indir >= INDIR_BLOCK_SIZE)
          num_indir -= INDIR_BLOCK_SIZE;
      size_t num_double = (num_indir < INDIR_BLOCK_SIZE) ? 0 : num_indir - INDIR_BLOCK_SIZE;

      DEBUG("num_dir = %d, num_indir = %d, num_double = %d\n", num_dir, num_indir, num_double);

      static char zeros[BLOCK_SECTOR_SIZE];
      size_t i;

      // Allocate direct blocks
      DEBUG("Allocating direct blocks\n");
      if (free_map_allocate(num_dir, &disk_inode->start))
      {
          block_write (fs_device, sector, disk_inode);
          for (i = 0; i < num_dir; i++) 
              block_write (fs_device, disk_inode->start + i, zeros);
          DEBUG("Direct blocks allocated successfully!\n");
          success = true;
      }
      else
      {
          DEBUG("Direct blocks not allocated successfully :(\n");
          success = false;
      }

      // Allocate indirect blocks
      if (num_indir > 0)
      {
          DEBUG("Allocating indirect blocks\n");
          disk_inode->indirect = calloc (1, BLOCK_SECTOR_SIZE);
          if (disk_inode->indirect != NULL)
          {
              for (i = 0; i < num_indir; i++)
              {
                  block_sector_t indir_block = *disk_inode->indirect + i;
                  DEBUG("indir_block = %d\n", indir_block);
                  if (free_map_allocate(1, &indir_block)) 
                      block_write (fs_device, indir_block, zeros);
                  else
                      success = false;
              }
          }
          else
              success = false;
          DEBUG("Indirect blocks %sallocated successfully%s\n", success ? "" : "not ", success ? "!" : " :(");
      }

      // Allocate doubly indirect blocks
      if (num_double > 0)
      {
          DEBUG("Allocating doubly indirect blocks\n");
          disk_inode->doubly_indirect = calloc (1, BLOCK_SECTOR_SIZE);
          if (disk_inode->indirect != NULL)
          {
              size_t num_indir_blocks = DIV_ROUND_UP(num_double, INDIR_BLOCK_SIZE);
              for (i = 0; i < num_indir_blocks; i++)
              {
                  block_sector_t *indir_block = *disk_inode->doubly_indirect + i;
                  DEBUG("indir_block = %p\n", indir_block);
                  indir_block = calloc (1, BLOCK_SECTOR_SIZE);
                  if (indir_block != NULL)
                  {
                      size_t j;
                      for (j = 0; j < INDIR_BLOCK_SIZE && j < num_double; j++)
                      {
                          block_sector_t dbl_indir_block = *indir_block + j;
                          DEBUG("dbl_indir_block = %d\n", dbl_indir_block);
                          if (free_map_allocate(1, &dbl_indir_block))
                              block_write (fs_device, dbl_indir_block, zeros);
                          else
                              success = false;
                      }
                      num_double -= INDIR_BLOCK_SIZE;
                      if (j < INDIR_BLOCK_SIZE)
                        break;
                  }
                  else
                  {
                      success = false;
                      break;
                  }
              }
          }
          else
              success = false;
          DEBUG("Doubly indirect blocks %sallocated successfully%s\n", success ? "" : "not ", success ? "!" : " :(");
      }
      free (disk_inode);
  }
  DEBUG("success = %d\n", success);
  DEBUG("inode_create(): END\n");
  DEBUG("==============================\n\n\n");
  return success;
#endif
#if 1
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
#endif
}

/* Reads an inode from SECTOR
   and returns a 'struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  // DEBUG("inode.c:inode_read_at() : inode->data.length = %d\n", inode->data.length);
  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

#if 0
  /* Grow inode if there is a write request past the EOF. */
  if (offset + size > inode->data.length)
  {
      DEBUG("inode_write_at(): inode size too small. Grow inode\n");
      lock_acquire(&inode->write_lock);
      inode_grow (&inode->data, offset + size);
      lock_release(&inode->write_lock);
  }
#endif

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
      #if 0
          lock_acquire(&inode->write_lock);
          block_write (fs_device, sector_idx, buffer + bytes_written);
          lock_release(&inode->write_lock);
      #endif
      #if 1
          block_write (fs_device, sector_idx, buffer + bytes_written);
      #endif
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
      #if 0
          lock_acquire(&inode->write_lock);
          block_write (fs_device, sector_idx, bounce);
          lock_release(&inode->write_lock);
      #endif
      #if 1
          block_write (fs_device, sector_idx, bounce);
      #endif
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

#if 0
/* Grows DISK_INODE to NEW_SIZE.
   Returns true if successful, and false otherwise. */
bool
inode_grow (struct inode_disk *disk_inode, off_t new_size)
{
    DEBUG("\n\ninode_grow(): START\n");

    bool success = true;

    // Calculate new block numbers
    size_t new_sectors = bytes_to_sectors (new_size);
    DEBUG("new_sectors = %d\n", new_sectors);
    size_t new_num_dir = (new_sectors < DIR_BLOCK_SIZE) ? new_sectors : DIR_BLOCK_SIZE;
    size_t new_num_indir = (new_num_dir < DIR_BLOCK_SIZE) ? 0 : new_sectors - DIR_BLOCK_SIZE;
    if (new_num_indir >= INDIR_BLOCK_SIZE)
        new_num_indir -= INDIR_BLOCK_SIZE;
    size_t new_num_double = (new_num_indir < INDIR_BLOCK_SIZE) ? 0 : new_num_indir - INDIR_BLOCK_SIZE;
    DEBUG("new_num_dir = %d, new_num_indir = %d, new_num_double = %d\n", new_num_dir, new_num_indir, new_num_double);

    // Calculate old block numbers
    size_t old_sectors = bytes_to_sectors(disk_inode->length);
    DEBUG("old_sectors = %d\n", old_sectors);
    size_t old_num_dir = (old_sectors < DIR_BLOCK_SIZE) ? old_sectors : DIR_BLOCK_SIZE;
    size_t old_num_indir = (old_num_dir < DIR_BLOCK_SIZE) ? 0 : old_sectors - DIR_BLOCK_SIZE;
    if (old_num_indir >= INDIR_BLOCK_SIZE)
        old_num_indir -= INDIR_BLOCK_SIZE;
    size_t old_num_double = (old_num_indir < INDIR_BLOCK_SIZE) ? 0 : old_num_indir - INDIR_BLOCK_SIZE;
    DEBUG("old num_dir = %d, old num_indir = %d, old num_double = %d\n", old_num_dir, old_num_indir, old_num_double);

    // Calculate differences
    size_t num_dir = new_num_dir - old_num_dir;
    size_t num_indir = new_num_indir - old_num_indir;
    size_t num_double = new_num_double - old_num_double;
    DEBUG("diff num_dir = %d, diff num_indir = %d, diff num_double = %d\n", num_dir, num_indir, num_double);

    // Set disk_inode's new length
    disk_inode->length = new_size;

    static char zeros[BLOCK_SECTOR_SIZE];
    size_t i;

    // Allocate additional direct blocks
    if (num_dir > 0) 
    {
        DEBUG("Allocating additional direct blocks\n");
        block_sector_t start = disk_inode->start + old_num_dir;
        if (free_map_allocate(num_dir, &start))
        {
            for (i = 0; i < num_dir; i++) 
                block_write (fs_device, start + i, zeros);
            DEBUG("Additional direct blocks allocated successfully!\n");
        }
        else
        {
            DEBUG("Additional direct blocks not allocated successfully :(\n");
            success = false;
        }
    }

    // Allocate additional indirect blocks
    if (num_indir > 0)
    {
        DEBUG("Allocating additional indirect blocks\n");
        if (disk_inode->indirect != NULL)
        {
            for (i = 0; i < num_indir; i++)
            {
                block_sector_t indir_block = *disk_inode->indirect + old_num_indir + i;
                DEBUG("indir_block = %p\n", indir_block);
                if (free_map_allocate(1, &indir_block))
                    block_write (fs_device, indir_block, zeros);
                else
                    success = false;
            }
        }
        else
            success = false;
        DEBUG("Additional indirect blocks %sallocated successfully%s\n", success ? "" : "not ", success ? "!" : " :(");
    }

    // Allocate additional doubly indirect blocks
    if (num_double > 0)
    {
        DEBUG("Allocating additional doubly indirect blocks\n");
        if (disk_inode->indirect != NULL)
        {
            size_t old_num_indir_blocks = DIV_ROUND_UP(old_num_double, INDIR_BLOCK_SIZE);
            size_t new_num_indir_blocks = DIV_ROUND_UP(new_num_double, INDIR_BLOCK_SIZE);
            size_t j = old_num_double;
            for (i = old_num_indir_blocks; i < new_num_indir_blocks; i++)
            {
                block_sector_t *indir_block = *disk_inode->doubly_indirect + i;
                DEBUG("indir_block = %p\n", indir_block);
                if (i > old_num_indir_blocks)
                    indir_block = calloc (1, BLOCK_SECTOR_SIZE);
                if (indir_block != NULL)
                {
                    for (; j < INDIR_BLOCK_SIZE && j < new_num_double; j++)
                    {
                        block_sector_t dbl_indir_block = *indir_block + j;
                        DEBUG("dbl_indir_block = %p\n", dbl_indir_block);
                        if (free_map_allocate(1, &dbl_indir_block))
                            block_write (fs_device, dbl_indir_block, zeros);
                        else
                            success = false;
                    }
                    new_num_double -= INDIR_BLOCK_SIZE;
                    if (j < INDIR_BLOCK_SIZE)
                        break;
                    j = 0;
                }
                else
                {
                    success = false;
                    break;
                }
            }
        }
        else
            success = false;
        DEBUG("Additional doubly indirect blocks %sallocated successfully%s\n", success ? "" : "not ", success ? "!" : " :(");
    }

    DEBUG("inode_grow(): END\n\n\n");
    return success;
}
#endif
