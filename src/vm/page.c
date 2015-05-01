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
#include <hash.h>
#include "vm/page.h"
#include "filesys/file.h"
#include "kernel/malloc.h"
#include "kernel/synch.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "kernel/pte.h"
#include "kernel/thread.h"
#include "kernel/pagedir.h"	//For eviction (accessed- and dirty-bit functions)

/////////////////////////
//                     //
//  Globale variables  //
//                     //
/////////////////////////

/* Talk about the hash table used for the SPT a little bit. Make sure to 
	mention the key is the address of a page. */
static struct hash spt;				/* Supplemental Page Table */
static struct lock lock;			/* Lock for synchronization of SPT */
static struct hash_iterator hand;	/* Iterator for use as clock hand in ESCRA */

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

unsigned page_hash (const struct hash_elem *, void *);
bool page_less (const struct hash_elem *, const struct hash_elem *, void *);
void page_destructor (struct hash_elem *, void *);
//static bool install_page (void *, void *, bool);

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
void
spt_init (void) 
{
	printf("Calling spt_init...\n"); 
	lock_init (&lock);
	hash_init (&spt, page_hash, page_less, NULL);
	hash_first (&hand, &spt);
	printf("spt_init successful: %p\n", &spt); 
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
add_page (void *addr)
{
	printf("Calling add_page for addr %p\n", addr);
	lock_acquire (&lock);
	printf ("Lock acquired in add_page for addr %p\n", addr);
    
    struct hash_elem *elem = (struct hash_elem *) malloc (sizeof (struct hash_elem));
    struct page *page = hash_entry (elem, struct page, hash_elem);
    page->addr = addr;
    page->pd = thread_current()->pagedir;
    hash_insert (&spt, &page->hash_elem);

    lock_release (&lock);
    printf ("Lock released in add_page for page addr %p\n", page);
    printf ("add_page successful for addr %p in page %p\n", addr, page);
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
remove_page (struct page *page)
{
	printf ("Calling remvoe_page for %p\n", page);
	lock_acquire (&lock);
	printf ("Lock acquired in release_page for page addr %p\n", page);
	struct hash_elem *e = hash_delete (&spt, &page->hash_elem);
	lock_release (&lock);
	printf("lock released in remove_page for page addr %p\n", page);
	free (page);
	free (e); //TODO: Figure out if this is needed
	printf ("remove_page successful for page %p\n", page);
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
unsigned
page_hash (const struct hash_elem *spt_elem, void *aux UNUSED)
{
	const struct page *page = hash_entry (spt_elem, struct page, hash_elem);
	return hash_bytes (&page->addr, sizeof page->addr);
}

/*
 * Function:  page_less
 * --------------------
 *	Compares to hash_element structs and determines which element's associated 
 *		entry in the SPT's hash table is "less." "Less" for SPT entries is 
 *		defined as having a lower addr member within the page struct.
 *
 *  first: the first hash_elem to be compared
 *	second: the second hash_elem to be compared
 *	aux: an unused parameter that comes as part of the hash_table page_less 
 *		function signature
 *
 *  returns: a boolean value of whether a is less than b or not
 */
bool
page_less (const struct hash_elem *first, const struct hash_elem *second, void *aux UNUSED)
{
	const struct page *a = hash_entry (first, struct page, hash_elem);
	const struct page *b = hash_entry (second, struct page, hash_elem);
	return a->addr < b->addr;
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
page_lookup (void *addr)
{
	struct page p;
	struct hash_elem *e;
	p.addr = addr;
	e = hash_find (&spt, &p.hash_elem);
	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

void
page_destructor (struct hash_elem *e, void *aux UNUSED)
{
	free (hash_entry (e, struct page, hash_elem));	//Hope that's all...
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
load_page (void *addr)
{
  /* Calculate how to fill this page.
     We will read PAGE_READ_BYTES bytes from FILE
     and zero the final PAGE_ZERO_BYTES bytes. */

  struct page *page = page_lookup (addr);

  /* Note: a file might be null for a given page; if so, this means it is not 
     a part of the executable section of a process. In that case, it would 
     also have a null offset (ofs). */

  if(!page || page->swapped) // Page is null or swapped - null is bad
  {
    //TODO: Implement swapping and stuff
    return false; //Faile for now :(    --    Could also panic kernel
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

  /* Mark the page as loaded */
  page->loaded = true;

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

static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

/*
 * Function:  evict_page
 * --------------------
 *	Does? Enhanced second-chance clock replacement
 *
 *  params...
 *
 */
static void
evict_page (void)
{
	//TODO: Don't forget to add synchronization (and make sure Rellermeyer's warning in the PDF is accounted for)!
	bool found = false;

	while (!found)
	{
		struct page *page = hash_entry (hash_cur (&hand), struct page, hash_elem);
		uint32_t *pd = page->pd;
		bool accessed = pagedir_is_accessed (pd, page->addr);
		bool dirty = pagedir_is_dirty (pd, page->addr);

		lock_acquire(&lock);

		if (!(accessed || dirty))
		{
			pagedir_clear_page (pd, page);
			found = true;
		}
		else if (!accessed && dirty)
		{
			pagedir_clear_page (pd, page);
			//write back;			//TODO: Write write-back method
			found = true;
		}
		else
			pagedir_set_accessed (pd, page->addr, false);

		lock_release(&lock);

		if (!hash_next (&hand))	//Advance pointer
				hash_first (&hand, &spt);	//If reached end, start over
	}
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
	printf("Page:\t%p"
		"\n\tPage Directory:\t%p"
		"\n\tLoaded:\t%s"
		"\n\tSwapped:\t%s"
		"\n\tAccessed:\t%s"
		"\n\tDirty:\t%s"
		"\n\tReferences:\t%d"
		"\n\tStack:\t%s"
		"\n\tBytes to zero:\t%d"
		"\n\tBytes to read:\t%d"
		"\n\tNumber:\t%d"
		"\n\tSize:\t%d"
		"\n\tProcess:\t%p"
		"\n\tWritable:\t%s"
		"\n\tFile:\t%p"
		"\n\tFile offset:\t%d"
		"\n\tHash element:\t%p"
		"\n",
		page->addr,
		page->pd,
		page->loaded ? "True" : "False",
		pagedir_is_accessed (page->pd, page) ? "True" : "False",
		pagedir_is_dirty (page->pd, page) ? "True" : "False",
		page->swapped ? "True" : "False",
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
