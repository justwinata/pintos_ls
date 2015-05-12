#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/page.h"
#include "vm/frame.h"

void st_init (void);
void st_init_swap_space (void);
void delete_from_swap (uint32_t);
bool swap_out (struct frame *, bool);
void swap_in (void *);
struct lock *get_st_lock(void);

#endif  /* vm/swap.h */
