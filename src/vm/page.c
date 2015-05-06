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
#include "kernel/interrupt.h"

#include <list.h>

/////////////////////////
//                     //
//  Globale variables  //
//                     //
/////////////////////////

/* Talk about the hash table used for the SPT a little bit. Make sure to 
	mention the key is the address of a page. */
// static struct hash spt;				/* Supplemental Page Table */
// static struct lock lock;			/* Lock for synchronization of SPT */
// static struct hash_iterator hand;	/* Iterator for use as clock hand in ESCRA */

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

// unsigned spt_page_hash (const struct hash_elem *, void *);
// bool spt_page_less (const struct hash_elem *, const struct hash_elem *, void *);
// void spt_page_destructor (struct hash_elem *, void *);

/////////////////
//             //
//  Functions  //
//             //
/////////////////

// NOT USED vvv
struct hash *
spt_create (void)
{
	printf("Calling spt_create...\n");
	struct hash *spt = (struct hash *) malloc (sizeof (struct hash));
	hash_init (spt, spt_page_hash, spt_page_less, NULL);
	// hash_clear (&spt, page_destructor);
	// hash_first (&hand, &spt);
	printf("spt_create successful: %p\n", spt);
	return spt;
}
// ^^^ NOT USED

void 
spt_destroy (struct hash *spt)
{
	printf("Calling spt_destroy...\n");
	hash_destroy(spt, spt_page_destructor);
	printf("spt_destroy successful.\n");
}

/*
 * Function:  spt_init
 * --------------------
 *	Initializes the hash table for the supplemental page table using hash_init 
 *		and also initializes the lock for the SPT's hash table.
 */
