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
	hash_first (&hand, &frame_table);

	// Create and insert dummy frame for sake of hand_frame
	struct frame *frame = (struct frame *) malloc (sizeof (struct frame));
	frame->addr = 0x0;
	frame->pinned = true;
	frame->thread = thread_current ();
	hash_insert(&frame_table, &frame->hash_elem);

	hand_frame = hash_entry (hash_next(&hand), struct frame, hash_elem);

	hash_next (&hand);
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
	return hash_bytes (&frame->addr, sizeof frame->addr);
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
void*
allocate_uframe(enum palloc_flags flags)
{
	lock_acquire(&lock);
	
		void *addr = palloc_get_page (flags);

		if(addr == NULL)
		{
			evict_page ();
			printf ("\t>> Page evicted <<\n");
			addr = palloc_get_page (flags);
		}

		struct frame *frame = (struct frame *) malloc (sizeof (struct frame));
		frame->addr = addr;
		frame->pinned = false;
		frame->thread = thread_current ();

		if (!frame->thread)
			printf ("\n\tNull thread in frame %p\n\n", frame);
		
		hash_insert(&frame_table, &frame->hash_elem);
	
	lock_release(&lock);
	
	return addr;
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
	//TODO: Consider if free_frame option needed
	lock_acquire (&lock);

		struct frame *frame = frame_lookup(addr);

		if (!frame)
			return;

		palloc_free_page (frame->addr);
		hash_delete(&frame_table, &frame->hash_elem);
		free (frame);

	lock_release (&lock);
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
	/* Regain meaning for iterator in between hash-table modifications */
	hash_variable (&hand, &hand_frame->hash_elem);

	//print_ft ();

	/* TODO: Don't forget to add synchronization (and make sure Rellermeyer's
		warning in the PDF is accounted for)! */
	uint32_t *pd;
	bool accessed;
	bool dirty;

	bool found = false;

	while (!found)
	{
		if (!hand_frame->pinned)
		{
			if (!hash_find (&frame_table, &hand_frame->hash_elem))
				goto advance_ptr;

			pd = hand_frame->thread->pagedir;
			accessed = pagedir_is_accessed (pd, hand_frame->addr);
			dirty = pagedir_is_dirty (pd, hand_frame->addr);

			if (!accessed)
			{
				swap_out (hand_frame, dirty);
				found = true;
				printf ("Swap out compeleted.\n");
			}
			else
				pagedir_set_accessed (pd, hand_frame->addr, false);
		}

advance_ptr:
		if (!hash_next (&hand))
				hash_first (&hand, &frame_table);	//If reached end, start over

		hand_frame = hash_entry (hash_cur(&hand), struct frame, hash_elem);
	}
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
		printf ("Frame %p", frame);
		printf (" with thread %p\n", frame->thread);
	}
	printf ("==================================\n");
}

struct lock *
get_ft_lock(void)
{
	return &lock;
}
