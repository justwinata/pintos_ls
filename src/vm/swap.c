/******************************************************************************
 |   Assignment:  PintOS Part 1 - Virtual Memory: Swapping
 |
 |      Authors:  Chukwudi Iwueze, Katie Park, Justin Winata, Jesse Wright
 |     Language:  ANSI C99 (Ja?)
 |
 |        Class:  CS439
 |   Instructor:  Rellermeyer, Jan S.
 |
 +-----------------------------------------------------------------------------
 |
 |  Description:  Virtual memory, etc.
 |
 |    Algorithm:  Swapping with enhanced second-chance replacement algorithm
 |
 |   Required Features Not Included:  Swapping, pinning.
 |
 |   Known Bugs:  None.
 |
******************************************************************************/

/////////////////
//             //
//  Resources  //
//             //
/////////////////

//

/////////////
//         //
//  TO-DO  //
//         //
/////////////

/*
	TODO:
		Add:
			Swap space
			Pinning
*/

////////////////
//            //
//  Includes  //
//            //
////////////////

#include <stdio.h>
#include <string.h>
#include <hash.h>
#include "vm/page.h"
#include "filesys/file.h"
#include "kernel/malloc.h"
#include "kernel/synch.h"
#include "devices/block.h"
#include "lib/kernel/bitmap.h"
#include "kernel/vaddr.h"

#include "vm/swap.h"
#include "vm/frame.h"
#include "kernel/pte.h"
#include "kernel/thread.h"
#include "kernel/palloc.h"
#include "kernel/pagedir.h"

/////////////////////////
//                     //
//  Globale variables  //
//                     //
/////////////////////////

static struct bitmap *swap_table;
static struct block *swap_space;
static struct lock lock;
static const uint32_t PGS_PER_BLK = PGSIZE / BLOCK_SECTOR_SIZE;

///////////////
//           //
//  Structs  //
//           //
///////////////

//

//////////////////
//              //
//  Prototypes  //
//              //
//////////////////

//

/////////////////
//             //
//  Functions  //
//             //
/////////////////

/*
 * Function:  <name>
 * --------------------
 *	Does?
 */
void
st_init (void)
{
	lock_init (&lock);
	swap_space = block_get_role (BLOCK_SWAP);
	swap_table = bitmap_create (0);
}

/*
 * Function:  <name>
 * --------------------
 *	Does?
 */
void
swap_out (struct frame *frame, bool dirty)
{
	ASSERT (frame);

	lock_acquire (&lock);
		lock_acquire (&frame->thread->spt->lock_kva);
			struct page *page = page_lookup_kva (&frame->thread->spt->table_kva, frame->addr);
		lock_release (&frame->thread->spt->lock_kva);
													// ^This is the sole reason table_kva exists

		ASSERT (page != NULL);

		if (dirty || page->is_stack)	// Stack must always be written (at least for now) because swapping in and swapping out destroys swap data
		{
			if (!bitmap_any (swap_table, 0, bitmap_size (swap_table)))
			{
				if(bitmap_file_size (swap_table) * PGSIZE < block_size (block_get_role (BLOCK_SWAP)))
					bitmap_expand (swap_table);
				else
					PANIC ("Swap disk out of space.\n");
			}

			int32_t index = bitmap_scan (swap_table, 0, 1, 0);
			printf ("Found index: %d\n", index);

			printf ("Writing %p\n", frame->addr);

			uint32_t counter;
			for (counter = 0; counter < PGS_PER_BLK; counter++)
				block_write (swap_space,
							(index * PGS_PER_BLK) + counter,
							&frame->addr + (counter * BLOCK_SECTOR_SIZE));

			printf (">>> Reached near-end of swap out <<<\n");

			bitmap_mark (swap_table, index);
			page->swap_index = index;
		}

		deallocate_uframe (frame);

	lock_release (&lock);
}

/*
 * Function:  <name>
 * --------------------
 *	Does?
 */
void
swap_in (void *addr)
{
	ASSERT (addr);

	void *kpage = allocate_uframe (PAL_USER);

	ASSERT (kpage);

	lock_acquire (&lock);

		struct frame *frame = frame_lookup (addr);

		lock_acquire (&frame->thread->spt->lock_kva);
			struct page *page = page_lookup_kva (&frame->thread->spt->table_kva, frame->addr);
		lock_release (&frame->thread->spt->lock_kva);

		ASSERT (page != NULL);

		install_page (addr, kpage, page->writable);
		int32_t index = page->swap_index;

		uint32_t counter;
		for (counter = 0; counter < PGS_PER_BLK; counter++)
				block_write (swap_space,
							(index * PGS_PER_BLK) + counter,
							&frame->addr + (counter * BLOCK_SECTOR_SIZE));

		bitmap_reset (swap_table, page->swap_index);
		page->swap_index = -1;
	lock_release (&lock);
}

struct lock *
get_st_lock(void)
{
	return &lock;
}