// void
// spt_init (void) 
// {
// 	printf("Calling spt_init...\n"); 
// 	lock_init (&lock);
// 	hash_init (&spt, page_hash, page_less, NULL);
// 	// hash_clear (&spt, page_destructor);
// 	hash_first (&hand, &spt);
// 	printf("spt_init successful: %p\n", &spt); 
// }

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
spt_add_page (struct hash *spt, void *addr)
{
	printf("\n\n>>>>>>>> ======================================\n");


	struct list_elem *elem2;
	elem2 = (struct list_elem *) malloc (sizeof (struct list_elem));
	elem2 = (struct list_elem *) malloc (sizeof (struct list_elem));
	elem2 = (struct list_elem *) malloc (sizeof (struct list_elem));

	// struct list l;
	// // l = (struct list *) malloc(sizeof (struct list));
	// list_init(&l);

	// struct list_elem elem2, elem3, elem4;
	// list_push_back(&l, &elem2);
	// list_push_back(&l, &elem3);
	// list_push_back(&l, &elem4);
	// int x;
	// for (x = 0; x < 3; ++x) {
	// 	elem2 = (struct list_elem *) malloc (sizeof (struct list_elem));
	// 	list_push_back(&l, elem2);
	// }

	// for (x = 0; x < 3; ++x) {
	// 	elem2 = list_pop_front(&l);
	// 	free(elem2);
	// }

	// struct list_elem *e;
	// for (e = list_begin (&l); e != list_end (&l); e = list_next (e))
	// {
	// 	// struct foo *f = list_entry (e, struct list_emel, elem);
	// 	printf("test_list: %p\n", e);
	// }





	printf("Calling add_page for addr %p\n", addr);

	// lock_acquire (&lock);
	// printf ("Lock acquired in add_page for addr %p\n", addr);

	printf("\nOLD SUPPLEMENTAL PAGE TABLE::\n");
	spt_print_spt(spt);
	printf("::END\n\n");
    
    struct hash_elem *elem = (struct hash_elem *) malloc (sizeof (struct hash_elem));
    struct page *page = hash_entry (elem, struct page, hash_elem);
    page->addr = addr;
    page->pd = thread_current()->pagedir;
    page->references = 0;
    hash_insert (spt, &page->hash_elem);

    printf("\nNEW SUPPLEMENTAL PAGE TABLE::\n");
    spt_print_spt(spt);
    printf("::END\n\n");

    // lock_release (&lock);
    // printf ("Lock released in add_page for page addr %p\n", page);
    
    printf ("add_page successful for addr %p in page %p in pagedir %p\n", addr, page, page->pd);
    printf("====================================== <<<<<<<<<\n");
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
spt_remove_page (struct hash *spt, struct page *page)
{
	printf ("Calling remvoe_page for %p\n", page);
	
	// lock_acquire (&lock);
	// printf ("Lock acquired in release_page for page addr %p\n", page);
	
	struct hash_elem *e = hash_delete (spt, &page->hash_elem);
	
	// lock_release (&lock);
	// printf("lock released in remove_page for page addr %p\n", page);
	
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
spt_page_hash (const struct hash_elem *spt_elem, void *aux UNUSED)
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
 *  returns: a boolean value of whether a is less than b or nott
 */
bool
spt_page_less (const struct hash_elem *first, const struct hash_elem *second, void *aux UNUSED)
{
	// print_spt ();
	const struct page *a = hash_entry (first, struct page, hash_elem);
	const struct page *b = hash_entry (second, struct page, hash_elem);
	printf("page_less return addresses: %p and %p\n", a->addr, b->addr);
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
spt_page_lookup (struct hash *spt, void *addr)
{
	struct page p;
	struct hash_elem *e;
	p.addr = addr;
	e = hash_find (spt, &p.hash_elem);
	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

void
spt_page_destructor (struct hash_elem *e, void *aux UNUSED)
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
spt_load_page (struct hash *spt, void *addr)
{
  /* Calculate how to fill this page.
     We will read PAGE_READ_BYTES bytes from FILE
     and zero the final PAGE_ZERO_BYTES bytes. */

  struct page *page = spt_page_lookup (spt, addr);

  /* Note: a file might be null for a given page; if so, this means it is not 
     a part of the executable section of a process. In that case, it would 
     also have a null offset (ofs). */

  if(page == NULL || page->swap_index >= 0) // Page is null or swapped - null is bad
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
  if (!spt_install_page (page->addr, kpage, page->writable)) 
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

bool
spt_install_page (void *upage, void *kpage, bool writable)
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
struct page*
spt_evict_page (struct hash *spt)
{
	printf("\nCalling spt_evict_page for spt %p...\n", spt);
	//TODO: Don't forget to add synchronization (and make sure Rellermeyer's warning in the PDF is accounted for)!
	struct hash_iterator hand;
	struct page *page;
	uint32_t *pd;
	bool accessed;
	bool dirty;

	bool found = false;

	while (!found)
	{
		page = hash_entry (hash_cur (&hand), struct page, hash_elem);
		pd = page->pagedir;
		accessed = pagedir_is_accessed (pd, page->addr);
		dirty = pagedir_is_dirty (pd, page->addr);

		// lock_acquire(&lock);

		//page->references - 1 != 0? Subtract one from references and don't clear page?

		if (!(accessed || dirty))
		{
			pagedir_clear_page (pd, page);
			found = true;
		}
		else if (!accessed && dirty)
		{
			PANIC ("Failed to evict page %p! No write back method available!\n", page);
			pagedir_clear_page (pd, page);
			//write back;			//TODO: Write write-back method
			found = true;
		}
		else
			pagedir_set_accessed (pd, page->addr, false);

		// lock_release(&lock);

		if (!hash_next (&hand))	{ //Advance pointer
			struct thread *cur = thread_current ();
			hash_first (&hand, &cur->spt);	//If reached end, start over
		}
	}

	printf("spt_evict_page successful!\n");

	return page;
}

/*
 * Function:  print_page
 * --------------------
 *	Does?
 *
 *  params...
 */
void
spt_print_page (struct page *page)
{
	//TODO: Figure out how to get PD
	printf("Page:\t%p"
		"\n\tPage directory:\t%p"
		// "\n\tLoaded:\t%s"
		// "\n\tSwap index:\t%d"
		// "\n\tAccessed:\t%s"
		// "\n\tDirty:\t%s"
		// "\n\tReferences:\t%d"
		// "\n\tStack:\t%s"
		// "\n\tBytes to zero:\t%d"
		// "\n\tBytes to read:\t%d"
		// "\n\tNumber:\t%d"
		// "\n\tSize:\t%d"
		// "\n\tProcess:\t%p"
		// "\n\tWritable:\t%s"
		// "\n\tFile:\t%p"
		// "\n\tFile offset:\t%d"
		// "\n\tHash element:\t%p"
		"\n",
		page->addr,
		page->pd//,
		// page->loaded ? "True" : "False",
		// pagedir_is_accessed (page->pd, page) ? "True" : "False",
		// pagedir_is_dirty (page->pd, page) ? "True" : "False",
		// page->swap_index,
		// page->references,
		// page->is_stack ? "True" : "False",
		// page->zero_bytes,
		// page->read_bytes,
		// page->number,
		// page->size,
		// page->proc_addr,
		// page->writable ? "True" : "False",
		// page->file,
		// page->ofs,
		// &page->hash_elem
		);
}

void
spt_print_spt (struct hash *spt)
{
  struct hash_iterator hand;
  hash_first (&hand, spt);
  int x = 0;

  int size = hash_size(spt);

  while(hash_next(&hand) && x < size) {
  // for (x = 0; x < size; ++x) {
  	spt_print_page (hash_entry (hash_cur (&hand), struct page, hash_elem));
  	// hash_next(&hand);
  	x++;
  }

  // printf("%d\n", spt.bucket_cnt);
  // hash_first (&hand, &spt);
  // for (x = 0; x < spt.bucket_cnt; ++x) {
  // 	printf("%p\n", hand.bucket[x]);
  // }

  // struct list_elem *i;
  // for (i = list_begin (&hand.bucket[0]); i != list_end (&hand.bucket[0]); i = list_next (i)) 
  //   {
  //     struct hash_elem *hi = list_elem_to_hash_elem (i);
  //     print_page(hash_entry(hi, struct page, hash_elem));
  //   //   if (!h->less (hi, e, h->aux) && !h->less (e, hi, h->aux))
  //   //     return hi; 
  //   }

  // while (hash_next (&hand))
  //   print_page (hash_entry (hash_cur (&hand), struct page, hash_elem));
}

void
spt_set_page (struct page *p,
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
	p->loaded = loaded;
	p->swap_index = swap_index;
	p->references = references;
	p->is_stack = is_stack;
	p->zero_bytes = zero_bytes;
	p->read_bytes = read_bytes;
	p->number = number;
	p->size = size;
	p->proc_addr = proc_addr;
	p->writable = writable;
	p->file = file;
	p->ofs = ofs;
}
