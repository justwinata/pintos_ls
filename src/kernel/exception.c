#include "kernel/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel/gdt.h"
#include "kernel/interrupt.h"
#include "kernel/thread.h"
#include "kernel/vaddr.h"
#include "kernel/process.h"
#include "kernel/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"

/* Number of page faults processed. */
static long long page_fault_cnt;
static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

static bool load_page(void *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  /* Handle bad dereferences from system call implementations. */
  if (!user)
    {
      f->eip = (void (*) (void)) f->eax;
      f->eax = 0;
      return;
    }

  // Our stuff from hereon:

  int *total_bytes = (int *)(PHYS_BASE - sizeof(char) - sizeof(void *));  // USER MEMORY ACCESS - BLEH // TO-DO:

  ASSERT(*total_bytes > MAX_STACK_SIZE); // WHAT DO?

  if(*((int *)fault_addr) > PHYS_BASE - f->esp)
  {
    //page swapped?
  }
  else if(fault_addr > f->esp - STACK_MARGIN)  //f->esp - STACK_MARGIN < PHYS_BASE - *total_bytes) //Stack growth - STACK_MARGIN is relatively arbitrary heuristic
  {
    bool writable = true;

    /* Add frame to FT and allocate */
    uint8_t *kpage = allocate_uframe(PAL_USER | PAL_ZERO); //palloc_get_page (PAL_USER | PAL_ZERO);

    ASSERT(kpage);  //Pls don't b null

    /* Add page to SPT */
    struct page *page = add_page (kpage);
    page->loaded = true;
    page->swapped = false;
    page->is_stack = true;
    page->number = 0;
    page->size = PGSIZE;
    page->writable = writable;
    /* Page added. */

    ASSERT(install_page (((uint8_t *) PHYS_BASE) - PGSIZE - *total_bytes, kpage, writable)); //Pls don't return null valu
  }
  /*
    How does OS handle a page fault?
    • Interrupt causes system to be entered
    • System saves state of running process, then vectors to page fault handler routine
      – find or create (through eviction) a page frame into which to load the needed page (1)
        • if I/O is required, run some other process while it’s going on
      – find the needed page on disk and bring it into the page frame (2)
        • run some other process while the I/O is going on
      – fix up the page table entry
        • mark it as “valid,” set “referenced” and “modified” bits to false, set protection bits appropriately, point to correct page frame
      – put the process on the ready queue

    Courtesy https://courses.cs.washington.edu/courses/cse451/12sp/lectures/13-hardware.support.pdf (slides 3-6)
  */

    else  // Lazy loading
    {
      load_page ((void *)((int) fault_addr - (int) fault_addr % (int) PGSIZE));
    }

    //Swapping? In load_page?
}

/*
 * Function:  load_page 
 * --------------------
 *  Loads pages as needed when a process page faults on memory it should have 
      have access to but has not been loaded (lazy loading). Calculates  using 
      the file from which the pages for the process are laoded (stored by 
      pointer in each page entry in the SPT) and loads the page-sized section 
      of the file. The faulting process will return to the statement on which 
      it page faulted and resume with that same instruction, which should then 
      continue without another page fault.
 *
 *  addr: the address on which the halted process faulted
 */
bool
load_page(void *addr)
{
  /* Calculate how to fill this page.
     We will read PAGE_READ_BYTES bytes from FILE
     and zero the final PAGE_ZERO_BYTES bytes. */

  struct page *page = page_lookup (addr);

  /* Note: a file might be null for a given page; if so, this means it is not 
     a part of the executable section of a process. In that case, it would 
     also have a null offset (ofs). */

  if(!page || page->swapped) // Page is null or swapped - null is bad
  {
    //TODO: Implement swapping and stuff
    return false; //Faile for now :(    --    Could also panic kernel
  }

  /* Get a page of memory. */
  //Should virtual addresses be contiguous for a process? Are they already?
  uint8_t *kpage = allocate_uframe (PAL_USER); //palloc_get_page (PAL_USER);
  if (kpage == NULL)
    return false;

  /* Load this page. */
  file_seek (page->file, page->ofs);
  if (file_read (page->file, kpage, page->read_bytes) != (int) page->read_bytes)
    {
      deallocate_uframe (kpage);
      return false; 
    }
    
  memset (kpage + page->read_bytes, 0, page->zero_bytes);

  /* Add the page to the process's address space. */
  if (!install_page (page->addr, kpage, page->writable)) 
    {
      deallocate_uframe (kpage);
      return false; 
    }

  /* Mark the page as loaded */
  page->loaded = true;

  return true;
}


/* Copied directly from process.c for use by exception.c in load_page */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

