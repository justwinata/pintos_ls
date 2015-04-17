#include <stdio.h>
#include <hash.h>
#include "vm/page.h"
/*
	Supplemental Page Table: "Gives you information about the status of virtual memory (which addresses are logicially mapped, etc.)" - Courtesy https://groups.google.com/forum/#!topic/12au-cs140/5LDnfoOMdfY
*/

/*
	-Functionalities:
	>Your “s-page table” must be able to decide where to load executable and which corresponding page of executable to load
	>Your “s-page table ” must be able to decide how to get swap disk and which part (in sector) of swap disk stores the corresponding page
	-Implementation
	>Use hash table (recommend)
	-Usage
	>Rewrite load_segment() (in process.c) to populate s-page table without loading pages into memory
	>Page fault handler then loads pages after consulting s-page table
	Courtesy http://courses.cs.vt.edu/cs3204/spring2007/pintos/Project3SessionSpring2007.ppt (slide 12)
*/

/*
	TODO:
		Modify the following:
		---------------------------
		load_segment() in process.c
		page_fault() in exception.c
*/

static struct hash spt;	/* Supplemental Page Table */

struct page
{
	void *addr;			/* Virtual address of page */
	bool mapped;		/* Page is logically mapped or not */
	uint number;		/* Assignment number to designate order of creation or being swapped in */
	uint size;			/* Size of data */
	void *proc_addr;	/* Address of process (thread) to which page belongs */
	struct hash_elem hash_elem;
};

unsigned page_hash (const struct hash_elem *spt_elem, void *aux UNUSED);
bool page_less (const struct hash_elem *first, const struct hash_elem *second, void *aux UNUSED);
struct page *page_lookup (void *address);

/* Take in two pages? */
void swap(void);

/* Take in one page and return evicted entry or address thereof? */
void evict(void);

/* Implement actual swapping/eviction and employ chosen replacement algorithm here */
void find_candidate(void);

void
spt_init() 
{ 
	printf("Calling spt_init..."); 
	hash_init(&spt, page_hash, page_less, NULL); 
	printf("spt_init successful: %p", &spt); 
}

unsigned
page_hash (const struct hash_elem *spt_elem, void *aux UNUSED)
{
	const struct page *page = hash_entry (spt_elem, struct page, hash_elem);
	return hash_bytes (&page->addr, sizeof page->addr);
}

bool
page_less (const struct hash_elem *first, const struct hash_elem *second, void *aux UNUSED)
{
	const struct page *a = hash_entry (first, struct page, hash_elem);
	const struct page *b = hash_entry (second, struct page, hash_elem);
	return a->addr < b->addr;
}

struct page*
page_lookup (void *address)
{
	struct page p;
	struct hash_elem *e;
	p.addr = address;
	e = hash_find (&spt, &p.hash_elem);
	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

/* Also needs to handle lazy loading of executables */

/* ... */
