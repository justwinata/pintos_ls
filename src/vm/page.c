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
 |  Description:  Virtual memory, etc.
 |
 |    Algorithm:  Hash table for SPT...
 |
 |   Required Features Not Included:  Eviction, swapping, etc.
 |
 |   Known Bugs:  None.
 |
******************************************************************************/

/////////////////
//             //
//  Resources  //
//             //
/////////////////

/*
	Supplemental Page Table: "Gives you information about the status of 
	 virtual memory (which addresses are logicially mapped, etc.)."
	 Courtesy https://groups.google.com/forum/#!topic/12au-cs140/5LDnfoOMdfY
*/

/*
	-Functionalities:
		>Your “s-page table” must be able to decide where to load executable 
			and which corresponding page of executable to load
		>Your “s-page table ” must be able to decide how to get swap disk and 
			which part (in sector) of swap disk stores the corresponding page
	-Implementation
		>Use hash table (recommend)
	-Usage
		>Rewrite load_segment() (in process.c) to populate s-page table 
			without loading pages into memory
		>Page fault handler then loads pages after consulting s-page table
	Courtesy http://courses.cs.vt.edu/cs3204/spring2007/pintos/Project3Session
		Spring2007.ppt (slide 12).
*/

/////////////
//         //
//  TO-DO  //
//         //
/////////////

/*
	TODO:
		Modify the following:
		---------------------------
		load_segment() in process.c
		page_fault() in exception.c
*/

////////////////
//            //
//  Includes  //
//            //
////////////////

#include <stdio.h>
#include <string.h>
#include "vm/page.h"
#include "filesys/file.h"
#include "kernel/malloc.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "kernel/vaddr.h"
#include "kernel/pte.h"
#include "kernel/thread.h"
#include "kernel/pagedir.h"
#include "kernel/interrupt.h"

/////////////////////////
//                     //
//  Globale variables  //
//                     //
/////////////////////////

/* Talk about the hash table used for the SPT a little bit. Make sure to 
	mention the key is the address of a page. */

///////////////
//           //
//  Structs  //
//           //
///////////////

/* struct page moved to page.h */

//////////////////
//              //
//  Prototypes  //
//              //
//////////////////

static unsigned page_hash (const struct hash_elem *, void *);
static unsigned page_hash_kva (const struct hash_elem *, void *);
static bool page_less (const struct hash_elem *, const struct hash_elem *, void *);
static bool page_less_kva (const struct hash_elem *, const struct hash_elem *, void *);
static void page_destructor (struct hash_elem *, void *);
static void page_destructor_kva (struct hash_elem *, void *);

/////////////////
//             //
//  Functions  //
//             //
/////////////////

/*
 * Function:  spt_init
 * --------------------
 *	Initializes the hash table for the supplemental page table using hash_init 
 *		and also initializes the lock for the SPT's hash table.
 */
struct spt *
spt_create (void) 
{
	struct spt *spt = (struct spt *) malloc (sizeof (struct spt));
	hash_init (&spt->table, page_hash, page_less, NULL);
	hash_init (&spt->table_kva, page_hash_kva, page_less_kva, NULL);
	lock_init (&spt->lock);
	lock_init (&spt->lock_kva);
	return spt;
}

void
spt_destroy (struct spt *spt) 
{
	lock_acquire (&spt->lock);
	lock_acquire (&spt->lock_kva);
		hash_destroy (&spt->table, page_destructor);
		hash_destroy (&spt->table_kva, page_destructor_kva);
		free (spt);
	// lock_release not needed (locks are freed)
}

/*
 * Function:  add_page 
 * --------------------
 *	Adds a page entry to the supplemental page table to keep track of the page's
 *		information such as when a page is allocated. Acquires the SPT's lock 
 *		before creating the hash element for the and page structures to wrap 
 *		the given address.
 *
 *  addr: the virtual address for a give page
 *
 *  returns: the created page struct that contains the corresponding hash_elem 
 *		struct for the SPT hash table
 */
struct page *
add_page (struct spt *spt, void *addr)
{
	lock_acquire (&spt->lock);
	    struct page *page = (struct page *) malloc (sizeof (struct page));
	    page->addr = addr;
	    page->pd = thread_current()->pagedir;
	    page->references = 0;
	    hash_insert (&spt->table, &page->hash_elem);
    lock_release (&spt->lock);

    return page;
}

/*
 * Function:  remove_page 
 * --------------------
 *	Removes a page entry from the supplemental page table, such as when a page 
 *		is deallocated (see end of setup_stack function in process.c for an 
 *		example). 
 *
 *  page: the page to be removed from the SPT
 */
