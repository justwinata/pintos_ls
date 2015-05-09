#include "kernel/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel/gdt.h"
#include "kernel/pagedir.h"
#include "kernel/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "kernel/flags.h"
#include "kernel/init.h"
#include "kernel/interrupt.h"
#include "kernel/palloc.h"
#include "kernel/thread.h"
#include "kernel/vaddr.h"
#include "kernel/synch.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "kernel/pte.h"

static thread_func start_process NO_RETURN;
static bool load (const char *file_args, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   CMDLINE.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *cmdline) 
{
  struct thread *curr = thread_current ();
  int *file_args;
  char *file_args_;
  char *new_cmdline;
  char *token;
  char *save_ptr;
  int cmdline_len;
  int i, j;
  int total_bytes;
  tid_t tid;
  bool valid_char_encountered;

  file_args = palloc_get_page (0);
  new_cmdline = palloc_get_page (0);
  if (file_args == NULL || new_cmdline == NULL)
    return TID_ERROR;

  /* Parse CMDLINE to NEW_CMDLINE into an acceptable format.
     Essentially, we are removing extraneous spaces because
     of the way we implemented tokenizing. */
  cmdline_len = strlen (cmdline) + 1;
  valid_char_encountered = false;
  j = 0;
  for (i = 0; i < cmdline_len; i++)
    {
      if (!valid_char_encountered && cmdline[i] != ' ')
        {
          new_cmdline[j] = cmdline[i];
          valid_char_encountered = true;
          j++;
        }
      else if (valid_char_encountered)
        {
          if (cmdline[i] == ' ')
            valid_char_encountered = false;
          new_cmdline[j] = cmdline[i];
          j++;
        }
    }

  /* Make a copy of NEW_CMDLINE to FILE_ARGS_. */
  file_args_ = (char *) (file_args + 1);
  strlcpy (file_args_, new_cmdline, PGSIZE);

  /* Tokenize FILE_ARGS_ using a space as the delimiter. */
  total_bytes = 0;
  for (token = strtok_r (file_args_, " ", &save_ptr); token != NULL;
       token = strtok_r (NULL, " ", &save_ptr))
    total_bytes += strlen (token) + 1;

  /* Put TOTAL_BYTES at the beginning of FILE_ARGS.
     So please understand that FILE_ARGS is a character
     array where the first 4 bytes represent an integer
     stating how many total bytes there are in the argument
     list. Thus, the first real character is at file_args[4]. */
  file_args[0] = total_bytes;
  
  /* Create a new thread to execute the process. */
  tid = thread_create (&file_args_[0], PRI_DEFAULT, start_process, file_args);
  sema_down (&curr->exec_sema);

  palloc_free_page (new_cmdline);
  if (tid == TID_ERROR)
    palloc_free_page (file_args);
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_args_)
{
  struct thread *curr = thread_current ();
  char *file_args = file_args_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_args, &if_.eip, &if_.esp);

  /* If load failed, quit. */
  palloc_free_page (file_args);
  if (curr->parent != NULL)
    {
      curr->parent->exec_child_success = success;
      sema_up (&curr->parent->exec_sema);
    }
  if (!success)
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid)
{
  int exit_status = -1;
  struct thread *curr = thread_current ();
  struct thread *t;
  struct list *live_list = &curr->live_children;
  struct list *zombie_list = &curr->zombie_children;
  struct list_elem *e;

  for (e = list_begin (live_list); e != list_end (live_list); e = list_next (e))
    {
      t = list_entry (e, struct thread, child_elem);
      if (t->tid == child_tid)
        {
          sema_down (&t->exit_sema);
          break;
        }
    }

  for (e = list_begin (zombie_list); e != list_end (zombie_list); e = list_next (e))
    {
      t = list_entry (e, struct thread, child_elem);
      if (t->tid == child_tid)
        {
          exit_status = t->exit_status;
          list_remove (e);
          palloc_free_page (t);
          break;
        }
    }

  return exit_status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Release process's held locks */
      struct lock *ft = get_ft_lock ();
      struct lock *spt = &cur->spt->lock;
      struct lock *st = get_st_lock ();

      if (lock_held_by_current_thread (ft))
        lock_release (ft);
      if (lock_held_by_current_thread (spt))
        lock_release (spt);
      if (lock_held_by_current_thread (st))
        lock_release (st);

      /* Page reclamation */
      reclaim_pages (cur);

      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, const char *file_args);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_ARGS into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_args, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory and SPT. */
  t->pagedir = pagedir_create ();
  t->spt = spt_create ();

  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (&file_args[sizeof (int)]);

  if (file == NULL)
    {
      printf ("load: %s: open failed\n", &file_args[sizeof (int)]);
      goto done; 
    }
  file_deny_write (file);

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", &file_args[sizeof (int)]);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, file_args))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  t->exec_file = file;
  return success;
}

