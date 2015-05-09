#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/page.h"
#include "vm/frame.h"

void st_init (void);
void swap_out (struct frame *, bool);
void swap_in (void *);
struct lock *get_st_lock(void);

#endif  /* vm/swap.h */
