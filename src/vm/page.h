#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "vm/page.c" // Quick fix for struct page access for exception.c

void spt_init (void);
struct page *page_lookup (void *);
struct page *add_page (void *);
void remove_page (struct page *);

#endif /* vm/page.h */
