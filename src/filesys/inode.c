#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "kernel/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define DIR_BLOCK_SIZE 123
#define INDIR_BLOCK_SIZE 128  // Each disk block can hold 512 / 4 = 128 addresses 
                              // to other blocks.
                              // http://web.stanford.edu/class/cs140/cgi-bin/section/10sp-proj4.pdf

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    // uint32_t unused[125];               /* Not used. */
    uint32_t unused[DIR_BLOCK_SIZE];    /* Direct blocks. */

    block_sector_t *indirect;           /* Indirect blocks. */
    block_sector_t **doubly_indirect;   /* Doubly indirect blocks. */
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
  };



/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  // printf("Calling byte_to_sector(%p, %d)...\n", inode, pos);
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    {
      size_t dir_pos = pos / BLOCK_SECTOR_SIZE;
      if (dir_pos <= DIR_BLOCK_SIZE)
      {
        // printf("byte at %d (%d) = %d\n", pos, dir_pos, inode->data.start + dir_pos);
        // printf("...byte_to_sector() successful.\n");
        return inode->data.start + dir_pos;
      }

      size_t indir_pos = dir_pos - DIR_BLOCK_SIZE;
      if (dir_pos <= INDIR_BLOCK_SIZE + DIR_BLOCK_SIZE)
      {
        // printf("byte at %d (%d, %d) = %d\n", pos, dir_pos, indir_pos, *inode->data.indirect + indir_pos);
        // printf("...byte_to_sector() successful.\n");
        return *inode->data.indirect + indir_pos;
      }

      size_t dbl_indir_index = indir_pos / INDIR_BLOCK_SIZE;
      size_t dbl_indir_pos = indir_pos % INDIR_BLOCK_SIZE;
      // printf("byte at %d (%d, %d, %d, %d) = %d\n", pos, dir_pos, indir_pos, dbl_indir_index, indir_pos % INDIR_BLOCK_SIZE, *(*(inode->data.doubly_indirect) + dbl_indir_index) + dbl_indir_pos);
      // printf("...byte_to_sector() successful.\n");
      return *(*(inode->data.doubly_indirect) + dbl_indir_index) + dbl_indir_pos;
    }
  else
    // printf("%d out of bounds\n", pos);
    printf("%d out of bounds (%d)\n", pos, inode->data.length);
    // printf("...byte_to_sector() successful.\n");
    return -1;

}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Method declarations. */
// void grow_file (struct inode *inode, off_t new_length);

// void 
// grow_file (struct inode *inode, off_t new_length)
// {
//   if (sectors <= DIR_BLOCK_SIZE) 
//     {
//       printf("Number of sectors is less than or equal to DIR_BLOCK_SIZE\n");
//       // Write to direct blocks
//       for (i = 0; i < sectors; i++) 
//         block_write (fs_device, disk_inode->start + i, zeros);
//       printf("\tDirect blocks written successfully.\n");
//     }
//   else if (sectors <= DIR_BLOCK_SIZE * INDIR_BLOCK_SIZE) 
//     {
//       printf("Number of sectors is less than or equal to DIR_BLOCK_SIZE * INDIR_BLOCK_SIZE\n");
//       // Write to direct blocks
//       for (i = 0; i < DIR_BLOCK_SIZE; i++) 
//         block_write (fs_device, disk_inode->start + i, zeros);
//       printf("\tDirect blocks written successfully.\n");

//       // Allocate memory for indirect blocks
//       disk_inode->indirect = calloc (1, DIR_BLOCK_SIZE);
//       size_t memory_size = sectors - DIR_BLOCK_SIZE;
//       printf("\tmemory_size = %d\n", memory_size);
//       if (free_map_allocate (memory_size, disk_inode->indirect)) 
//         {
//           printf("\tIndirect block memory allocated\n");
//           // Write to indirect blocks
//           for (; i < sectors; i++)
//             block_write (fs_device, *(disk_inode->indirect) + i, zeros);
//           printf("\tIndirect blocks written successfully.\n");
//         }
//     }
//   else
//     {
//       printf("Number of sectors is less than or equal to MAX_FILE_SIZE\n");
//       // Write to direct blocks
//       for (i = 0; i < DIR_BLOCK_SIZE; i++) 
//         block_write (fs_device, disk_inode->start + i, zeros);
//       printf("\tDirect blocks written successfully.\n");

//       // Allocate memory for indirect blocks
//       disk_inode->indirect = calloc (1, DIR_BLOCK_SIZE);
//       size_t memory_size = sectors - DIR_BLOCK_SIZE;
//       printf("\tmemory_size = %d\n", memory_size);
//       if (free_map_allocate (memory_size, disk_inode->indirect)) 
//         {
//           // Write to indirect blocks
//           for (; i < INDIR_BLOCK_SIZE * DIR_BLOCK_SIZE; i++)
//             block_write (fs_device, *(disk_inode->indirect) + i, zeros);
//           printf("\tIndirect blocks written successfully.\n");

