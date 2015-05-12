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
 |    Algorithm:  Swapping with second-chance replacement algorithm
 |
******************************************************************************/

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

/////////////////
//             //
//  Functions  //
//             //
/////////////////

void
st_init (void)
{
	lock_init (&lock);
}

void
st_init_swap_space (void)
{
	swap_space = block_get_role (BLOCK_SWAP);
	swap_table = bitmap_create (block_size (swap_space));
}

void
delete_from_swap (uint32_t index)
{
	ASSERT (index < bitmap_size (swap_table));
	bitmap_reset (swap_table, index);
}

bool
swap_out (struct frame *frame, bool dirty)
{
	ASSERT (frame);

	lock_acquire (&lock);
		lock_acquire (&frame->thread->spt->lock_kva);
			struct page *page = page_lookup_kva (&frame->thread->spt->table_kva, frame->addr);
		lock_release (&frame->thread->spt->lock_kva);
		ASSERT (page);

		page->pinned = frame->pinned;

		if (dirty || page->is_stack)
		{
			int index = bitmap_scan (swap_table, 0, 1, 0);

			if (index == -1)
				PANIC ("Swap disk out of space.\n");

			size_t counter;
			for (counter = 0; counter < PGS_PER_BLK; counter++)
				block_write (swap_space,
							(index * PGS_PER_BLK) + counter,
							page->addr + (counter * BLOCK_SECTOR_SIZE));

			bitmap_mark (swap_table, index);
			page->swap_index = index;
		}
		pagedir_clear_page (frame->thread->pagedir, page->addr);
	lock_release (&lock);

	return true;
}

void
swap_in (void *addr)
{
	struct frame *frame = allocate_uframe (PAL_USER);
	ASSERT (frame);

	lock_acquire (&lock);
		lock_acquire (&frame->thread->spt->lock);
			struct page *page = page_lookup (&frame->thread->spt->table, addr);
		lock_release (&frame->thread->spt->lock);
		ASSERT (page);

		frame->pinned = page->pinned;
		install_page (addr, frame->addr, page->writable);

		int32_t index = page->swap_index;
		ASSERT (index > -1);

		uint32_t counter;
		for (counter = 0; counter < PGS_PER_BLK; counter++)
				block_read (swap_space,
							(index * PGS_PER_BLK) + counter,
							page->addr + (counter * BLOCK_SECTOR_SIZE));

		bitmap_reset (swap_table, page->swap_index);
		page->swap_index = -1;
	lock_release (&lock);
}

struct lock *
get_st_lock(void)
{
	return &lock;
}
