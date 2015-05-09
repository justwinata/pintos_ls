#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stddef.h>
#include <hash.h>
#include "kernel/palloc.h"

struct frame
{
	struct hash_elem hash_elem;	/* Hash-table element */
	void *addr;					/* Physical address of frame */
	bool pinned;				/* Boolean for pinning */
	struct thread *thread;		/* Thread to which frame belongs */
};

void ft_init(void);
void remove_frame(struct frame *);
struct frame *frame_lookup(void *);
void *allocate_uframe(enum palloc_flags);
void deallocate_uframe(void *);
void evict_page (void);
void print_ft (void);
struct lock *get_ft_lock(void);

#endif /* vm/frame.h */