void
remove_page (struct spt *spt, struct page *page)
{
	lock_acquire (&spt->lock);
		free (hash_delete (&spt->table, &page->hash_elem));
		free (hash_delete (&spt->table_kva, &page->hash_elem_kva));
	lock_release (&spt->lock);
}

/*
 * Function:  page_hash 
 * --------------------
 *	Hashes a page's address for use by the SPT's hash_table, especially for 
 *		insertions, look ups, and deletions.
 *
 *  spt_elem: a pointer to a hash_elem struct that's associated with a page 
 *		and used to insert that page into the SPT's hash table
 *	aux: an unused parameter that comes as part of the hash_table page_hash 
 *		function signature
 *
 *  returns: a hash value for a page
 */
static unsigned
page_hash (const struct hash_elem *spt_elem, void *aux UNUSED)
{
	const struct page *page = hash_entry (spt_elem, struct page, hash_elem);
	return hash_int ((int) page->addr);
}

static unsigned
page_hash_kva (const struct hash_elem *spt_elem_kva, void *aux UNUSED)
{
	const struct page *page = hash_entry (spt_elem_kva, struct page, hash_elem_kva);
	return hash_int ((int) page->kv_addr);
}

/*
 * Function:  page_less
 * --------------------
 *	Compares to hash_element structs and determines which element's associated 
 *		entry in the SPT's hash table is "less." "Less" for SPT entries is 
 *		defined as having a lower addr member within the page struct.
 *
 *  first: 	the first hash_elem to be compared
 *	second: the second hash_elem to be compared
 *	aux: 	an unused parameter that comes as part of the hash_table page_less 
 *				function signature
 *
 *  returns: a boolean value of whether a is less than b or nott
 */
static bool
page_less (const struct hash_elem *first, const struct hash_elem *second, void *aux UNUSED)
{
	const struct page *a = hash_entry (first, struct page, hash_elem);
	const struct page *b = hash_entry (second, struct page, hash_elem);
	return a->addr < b->addr;
}

static bool
page_less_kva (const struct hash_elem *first, const struct hash_elem *second, void *aux UNUSED)
{
	const struct page *a = hash_entry (first, struct page, hash_elem_kva);
	const struct page *b = hash_entry (second, struct page, hash_elem_kva);
	return a->kv_addr < b->kv_addr;
}

/*
 * Function:  <name>
 * --------------------
 *	Does?
 */
void
page_destructor (struct hash_elem *elem, void *aux UNUSED)
{
	free (hash_entry (elem, struct page, hash_elem));
}

void
page_destructor_kva (struct hash_elem *elem, void *aux UNUSED)
{
	free (hash_entry (elem, struct page, hash_elem_kva));
}

/*
 * Function:  page_lookup
 * --------------------
 *	Looks up an address in the SPT's hash table using the hash_find function.
 *
 *  addr: the address to look for
 *
 *  returns: the page with the given address if found in the table or null if not
 */
struct page*
page_lookup (struct hash *table, void *addr)
{
	struct page page;
	struct hash_elem *elem;
	page.addr = addr;
	elem = hash_find (table, &page.hash_elem);
	return elem != NULL ? hash_entry (elem, struct page, hash_elem) : NULL;
}

struct page*
page_lookup_kva (struct hash *table, void *kv_addr)
{
	struct page page;
	struct hash_elem *elem;
	page.kv_addr = kv_addr;
	elem = hash_find (table, &page.hash_elem_kva);
	return elem != NULL ? hash_entry (elem, struct page, hash_elem_kva) : NULL;
}

/*
 * Function:  load_page 
 * --------------------
 *  Loads pages as needed when a process page faults on memory it should have 
 *    have access to but has not been loaded (lazy loading). Calculates  using 
 *    the file from which the pages for the process are laoded (stored by 
 *    pointer in each page entry in the SPT) and loads the page-sized section 
 *    of the file. The faulting process will return to the statement on which 
 *    it page faulted and resume with that same instruction, which should then 
 *    continue without another page fault.
 *
 *  addr: the address on which the halted process faulted
 */
bool
load_page (struct spt *spt, void *addr)
{
  /* Calculate how to fill this page.
     We will read PAGE_READ_BYTES bytes from FILE
     and zero the final PAGE_ZERO_BYTES bytes. */
  
    struct page *page;
	
	lock_acquire (&spt->lock);
		page = page_lookup (&spt->table, addr);
	lock_release (&spt->lock);

	/* Note: a file might be null for a given page; if so, this means it is not 
	 a part of the executable section of a process. In that case, it would 
	 also have a null offset (ofs). */

	if(page == NULL || page->swap_index >= 0) // Page is null or swapped - null is bad
	{
		//TODO: Implement swapping and stuff
		return false; //Fail for now :(    --    Could also panic kernel
	}

	/* Get a page of memory. */
	//Should virtual addresses be contiguous for a process? Are they already?
	uint8_t *kpage = allocate_uframe (PAL_USER); //palloc_get_page (PAL_USER);
	if (kpage == NULL)
		return false;

	/* Load this page. */
	file_seek (page->file, page->ofs);
	if (file_read (page->file, kpage, page->read_bytes) != (int) page->read_bytes)
	{
		deallocate_uframe (kpage);
		return false; 
	}

	memset (kpage + page->read_bytes, 0, page->zero_bytes);

	/* Add the page to the process's address space. */
	if (!install_page (page->addr, kpage, page->writable)) 
	{
		deallocate_uframe (kpage);
		return false; 
	}

	/* Mark the page as loaded and set kv_addr */
	page->loaded = true;
	set_page_kva (spt, page, kpage);

	return true;
}


