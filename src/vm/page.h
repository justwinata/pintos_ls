#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stddef.h>
#include <hash.h>
#include "filesys/off_t.h"

struct page
{
	void *addr;					/* Virtual address of page */
	void *pd;					/* Page directory associated with the page */
	bool loaded;				/* Page is loaded from file or not */
	bool swapped;				/* Page is swapped or not */
	uint8_t references;			/* Number of references to this page */
	bool is_stack;				/* Page is part of a prcoess's stack or not */
	int16_t zero_bytes;			/* Amount of page to be zeroed */
	int16_t read_bytes;			/* Amoutn of page to be read */
	uint32_t number;			/* Assignment number to designate order of 
									creation or being swapped in */
	uint32_t size;				/* Size of data */
	void *proc_addr;			/* Address of process (thread) to which page 
									belongs */
	bool writable;				/* Page is writable or not*/
	struct file *file;			/* The file the page is loaded from */
	off_t ofs;					/* The offset the page is at within file */
	struct hash_elem hash_elem;	/* The hash element used to store a page in 
									the SPT hash-table */
};

void spt_init (void);
void remove_page (struct page *);
struct page *add_page (void *);
struct page *page_lookup (void *);
bool load_page(void *);
bool install_page (void *, void *, bool);
void evict_page (void);
void print_page (struct page *);

#endif  /* vm/page.h */
