#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stddef.h>
#include <hash.h>
#include "kernel/palloc.h"

struct frame
{
	struct hash_elem hash_elem;	/* Hash-table element */
	void *addr;					/* Physical address of frame */
	//pid, etc.?
};

void ft_init(void);
void *allocate_uframe(enum palloc_flags flags);
void deallocate_uframe(void *addr);

#endif /* vm/frame.h */
