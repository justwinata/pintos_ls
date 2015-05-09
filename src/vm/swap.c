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
#include "vm/page.h"
#include "vm/frame.h"
#include "kernel/pte.h"
#include "kernel/thread.h"

/////////////////////////
//                     //
//  Globale variables  //
//                     //
/////////////////////////

static struct bitmap *swap_table;
static struct block *swap_space;
static struct lock lock;

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
	printf("Calling st_init...\n");
	lock_init (&lock);
	swap_space = block_get_role (BLOCK_SWAP);
	swap_table = bitmap_create (0);
	printf("st_init successful: %p\n", &swap_table); 
}

/*
 * Function:  <name>
 * --------------------
 *	Does?
 */
void
swap_out (struct page *page)
{
	if (page == NULL)
	{
		PANIC ("Null page in swap_out in swap.c.\n");
	}

	if (!bitmap_any (swap_table, 0, bitmap_size (swap_table)))
	{
		if (bitmap_file_size (swap_table) == BLOCK_SECTOR_SIZE * bitmap_size (swap_table))
			PANIC ("Out of swap space! Page %p does not fit!\n", page);
		bitmap_expand (swap_table);
	}

	// Start at 0, looking for a size-1 group of 0-valued bits
	int32_t index = bitmap_scan (swap_table, 0, 1, 0);

	if (index < 0)
		PANIC ("Failed to swap out page\t%p! No space in swap table!\n", page);

	uint32_t counter;
	for (counter = 0; counter < PGSIZE / BLOCK_SECTOR_SIZE; counter++)
		block_write (swap_space, index + counter, page->addr + counter * sizeof (void *));

	bitmap_mark (swap_table, index);
	page->swap_index = index;
}

/*
 * Function:  <name>
 * --------------------
 *	Does?
 */
void
swap_in (struct page *page)
{
	/* Still need to consider when another page was loaded in current page's
		place. i.e., [a b c] : swap out a with d -> [d b c] -> swap back in a
		-> swap out d -> [a b c] -- is it a problem if d is overwritten? Dirty
		bit? (YES to all - TODO: !!!) */

	if ((int) bitmap_size (swap_table) < page->swap_index)
		PANIC ("Failed to swap in page %p! Page not found in swap table!\n", page);

	uint32_t counter;
	for (counter = 0; counter < PGSIZE / BLOCK_SECTOR_SIZE; counter++)
		block_read (swap_space, page->swap_index + counter, page->addr + counter * sizeof (void *));

	bitmap_reset (swap_table, page->swap_index);
	page->swap_index = -1;
}
