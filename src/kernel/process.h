#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include "kernel/thread.h"
#include "kernel/interrupt.h"
#include "vm/page.h"

static int MAX_STACK_SIZE = 8 * 1024 * 1024;	/* An 8MB maximum stack size */
static int STACK_MARGIN = 32;					/* A margin of error to be 
													used primarily in the 
													heuristic for stack 
													growth - the 32 comes from 
													the size of the PUSHA
													instruction */

tid_t process_execute (const char *cmdline);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool is_stack (struct intr_frame *, void *);
void load_stack (struct intr_frame *, void *);
bool load_page (struct spt *, struct page *);

#endif /* kernel/process.h */
