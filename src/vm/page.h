#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stddef.h>
#include <hash.h>
#include "kernel/synch.h"
#include "filesys/off_t.h"

struct spt
{
	struct hash table;				/* Supplemental page table. */
	struct hash table_kva;			/* SPT's secondary hash table */
    struct lock lock;				/* Lock for synchronization of SPT */
    struct lock lock_kva;			/* Lock for synchronization of table_kva */
};

struct page
{
	void *addr;						/* Virtual address of page */
	void *kv_addr;					/* Address of page from palloc_get_page */
	void *pd;						/* Page directory associated with page */
	bool loaded;					/* Page is loaded from file or not */
	bool pinned;					/* Page is pinned or not */
	int16_t swap_index;				/* Page's index in swap table if swapped */
	uint8_t references;				/* Number of references to this page */
	bool is_stack;					/* Page is part of process stack or not */
	int16_t zero_bytes;				/* Amount of page to be zeroed */
	int16_t read_bytes;				/* Amoutn of page to be read */
	uint32_t number;				/* Assignment number to designate order of 
										creation or being swapped in */
	uint32_t size;					/* Size of data */
	void *proc_addr;				/* Address of process (thread) to which
										page belongs */
	uint32_t *pagedir;				/* Page directory page is in */
	bool writable;					/* Page is writable or not*/
	struct file *file;				/* The file the page is loaded from */
	off_t ofs;						/* The offset the page is at within file */
	struct hash_elem hash_elem;		/* The hash element used to store a page in 
										the SPT hash-table */
	struct hash_elem hash_elem_kva;	/* The hash element used to store a page in 
										the SPTs secondary hash-table for quick
										translation from a virtual adress to a
										kernel virtual address */
};

struct spt *spt_create (void);
void spt_destroy (struct spt *);
void remove_page (struct spt *, struct page *);
struct page *add_page (struct spt *, void *);
struct page *page_lookup (struct hash *, void *);
struct page* page_lookup_kva (struct hash *, void *);
bool install_page (void *, void *, bool);
bool is_writable_buffer (char **, unsigned);
void reclaim_pages (struct thread *);

void print_page (struct page *);
void print_spt (struct spt *spt);
bool is_resident (void *);
void set_page (struct page *p,
				bool loaded,
				bool pinned,
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

void set_page_kva (struct spt *, struct page *, void *);

#endif  /* vm/page.h */
