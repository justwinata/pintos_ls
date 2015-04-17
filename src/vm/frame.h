#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stddef.h>
#include "kernel/palloc.h"

void ft_init(void);
void *allocate_uframe(enum palloc_flags flags);
void deallocate_uframe(void *addr);

#endif /* vm/frame.h */
