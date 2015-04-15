#include <stdio.h>

/* Supplemental Page Table: "Gives you information about the status of virtual memory (which addresses are logicially mapped, etc.)" - Courtesy https://groups.google.com/forum/#!topic/12au-cs140/5LDnfoOMdfY */

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

struct page
{
	bool mapped;		/* Page is logically mapped or not */
	uint size;			/* Size of data */
	void *addr;			/* Virtual address of page */
	uint number;		/* Assignment number to designate order of creation or being swapped in */
	void *proc_addr;	/* Address of process (thread) to which page belongs */
};

/* Take in two pages? */
void swap(void);

/* Take in one page and return evicted entry or address thereof? */
void evict(void);

/* Implement actual swapping/eviction and employ chosen replacement algorithm here */
void find_candidate(void)

/* Also needs to handle lazy loading of executables */

/* ... */