/* Copied directly from process.c for use by exception.c in load_page */
/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */

bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

bool
is_writable_buffer (char ** buffer, unsigned size)
{
	void *begin = pg_round_down (buffer);
	void *end = pg_round_up (buffer + size);

	for(; begin < end; begin += PGSIZE)
		if (!page_lookup (&thread_current()->spt->table, pg_round_down (buffer))->writable)
			return false;
	return true;
}

void
reclaim_pages (struct thread *thread)
{
	struct spt *spt = thread->spt;

	lock_acquire (&spt->lock);

		struct page *current;
		struct hash_iterator hand;
		hash_first (&hand, &spt->table);

		while (hash_next (&hand))
		{
			current = hash_entry (hash_cur (&hand), struct page, hash_elem);
			if(current->loaded)
				deallocate_uframe (current->kv_addr);
		}

	lock_release (&spt->lock);

	spt_destroy (spt);
}

/*
 * Function:  print_page
 * --------------------
 *	Does?
 *
 *  params...
 */
void
print_page (struct page *page)
{
	//TODO: Figure out how to get PD
	printf("Page:\t\t%p\nAddress:\t%p\nKV address:\t%p"
		"\n-----------------------------------"
		"\n\tPage directory:\t%p"
		"\n\tLoaded:\t\t%s"
		"\n\tResident:\t%s"
		"\n\tSwap index:\t%d"
		"\n\tAccessed:\t%s"
		"\n\tDirty:\t\t%s"
		"\n\tReferences:\t%d"
		"\n\tStack:\t\t%s"
		"\n\tBytes to zero:\t%d"
		"\n\tBytes to read:\t%d"
		"\n\tNumber:\t\t%d"
		"\n\tSize:\t\t%d"
		"\n\tProcess:\t%p"
		"\n\tWritable:\t%s"
		"\n\tFile:\t\t%p"
		"\n\tFile offset:\t%d"
		"\n\tHash element:\t%p"
		"\n\n",
		page,
		page->addr,
		page->kv_addr,
		page->pd,
		page->loaded ? "True" : "False",
		((uint32_t) page & PTE_P) ? "True" : "False",
		page->swap_index,
		pagedir_is_accessed (page->pd, page->kv_addr) ? "True" : "False",
		pagedir_is_dirty (page->pd, page->kv_addr) ? "True" : "False",
		page->references,
		page->is_stack ? "True" : "False",
		page->zero_bytes,
		page->read_bytes,
		page->number,
		page->size,
		page->proc_addr,
		page->writable ? "True" : "False",
		page->file,
		page->ofs,
		&page->hash_elem);
}

void
print_spt (struct spt *spt)
{
	struct hash_iterator hand;
	hash_first (&hand, &spt->table);

	printf ("=============== SPT ===============\n");
	while (hash_next (&hand))
		//printf ("%p\n", hash_entry (hash_cur (&hand), struct page, hash_elem));
		print_page (hash_entry (hash_cur (&hand), struct page, hash_elem));
	printf ("===================================\n");
}

void
set_page (struct page *page,
			bool loaded,
			int16_t swap_index,
			uint8_t references,
			bool is_stack,
			int16_t zero_bytes,
			int16_t read_bytes,
			uint32_t number,
			uint32_t size,
			void *proc_addr,
			bool writable,
			struct file *file,
			off_t ofs)
{
	page->loaded = loaded;
	page->swap_index = swap_index;
	page->references = references;
	page->is_stack = is_stack;
	page->zero_bytes = zero_bytes;
	page->read_bytes = read_bytes;
	page->number = number;
	page->size = size;
	page->proc_addr = proc_addr;
	page->writable = writable;
	page->file = file;
	page->ofs = ofs;
}

void
set_page_kva (struct spt *spt, struct page *page, void *kva)
{
	page->kv_addr = kva;

	lock_acquire (&spt->lock_kva);
		hash_insert (&spt->table_kva, &page->hash_elem_kva);
	lock_release (&spt->lock_kva);
}
