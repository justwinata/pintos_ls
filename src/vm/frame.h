#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stddef.h>
#include <hash.h>
#include "kernel/palloc.h"

struct frame
{
	void *addr;					/* Physical address of frame */
	bool pinned;				/* Boolean for pinning */
	struct thread *thread;		/* Thread to which frame belongs */
	struct hash_elem hash_elem;	/* Hash-table element */
};

void ft_init(void);
struct frame *frame_lookup(void *);
struct frame *allocate_uframe(enum palloc_flags);
void deallocate_uframe(void *);
void deallocate_uframe_f (struct frame *);
void evict_page (void);
void print_ft (void);
struct lock *get_ft_lock(void);

#endif /* vm/frame.h */