//           // Allocate memory for doubly indirect blocks
//           size_t sectors_left = DIV_ROUND_UP (sectors, INDIR_BLOCK_SIZE * DIR_BLOCK_SIZE);
//           size_t j;
//           for (j = 0; j < sectors_left; j++)
//             {
//               // Allocate memory for indirect blocks
//               disk_inode->doubly_indirect = calloc (INDIR_BLOCK_SIZE, DIR_BLOCK_SIZE);
//               memory_size = sectors - DIR_BLOCK_SIZE - j*INDIR_BLOCK_SIZE;
//               printf("\tmemory_size = %d\n", memory_size);
//               if (free_map_allocate (memory_size, 
//                                      *disk_inode->doubly_indirect)) 
//                 {
//                   // Write to indirect blocks
//                   for (; i < sectors; i++)
//                     block_write (fs_device, *(*(disk_inode->doubly_indirect)) + i, zeros);
//                 }
//             }
//           printf("\tDoubly indirect blocks written successfully.\n");
//         }
//     }
// }

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
  printf("\n\n==============================\n");
  printf("Calling inode_create...\n");
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  printf("length = %d\n", length);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      printf("num sectors = %d\n", sectors);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          printf("Allocated free map.\n");
          block_write (fs_device, sector, disk_inode);
          printf("Wrote first block.\n");
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;

              // Write to direct blocks
              if (sectors <= DIR_BLOCK_SIZE) 
                {
                  printf("Number of sectors <= DIR_BLOCK_SIZE\n");
                  for (i = 0; i < sectors; i++) 
                    block_write (fs_device, disk_inode->start + i, zeros);
                  printf("\tDirect blocks written successfully.\n");
                }
              else 
                {
                  printf("Number of sectors > DIR_BLOCK_SIZE\n");
                  for (i = 0; i < DIR_BLOCK_SIZE; i++) 
                    block_write (fs_device, disk_inode->start + i, zeros);
                  printf("\tDirect blocks written successfully.\n");

                  sectors -= DIR_BLOCK_SIZE;

                  // Allocate memory for indirect blocks
                  disk_inode->indirect = calloc (INDIR_BLOCK_SIZE, BLOCK_SECTOR_SIZE);
                  printf("\tIndirect block memory allocated\n");
                  if (sectors <= INDIR_BLOCK_SIZE) 
                    {
                      printf("Number of sectors <= DIR_BLOCK_SIZE + INDIR_BLOCK_SIZE\n");

                      // Write to indirect blocks
                      for (i = 0; i < sectors; i++)
                        block_write (fs_device, *disk_inode->indirect + i, zeros);
                      printf("\tIndirect blocks written successfully.\n");
                    }
                  else
                    {
                      printf("Number of sectors > DIR_BLOCK_SIZE + INDIR_BLOCK_SIZE\n");

                      // Write to indirect blocks
                      for (i = 0; i < INDIR_BLOCK_SIZE; i++)
                        block_write (fs_device, *(disk_inode->indirect) + i, zeros);
                      printf("\tIndirect blocks written successfully.\n");

                      sectors -= INDIR_BLOCK_SIZE;

                      size_t num_indir = DIV_ROUND_UP (sectors, INDIR_BLOCK_SIZE);
                      size_t j;
                      for (j = 0, i = 0; j < num_indir && i < sectors; j++)
                        {
                          // Allocate memory for doubly indirect blocks
                          block_sector_t *block = *disk_inode->doubly_indirect + j;
                          block = calloc (INDIR_BLOCK_SIZE, BLOCK_SECTOR_SIZE);
                          // Write to doubly indirect blocks
                          for (i = 0; i < sectors; i++)
                            block_write (fs_device, *(*disk_inode->doubly_indirect + j) + i, zeros);
                        }
                      printf("\tDoubly indirect blocks written successfully.\n");
                    }
                }

              printf("Wrote all blocks successfully.\n");
            }
          success = true; 
        } 
      free (disk_inode);
    }
  printf("success = %d\n", success);
  printf("...inode_create() successful\n");
  printf("==============================\n\n\n");
  return success;
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
  printf("inode.c:inode_read_at()...\n");

  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  // printf("inode.c:inode_read_at() : inode->data.length = %d\n", inode->data.length);
  while (size > 0) 
    {
      // printf("inode.c:inode_read_at() : size = %d\n", size);
      // printf("inode.c:inode_read_at() : offset = %d\n", offset);

      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;
      // printf("inode.c:inode_read_at() : sector_idx = %d\n", sector_idx);
      // printf("inode.c:inode_read_at() : sector_ofs = %d\n", sector_ofs);

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;
      // printf("inode.c:inode_read_at() : inode_left = %d\n", inode_left);
      // printf("inode.c:inode_read_at() : sector_left = %d\n", sector_left);
      // printf("inode.c:inode_read_at() : min_left = %d\n", min_left);

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      // printf("inode.c:inode_read_at() : chunk_size = %d\n", chunk_size);
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

  printf("inode.c:...inode_read_at()\n");

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
          block_write (fs_device, sector_idx, buffer + bytes_written);
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
          block_write (fs_device, sector_idx, bounce);
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
