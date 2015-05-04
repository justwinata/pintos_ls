#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/page.h"

void st_init (void);
void swap_out (struct page *);
void swap_in (struct page *);

#endif  /* vm/swap.h */
