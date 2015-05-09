#ifndef KERNEL_EXCEPTION_H
#define KERNEL_EXCEPTION_H

#include "kernel/interrupt.h"

/* Page fault error code bits that describe the cause of the exception.  */
#define PF_P 0x1    /* 0: not-present page. 1: access rights violation. */
#define PF_W 0x2    /* 0: read, 1: write. */
#define PF_U 0x4    /* 0: kernel, 1: user process. */

void exception_init (void);
void exception_print_stats (void);

bool process_pf (struct intr_frame *, void *, bool);

#endif /* kernel/exception.h */
