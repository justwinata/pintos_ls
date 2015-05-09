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
#include <hash.h>
#include "kernel/synch.h"
#include "kernel/malloc.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "kernel/pagedir.h"

#include "kernel/vaddr.h"

/////////////////////////
//                     //
//  Globale variables  //
//                     //
/////////////////////////

static struct hash frame_table;	/* Frame Table */
static struct lock lock;		/* Lock for synchronization of frame table */

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
struct frame* frame_lookup (void *);

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
			swap_out (evict_page ());

		struct frame *frame = (struct frame *) malloc (sizeof (struct frame));
		frame->addr = addr;
		
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
deallocate_uframe(void *addr, bool palloc_free)
{
	//TODO: Consider if free_frame option needed
	lock_acquire (&lock);

		struct frame *frame = frame_lookup(addr);

		if (!frame)
			return;

		ASSERT (frame);

		if (palloc_free)
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
struct frame*
evict_page (void)
{
	static struct hash_iterator hand;	/* Iterator for use as clock hand in ESCRA */
	hash_first (&hand, &frame_table);
	hash_next (&hand);

	//TODO: Hand is invalidated upon any modification of the SPT!!! D: Fix!
	//TODO: Don't forget to add synchronization (and make sure Rellermeyer's warning in the PDF is accounted for)!
	struct frame *frame;
	uint32_t *pd;
	bool accessed;
	bool dirty;

	bool found = false;

	while (!found)
	{
		// frame = hash_entry (hash_cur (&hand), struct frame, hash_elem);
		// pd = frame->pagedir;
		// accessed = pagedir_is_accessed (pd, frame->addr);
		// dirty = pagedir_is_dirty (pd, frame->addr);

		// //frame->references - 1 != 0? Subtract one from references and don't clear frame?

		// if (!(accessed || dirty))
		// {
		// 	pagedir_clear_page (pd, frame);
		// 	found = true;
		// }
		// else if (!accessed && dirty)
		// {
		// 	PANIC ("Failed to evict frame %p! No write back method available!\n", frame);
		// 	pagedir_clear_page (pd, frame);
		// 	//write back;			//TODO: Write write-back method
		// 	found = true;
		// }
		// else
		// 	pagedir_set_accessed (pd, frame->addr, false);

		// if (!hash_next (&hand))	//Advance pointer
		// 		hash_first (&hand, &spt);	//If reached end, start over
	}

	return frame;
}
