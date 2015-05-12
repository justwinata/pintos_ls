/******************************************************************************
 |   Assignment:  PintOS Part 1 - Virtual Memory
 |
 |      Authors:  Chukwudi Iwueze, Katie Park, Justin Winata, Jesse Wright
 |     Language:  ANSI C99 (Ja?)
 |
 |        Class:  CS439
 |   Instructor:  Rellermeyer, Jan S.
 |
 +-----------------------------------------------------------------------------
 |
 |  Description:  Vritual memory, etc.
 |
 |    Algorithm:  Hash table for SPT...
 |
 |   Required Features Not Included:  Eviction, swapping, etc.
 |
 |   Known Bugs:  None.
 |
*******************************************************************************/

/////////////////
//             //
//  Resources  //
//             //
/////////////////

/////////////
//         //
//  TO-DO  //
//         //
/////////////
 
/*
	TODO:
		Figure out when you know you've run out of frames
*/

////////////////
//            //
//  Includes  //
//            //
////////////////

#include <stdio.h>
#include "kernel/synch.h"
#include "kernel/malloc.h"
#include "kernel/thread.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "kernel/pagedir.h"

#include "kernel/vaddr.h"
#include "vm/page.h"

/////////////////////////
//                     //
//  Globale variables  //
//                     //
/////////////////////////

static struct hash frame_table;		/* Frame Table */
static struct lock lock;			/* Lock for synchronization of frame table */
static struct hash_iterator hand;	/* Iterator for use as clock hand in ESCRA */
static struct frame *hand_frame;	/* Hand's current frame for the sake of hash_variable */

///////////////
//           //
//  Structs  //
//           //
///////////////

//struct frame moved to frame.h

//////////////////
//              //
//  Prototypes  //
//              //
//////////////////

unsigned frame_hash (const struct hash_elem *, void *);
bool frame_less (const struct hash_elem *, const struct hash_elem *, void *);

/////////////////
//             //
//  Functions  //
//             //
/////////////////

/*
 * Function:  <function_name> 
 * --------------------
 *	<function description>
 *
 *  <parameter one>: <parameter description>
 *
 *  returns: <return description> 
 */
void
ft_init()
{
	hash_init (&frame_table, frame_hash, frame_less, NULL);

	// Create and insert dummy frame for sake of hand_frame
	struct frame *frame = (struct frame *) malloc (sizeof (struct frame));
	frame->addr = 0x0;
	frame->pinned = true;
	frame->thread = thread_current ();
	hash_insert(&frame_table, &frame->hash_elem);

	// Must come after any frame-table modifications
	hash_first (&hand, &frame_table);

	// Call hash_next on hand and set hand_frame to get clock ready
	hand_frame = hash_entry (hash_next (&hand), struct frame, hash_elem);

	lock_init (&lock);
}

/*
 * Function:  <function_name> 
 * --------------------
 *	<function description>
 *
 *  <parameter one>: <parameter description>
 *
 *  returns: <return description> 
 */
unsigned
frame_hash (const struct hash_elem *ft_elem, void *aux UNUSED)
{
	const struct frame *frame = hash_entry (ft_elem, struct frame, hash_elem);
	return hash_int ((int) frame->addr);
}

/*
 * Function:  <function_name> 
 * --------------------
 *	<function description>
 *
 *  <parameter one>: <parameter description>
 *
 *  returns: <return description> 
 */
bool
frame_less (const struct hash_elem *first, const struct hash_elem *second, void *aux UNUSED)
{
	const struct frame *a = hash_entry (first, struct frame, hash_elem);
	const struct frame *b = hash_entry (second, struct frame, hash_elem);
	return a->addr < b->addr;
}

/*
 * Function:  <function_name> 
 * --------------------
 *	<function description>
 *
 *  <parameter one>: <parameter description>
 *
 *  returns: <return description> 
 */
struct frame*
frame_lookup (void *address)
{
	struct frame f;
	struct hash_elem *e;
	f.addr = address;
	e = hash_find (&frame_table, &f.hash_elem);
	return e != NULL ? hash_entry (e, struct frame, hash_elem) : NULL;
}

/*
 * Function:  <function_name> 
 * --------------------
 *	<function description>
 *
 *  <parameter one>: <parameter description>
 *
 *  returns: <return description> 
 */
struct frame *
allocate_uframe(enum palloc_flags flags)
{
	void *addr = palloc_get_page (flags);

	if (!addr)
	{
		evict_page ();
		addr = palloc_get_page (flags);
	}

	if (!addr)
		return addr;

	struct frame *frame = (struct frame *) malloc (sizeof (struct frame));
	frame->addr = addr;
	frame->pinned = false;
	frame->thread = thread_current ();
	
	lock_acquire(&lock);
		hash_insert(&frame_table, &frame->hash_elem);
	lock_release(&lock);
	
	return frame;
}

/*
 * Function:  <function_name> 
 * --------------------
 *	<function description>
 *
 *  <parameter one>: <parameter description>
 *
 *  returns: <return description> 
 */
void
deallocate_uframe (void *addr)
{
	lock_acquire (&lock);
		struct frame *frame = frame_lookup(addr);
	lock_release (&lock);

	if (!frame)
		return;
	
	deallocate_uframe_f (frame);
}

void
deallocate_uframe_f (struct frame *frame)
{
	palloc_free_page (frame->addr);

	lock_acquire (&lock);
		hash_delete(&frame_table, &frame->hash_elem);
	lock_release (&lock);

	free (frame);
}

/*
 * Function:  evict_page
 * --------------------
 *	Does? Enhanced second-chance clock replacement
 *
 *  params...
 *
 */

void
evict_page (void)
{
	lock_acquire (&lock);
		hash_variable (&hand);

		struct frame *victim;
		uint32_t *pd;
		bool accessed;
		bool dirty;

		bool found = false;

		while (!found)
		{
			if (!hand_frame->pinned && hash_find (&frame_table, &hand_frame->hash_elem))
			{
				pd = hand_frame->thread->pagedir;
				accessed = pagedir_is_accessed (pd, hand_frame->addr);
				dirty = pagedir_is_dirty (pd, hand_frame->addr);

				if (!accessed)
					found = swap_out (hand_frame, dirty);
				else
					pagedir_set_accessed (pd, hand_frame->addr, false);
			}

			if (!hash_next (&hand))
					hash_first (&hand, &frame_table);

			victim = hand_frame;
			hand_frame = hash_entry (hash_cur(&hand), struct frame, hash_elem);
		}
	lock_release (&lock);
	deallocate_uframe_f (victim);
}

void
print_ft (void)
{
	struct hash_iterator hand;
	hash_first (&hand, &frame_table);
	struct frame *frame;

	printf ("\n=============== FT ===============\n");
	while (hash_next (&hand))
	{
		frame = hash_entry (hash_cur (&hand), struct frame, hash_elem);
		printf ("Frame address %p", frame->addr);
		printf (" with hash elem %p", &frame->hash_elem);
		printf (" and addr %p\n", frame->addr);
	}
	printf ("==================================\n");
}

struct lock *
get_ft_lock(void)
{
	return &lock;
}
