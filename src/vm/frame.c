#include <stdio.h>
//#include "kernel/synch.h" //TODO: Consider if synchronization for hash table needed
#include <hash.h>
#include "vm/frame.h"

static struct hash frame_table;

/*
EXPLAIN STRUCT FRAME
*/
struct frame
{
	struct hash_elem hash_elem;	/* Hash-table element */
	void *addr;					/* Physical address of frame */
	//pid, etc.?
};

unsigned frame_hash (const struct hash_elem *, void *);
bool frame_less (const struct hash_elem *, const struct hash_elem *, void *);
struct frame* frame_lookup (void *);

void
ft_init()
{
	printf("Calling ft_init...");
	hash_init(&frame_table, frame_hash, frame_less, NULL);
	printf("ft_init successful.");
}

unsigned
frame_hash (const struct hash_elem *ft_elem, void *aux UNUSED)
{
	const struct frame *frame = hash_entry (ft_elem, struct frame, hash_elem);
	return hash_bytes (&frame->addr, sizeof frame->addr);
}

bool
frame_less (const struct hash_elem *first, const struct hash_elem *second, void *aux UNUSED)
{
	const struct frame *a = hash_entry (first, struct frame, hash_elem);
	const struct frame *b = hash_entry (second, struct frame, hash_elem);
	return a->addr < b->addr;
}

struct frame*
frame_lookup (void *address)
{
	struct frame f;
	struct hash_elem *e;
	f.addr = address;
	e = hash_find (&frame_table, &f.hash_elem);
	return e != NULL ? hash_entry (e, struct frame, hash_elem) : NULL;
}

void*
allocate_uframe(enum palloc_flags flags)
{
	printf("Calling allocate_uframe...");
	void *addr = palloc_get_page (flags | PAL_ASSERT);
	struct hash_elem *elem = (struct hash_elem *) malloc (sizeof (struct hash_elem));
	struct frame *frame = hash_entry (elem, struct frame, hash_elem);
	frame->addr = addr;
	hash_insert(&frame_table, &frame->hash_elem);
	printf("allocate_uframe successful. Memory assigned at: %p", addr);
	return addr;
}

void
deallocate_uframe(void *addr)
{
	printf("Calling deallocate_uframe for %p", addr);
	struct frame *f = frame_lookup(addr);

	if(!f)
	{
		printf("WARNING: Attempting to delete non-existent frame from frame_table! %p", addr);
	}
	else
	{
		struct hash_elem *e = hash_delete(&frame_table, &f->hash_elem);
		//TODO: Consider what we actually need to free
		free(e);
		free(f);
	}
	//TODO: Consider if palloc_free_page call is necessary
	palloc_free_page (addr);
	printf("deallocate_uframe successful for %p", addr);
}