/* load() helpers. */

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  /* Mark pages in SPT. Here, for lazy loading, no pages will actually be 
     loaded from the executable's corresponding file on disk. Rather, in the 
     page-fault handler in exception.c, pages will be loaded when a process 
     tries to load them and subsequently page faults. To allow this, however, 
     pages must be added to the SPT to keep track of which are loaded and 
     which are not. */
  struct spt *spt = thread_current ()->spt;
  uint32_t num_pages = (read_bytes + zero_bytes) / PGSIZE;
  void *addr = upage;
  uint32_t count;

  for(count = 0; count < num_pages; count++)
  {
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    /* Add page to SPT and set initial values */
    struct page *page = add_page (spt, addr);
    set_page (page, false, -1, 1, false, page_zero_bytes, page_read_bytes, 
      count, PGSIZE, NULL, writable, file, PGSIZE * count);
    /* Page added and values set. */

    /* Advance */
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    //upage += PGSIZE;
    addr += page->size;
  }

  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, const char *file_args)
{
  uint8_t *kpage;
  char *esp_char;
  unsigned int *esp_uint;
  int *esp_int;
  int total_bytes;
  int argc = 0;
  bool success = false;
  bool writable = true;

  /* Add frame to FT and allocate */
  kpage = allocate_uframe (PAL_USER | PAL_ZERO); //palloc_get_page (PAL_USER | PAL_ZERO);

  if (kpage != NULL) 
    {
      void *addr = PHYS_BASE - PGSIZE;
      success = install_page (addr, kpage, writable);

      if (success)
        {
          /* Add page to SPT */

          /* Page added. */

          /* Extract the TOTAL_BYTES to push initially, and decrement
             ESP by that amount. */

          total_bytes = ((int *) file_args)[0];
          *esp = PHYS_BASE - total_bytes;

          /* Save the value of ESP as a char pointer, and copy all
             of the tokenized arguments onto the stack. */
          esp_char = *esp;
          memcpy (esp_char, &file_args[sizeof (int)], total_bytes);

          /* Word-align ESP and save it as an unsigned int pointer. */
          *esp -= ((unsigned int) esp_char % 4);
          esp_uint = *esp;

          /* Now push the starting stack address of each tokenized
             argument onto the stack, starting with address 0. */
          esp_uint--;
          *esp_uint = 0;
          do
            {
              total_bytes--;
              if (esp_char[total_bytes - 1] == '\0')
                {
                  esp_uint--;
                  *esp_uint = (unsigned int) &esp_char[total_bytes];
                  argc++;
                }
            }
          while (total_bytes > 0);

          /* Now push the stack address of the last address pushed onto the
             stack. In other words, push what will be known as 'char **argv'
             within a program's main function. */
          esp_uint--;
          *esp_uint = (unsigned int) (esp_uint + 1);

          /* Next, push ARGC onto the stack. */
          esp_int = (int *) --esp_uint;
          *esp_int = argc;

          /* Finally, push a fake return address of 0 onto the stack. */
          esp_uint--;
          *esp_uint = 0;

          /* Update ESP to point to the end of the initialized stack. */
          *esp = (void *) esp_uint;

          //hex_dump ((uintptr_t) *esp, *esp, (size_t) (PHYS_BASE - *esp), 1);
        }
      else
        {
          /* Remove frame from frame table */
          deallocate_uframe (kpage);
          /* Removed. */
        }
    }
  return success;
}

bool
is_stack (struct intr_frame *frame, void *fault_addr)
{
  return (fault_addr < PHYS_BASE) && (fault_addr >= (frame->esp - STACK_MARGIN));
}

void
load_stack (struct intr_frame *frame, void *f_paddr)
{
  if(PHYS_BASE - frame->esp >= MAX_STACK_SIZE)
  {
    printf ("Max stack size reached.");
    thread_exit ();
  }

  bool writable = true;

  /* Add frame to FT and allocate */
  // !!! !!! !!! TODO: Make reentrant (or something ) !!! !!! !!!
  void *kpage = allocate_uframe (PAL_USER | PAL_ZERO); //palloc_get_page (PAL_USER | PAL_ZERO);

  if (kpage == NULL)
    PANIC ("Null page allocated during page fault on address.\n");

  /* Add page to SPT */
  // !!! !!! !!! TODO: Make reentrant !!! !!! !!!
  struct spt *spt = thread_current()->spt;
  struct page *page = add_page (spt, f_paddr);
  set_page (page, true, -1, 1, true, 0, 0, 0, PGSIZE, NULL, writable, NULL, 0);
  set_page_kva (spt, page, kpage);
  /* Page added. */

  ASSERT (install_page (f_paddr, kpage, writable));
}
