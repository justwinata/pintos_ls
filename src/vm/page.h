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
	int16_t swap_index;			/* Page is swapped at index if index not < 0 */
	uint8_t references;			/* Number of references to this page */
	bool is_stack;				/* Page is part of a process's stack or not */
	int16_t zero_bytes;			/* Amount of page to be zeroed */
	int16_t read_bytes;			/* Amoutn of page to be read */
	uint32_t number;			/* Assignment number to designate order of 
									creation or being swapped in */
	uint32_t size;				/* Size of data */
	void *proc_addr;			/* Address of process (thread) to which page 
									belongs */
	uint32_t *pagedir;			/* Page directory page is in */
	bool writable;				/* Page is writable or not*/
	struct file *file;			/* The file the page is loaded from */
	off_t ofs;					/* The offset the page is at within file */
	struct hash_elem hash_elem;	/* The hash element used to store a page in 
									the SPT hash-table */
};

unsigned spt_page_hash (const struct hash_elem *, void *);
bool spt_page_less (const struct hash_elem *, const struct hash_elem *, void *);
void spt_page_destructor (struct hash_elem *, void *);

struct hash *spt_create (void);
void spt_destroy (struct hash *);
void spt_remove_page (struct hash *, struct page *);
struct page *spt_add_page (struct hash *, void *);
struct page *spt_page_lookup (struct hash *, void *);
bool spt_load_page(struct hash *, void *);
bool spt_install_page (void *, void *, bool);
struct page *spt_evict_page (struct hash *);
void spt_print_page (struct page *);
void spt_print_spt (struct hash *);
void spt_set_page (struct page *p,
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
					off_t ofs);

// void spt_init (void);
// void remove_page (struct page *);
// struct page *add_page (void *);
// struct page *page_lookup (void *);
// bool load_page(void *);
// bool install_page (void *, void *, bool);
// struct page *evict_page (void);
// void print_page (struct page *);
// void print_spt (void);
// void set_page (struct page *p,
// 				bool loaded,
// 				int16_t swap_index,
// 				uint8_t references,
// 				bool is_stack,
// 				int16_t zero_bytes,
// 				int16_t read_bytes,
// 				uint32_t number,
// 				uint32_t size,
// 				void *proc_addr,
// 				bool writable,
// 				struct file *file,
// 				off_t ofs);

#endif  /* vm/page.h */
