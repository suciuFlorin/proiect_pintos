#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/input.h"
#include "devices/shutdown.h" /* Imports shutdown_power_off() for use in halt(). */
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "threads/init.h"

static void syscall_handler(struct intr_frame *);

static int sys_halt(void);
static int sys_exit(int status);
static int sys_exec(const char *ufile);
static int sys_wait(tid_t);
static int sys_create(const char *ufile, unsigned initial_size);
static int sys_remove(const char *ufile);
static int sys_open(const char *ufile);
static int sys_filesize(int handle);
static int sys_read(int handle, void *udst_, unsigned size);
static int sys_write(int handle, void *usrc_, unsigned size);
static int sys_seek(int handle, unsigned position);
static int sys_tell(int handle);
static int sys_close(int handle);
static int sys_mmap(int handle, void *addr);
static int sys_munmap(int mapping);

static void syscall_handler(struct intr_frame *);
static void copy_in(void *, const void *, size_t);

/* Get up to three arguments from a programs stack (they directly follow the system
call argument). */
void get_stack_arguments(struct intr_frame *f, int *args, int num_of_args);

/* Creates a struct to insert files and their respective file descriptor into
   the file_descriptors list for the current thread. */
struct thread_file
{
  struct list_elem file_elem;
  struct file *file_addr;
  int file_descriptor;
};

/* Lock is in charge of ensuring that only one process can access the file system at one time. */
struct lock lock_filesys;

void syscall_init(void)
{
  /* Initialize the lock for the file system. */
  lock_init(&lock_filesys);

  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Handles a system call initiated by a user program. */
static void
syscall_handler(struct intr_frame *f UNUSED)
{
  /* First ensure that the system call argument is a valid address. If not, exit immediately. */
  check_valid_addr((const void *)f->esp);

  /* Holds the stack arguments that directly follow the system call. */
  int args[3];

  /* Stores the physical page pointer. */
  void *phys_page_ptr;

  /* Get the value of the system call (based on enum) and call corresponding syscall function. */
  switch (*(int *)f->esp)
  {
  case SYS_HALT:
    /* Call the halt() function, which requires no arguments */
    halt();
    break;

  case SYS_EXIT:
    /* Exit has exactly one stack argument, representing the exit status. */
    get_stack_arguments(f, &args[0], 1);

    /* We pass exit the status code of the process. */
    exit(args[0]);
    break;

  case SYS_EXEC:
    /* The first argument of exec is the entire command line text for executing the program */
    get_stack_arguments(f, &args[0], 1);

    /* Ensures that converted address is valid. */
    phys_page_ptr = (void *)pagedir_get_page(thread_current()->pagedir, (const void *)args[0]);
    if (phys_page_ptr == NULL)
    {
      exit(-1);
    }
    args[0] = (int)phys_page_ptr;

    /* Return the result of the exec() function in the eax register. */
    f->eax = exec((const char *)args[0]);
    break;

  case SYS_WAIT:
    /* The first argument is the PID of the child process
           that the current process must wait on. */
    get_stack_arguments(f, &args[0], 1);

    /* Return the result of the wait() function in the eax register. */
    f->eax = wait((pid_t)args[0]);
    break;

  case SYS_CREATE:
    /* The first argument is the name of the file being created,
           and the second argument is the size of the file. */
    get_stack_arguments(f, &args[0], 2);
    check_buffer((void *)args[0], args[1]);

    /* Ensures that converted address is valid. */
    phys_page_ptr = pagedir_get_page(thread_current()->pagedir, (const void *)args[0]);
    if (phys_page_ptr == NULL)
    {
      exit(-1);
    }
    args[0] = (int)phys_page_ptr;

    /* Return the result of the create() function in the eax register. */
    f->eax = create((const char *)args[0], (unsigned)args[1]);
    break;

  case SYS_REMOVE:
    /* The first argument of remove is the file name to be removed. */
    get_stack_arguments(f, &args[0], 1);

    /* Ensures that converted address is valid. */
    phys_page_ptr = pagedir_get_page(thread_current()->pagedir, (const void *)args[0]);
    if (phys_page_ptr == NULL)
    {
      exit(-1);
    }
    args[0] = (int)phys_page_ptr;

    /* Return the result of the remove() function in the eax register. */
    f->eax = remove((const char *)args[0]);
    break;

  case SYS_OPEN:
    /* The first argument is the name of the file to be opened. */
    get_stack_arguments(f, &args[0], 1);

    /* Ensures that converted address is valid. */
    phys_page_ptr = pagedir_get_page(thread_current()->pagedir, (const void *)args[0]);
    if (phys_page_ptr == NULL)
    {
      exit(-1);
    }
    args[0] = (int)phys_page_ptr;

    /* Return the result of the remove() function in the eax register. */
    f->eax = open((const char *)args[0]);

    break;

  case SYS_FILESIZE:
    /* filesize has exactly one stack argument, representing the fd of the file. */
    get_stack_arguments(f, &args[0], 1);

    /* We return file size of the fd to the process. */
    f->eax = filesize(args[0]);
    break;

  case SYS_READ:
    /* Get three arguments off of the stack. The first represents the fd, the second
           represents the buffer, and the third represents the buffer length. */
    get_stack_arguments(f, &args[0], 3);

    /* Make sure the whole buffer is valid. */
    check_buffer((void *)args[1], args[2]);

    /* Ensures that converted address is valid. */
    phys_page_ptr = pagedir_get_page(thread_current()->pagedir, (const void *)args[1]);
    if (phys_page_ptr == NULL)
    {
      exit(-1);
    }
    args[1] = (int)phys_page_ptr;

    /* Return the result of the read() function in the eax register. */
    f->eax = read(args[0], (void *)args[1], (unsigned)args[2]);
    break;

  case SYS_WRITE:
    /* Get three arguments off of the stack. The first represents the fd, the second
           represents the buffer, and the third represents the buffer length. */
    get_stack_arguments(f, &args[0], 3);

    /* Make sure the whole buffer is valid. */
    check_buffer((void *)args[1], args[2]);

    /* Ensures that converted address is valid. */
    phys_page_ptr = pagedir_get_page(thread_current()->pagedir, (const void *)args[1]);
    if (phys_page_ptr == NULL)
    {
      exit(-1);
    }
    args[1] = (int)phys_page_ptr;

    /* Return the result of the write() function in the eax register. */
    f->eax = write(args[0], (const void *)args[1], (unsigned)args[2]);
    break;

  case SYS_SEEK:
    /* Get two arguments off of the stack. The first represents the fd, the second
           represents the position. */
    get_stack_arguments(f, &args[0], 2);

    /* Return the result of the seek() function in the eax register. */
    seek(args[0], (unsigned)args[1]);
    break;

  case SYS_TELL:
    /* tell has exactly one stack argument, representing the fd of the file. */
    get_stack_arguments(f, &args[0], 1);

    /* We return the position of the next byte to read or write in the fd. */
    f->eax = tell(args[0]);
    break;

  case SYS_CLOSE:
    /* close has exactly one stack argument, representing the fd of the file. */
    get_stack_arguments(f, &args[0], 1);

    /* We close the file referenced by the fd. */
    close(args[0]);
    break;

  default:
    /* If an invalid system call was sent, terminate the program. */
    exit(-1);
    break;
  }
}

/* Copies SIZE bytes from user address USRC to kernel address
   DST.
   Call thread_exit() if any of the user accesses are invalid. */
static void
copy_in (void *dst_, const void *usrc_, size_t size)
{
  uint8_t *dst = dst_;
  const uint8_t *usrc = usrc_;

  while (size > 0)
    {
      size_t chunk_size = PGSIZE - pg_ofs (usrc);
      if (chunk_size > size)
        chunk_size = size;

      if (!page_lock (usrc, false))
        thread_exit ();
      memcpy (dst, usrc, chunk_size);
      page_unlock (usrc);

      dst += chunk_size;
      usrc += chunk_size;
      size -= chunk_size;
    }
}

/* Creates a copy of user string US in kernel memory
   and returns it as a page that must be freed with
   palloc_free_page().
   Truncates the string at PGSIZE bytes in size.
   Call thread_exit() if any of the user accesses are invalid. */
static char *
copy_in_string (const char *us)
{
  char *ks;
  char *upage;
  size_t length;

  ks = palloc_get_page (0);
  if (ks == NULL)
    thread_exit ();

  length = 0;
  for (;;)
    {
      upage = pg_round_down (us);
      if (!page_lock (upage, false))
        goto lock_error;

      for (; us < upage + PGSIZE; us++)
        {
          ks[length++] = *us;
          if (*us == '\0')
            {
              page_unlock (upage);
              return ks;
            }
          else if (length >= PGSIZE)
            goto too_long_error;
        }

      page_unlock (upage);
    }

 too_long_error:
  page_unlock (upage);
 lock_error:
  palloc_free_page (ks);
  thread_exit ();
}

/* Terminates Pintos, shutting it down entirely (bummer). */
void halt(void)
{
  shutdown_power_off();
}

/* Get the system call. */
copy_in(&call_nr, f->esp, sizeof call_nr);
if (call_nr >= sizeof syscall_table / sizeof *syscall_table)
  thread_exit();
sc = syscall_table + call_nr;

/* Get the system call arguments. */
ASSERT(sc->arg_cnt <= sizeof args / sizeof *args);
memset(args, 0, sizeof args);
copy_in(args, (uint32_t *)f->esp + 1, sizeof *args * sc->arg_cnt);

/* Execute the system call,
     and set the return value. */
f->eax = sc->func(args[0], args[1], args[2]);
}

/* Wait system call. */
static int
sys_wait(tid_t child)
{
  return process_wait(child);
}

/* Create system call. */
static int
sys_create(const char *ufile, unsigned initial_size)
{
  char *kfile = copy_in_string(ufile);
  bool ok;

  lock_acquire(&fs_lock);
  ok = filesys_create(kfile, initial_size);
  lock_release(&fs_lock);

  palloc_free_page(kfile);

  return ok;
}

/* Remove system call. */
static int
sys_remove(const char *ufile)
{
  char *kfile = copy_in_string(ufile);
  bool ok;

  lock_acquire(&fs_lock);
  ok = filesys_remove(kfile);
  lock_release(&fs_lock);

  palloc_free_page(kfile);

  return ok;
}

/* A file descriptor, for binding a file handle to a file. */
struct file_descriptor
  {
    struct list_elem elem;      /* List element. */
    struct file *file;          /* File. */
    int handle;                 /* File handle. */
  };

/* Open system call. */
static int
sys_open (const char *ufile)
{
  char *kfile = copy_in_string (ufile);
  struct file_descriptor *fd;
  int handle = -1;

  fd = malloc (sizeof *fd);
  if (fd != NULL)
    {
      lock_acquire (&fs_lock);
      fd->file = filesys_open (kfile);
      if (fd->file != NULL)
        {
          struct thread *cur = thread_current ();
          handle = fd->handle = cur->next_handle++;
          list_push_front (&cur->fds, &fd->elem);
        }
      else
        free (fd);
      lock_release (&fs_lock);
    }

  palloc_free_page (kfile);
  return handle;
}

/* Returns the file descriptor associated with the given handle.
   Terminates the process if HANDLE is not associated with an
   open file. */
static struct file_descriptor *
lookup_fd (int handle)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;

  for (e = list_begin (&cur->fds); e != list_end (&cur->fds);
       e = list_next (e))
    {
      struct file_descriptor *fd;
      fd = list_entry (e, struct file_descriptor, elem);
      if (fd->handle == handle)
        return fd;
    }

  thread_exit ();
}

/* Filesize system call. */
static int
sys_filesize (int handle)
{
  struct file_descriptor *fd = lookup_fd (handle);
  int size;

  lock_acquire (&fs_lock);
  size = file_length (fd->file);
  lock_release (&fs_lock);

  return size;
}

/* Read system call. */
static int
sys_read (int handle, void *udst_, unsigned size)
{
  uint8_t *udst = udst_;
  struct file_descriptor *fd;
  int bytes_read = 0;

  fd = lookup_fd (handle);
  while (size > 0)
    {
      /* How much to read into this page? */
      size_t page_left = PGSIZE - pg_ofs (udst);
      size_t read_amt = size < page_left ? size : page_left;
      off_t retval;

      /* Read from file into page. */
      if (handle != STDIN_FILENO)
        {
          if (!page_lock (udst, true))
            thread_exit ();
          lock_acquire (&fs_lock);
          retval = file_read (fd->file, udst, read_amt);
          lock_release (&fs_lock);
          page_unlock (udst);
        }
      else
        {
          size_t i;

          for (i = 0; i < read_amt; i++)
            {
              char c = input_getc ();
              if (!page_lock (udst, true))
                thread_exit ();
              udst[i] = c;
              page_unlock (udst);
            }
          bytes_read = read_amt;
        }

      /* Check success. */
      if (retval < 0)
        {
          if (bytes_read == 0)
            bytes_read = -1;
          break;
        }
      bytes_read += retval;
      if (retval != (off_t) read_amt)
        {
          /* Short read, so we're done. */
          break;
        }

      /* Advance. */
      udst += retval;
      size -= retval;
    }

  return bytes_read;
}

/* Write system call. */
static int
sys_write (int handle, void *usrc_, unsigned size)
{
  uint8_t *usrc = usrc_;
  struct file_descriptor *fd = NULL;
  int bytes_written = 0;

  /* Lookup up file descriptor. */
  if (handle != STDOUT_FILENO)
    fd = lookup_fd (handle);

  while (size > 0)
    {
      /* How much bytes to write to this page? */
      size_t page_left = PGSIZE - pg_ofs (usrc);
      size_t write_amt = size < page_left ? size : page_left;
      off_t retval;

      /* Write from page into file. */
      if (!page_lock (usrc, false))
        thread_exit ();
      lock_acquire (&fs_lock);
      if (handle == STDOUT_FILENO)
        {
          putbuf ((char *) usrc, write_amt);
          retval = write_amt;
        }
      else
        retval = file_write (fd->file, usrc, write_amt);
      lock_release (&fs_lock);
      page_unlock (usrc);

      /* Handle return value. */
      if (retval < 0)
        {
          if (bytes_written == 0)
            bytes_written = -1;
          break;
        }
      bytes_written += retval;

      /* If it was a short write we're done. */
      if (retval != (off_t) write_amt)
        break;

      /* Advance. */
      usrc += retval;
      size -= retval;
    }

  return bytes_written;
}

/* Seek system call. */
static int
sys_seek (int handle, unsigned position)
{
  struct file_descriptor *fd = lookup_fd (handle);

  lock_acquire (&fs_lock);
  if ((off_t) position >= 0)
    file_seek (fd->file, position);
  lock_release (&fs_lock);

  return 0;
}

/* Tell system call. */
static int
sys_tell (int handle)
{
  struct file_descriptor *fd = lookup_fd (handle);
  unsigned position;

  lock_acquire (&fs_lock);
  position = file_tell (fd->file);
  lock_release (&fs_lock);

  return position;
}

/* Close system call. */
static int
sys_close (int handle)
{
  struct file_descriptor *fd = lookup_fd (handle);
  lock_acquire (&fs_lock);
  file_close (fd->file);
  lock_release (&fs_lock);
  list_remove (&fd->elem);
  free (fd);
  return 0;
}

/* Binds a mapping id to a region of memory and a file. */
struct mapping
  {
    struct list_elem elem;      /* List element. */
    int handle;                 /* Mapping id. */
    struct file *file;          /* File. */
    uint8_t *base;              /* Start of memory mapping. */
    size_t page_cnt;            /* Number of pages mapped. */
  };

/* Returns the file descriptor associated with the given handle.
   Terminates the process if HANDLE is not associated with a
   memory mapping. */
static struct mapping *
lookup_mapping (int handle)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;

  for (e = list_begin (&cur->mappings); e != list_end (&cur->mappings);
       e = list_next (e))
    {
      struct mapping *m = list_entry (e, struct mapping, elem);
      if (m->handle == handle)
        return m;
    }

  thread_exit ();
}

/* Terminates the current user program. It's exit status is printed,
   and its status returned to the kernel. */
void exit(int status)
{
  thread_current()->exit_status = status;
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();
}

/* Writes LENGTH bytes from BUFFER to the open file FD. Returns the number of bytes actually written,
 which may be less than LENGTH if some bytes could not be written. */
int write(int fd, const void *buffer, unsigned length)
{
  /* list element to iterate the list of file descriptors. */
  struct list_elem *temp;

  lock_acquire(&lock_filesys);

  /* If fd is equal to one, then we write to STDOUT (the console, usually). */
  if (fd == 1)
  {
    putbuf(buffer, length);
    lock_release(&lock_filesys);
    return length;
  }
  /* If the user passes STDIN or no files are present, then return 0. */
  if (fd == 0 || list_empty(&thread_current()->file_descriptors))
  {
    lock_release(&lock_filesys);
    return 0;
  }

  /* Check to see if the given fd is open and owned by the current process. If so, return
     the number of bytes that were written to the file. */
  for (temp = list_front(&thread_current()->file_descriptors); temp != NULL; temp = temp->next)
  {
    struct thread_file *t = list_entry(temp, struct thread_file, file_elem);
    if (t->file_descriptor == fd)
    {
      int bytes_written = (int)file_write(t->file_addr, buffer, length);
      lock_release(&lock_filesys);
      return bytes_written;
    }
  }

  lock_release(&lock_filesys);

  /* If we can't write to the file, return 0. */
  return 0;
}

/* Executes the program with the given file name. */
pid_t exec(const char *file)
{
  /* If a null file is passed in, return a -1. */
  if (!file)
  {
    return -1;
  }
  lock_acquire(&lock_filesys);
  /* Get and return the PID of the process that is created. */
  pid_t child_tid = process_execute(file);
  lock_release(&lock_filesys);
  return child_tid;
}

/* If the PID passed in is our child, then we wait on it to terminate before proceeding */
int wait(pid_t pid)
{
  /* If the thread created is a valid thread, then we must disable interupts, and add it to this threads list of child threads. */
  return process_wait(pid);
}

/* Creates a file of given name and size, and adds it to the existing file system. */
bool create(const char *file, unsigned initial_size)
{
  lock_acquire(&lock_filesys);
  bool file_status = filesys_create(file, initial_size);
  lock_release(&lock_filesys);
  return file_status;
}

/* Remove the file from the file system, and return a boolean indicating
   the success of the operation. */
bool remove(const char *file)
{
  lock_acquire(&lock_filesys);
  bool was_removed = filesys_remove(file);
  lock_release(&lock_filesys);
  return was_removed;
}

/* Opens a file with the given name, and returns the file descriptor assigned by the
   thread that opened it. Inspiration derived from GitHub user ryantimwilson (see
   Design2.txt for attribution link). */
int open(const char *file)
{
  /* Make sure that only one process can get ahold of the file system at one time. */
  lock_acquire(&lock_filesys);

  struct file *f = filesys_open(file);

  /* If no file was created, then return -1. */
  if (f == NULL)
  {
    lock_release(&lock_filesys);
    return -1;
  }

  /* Create a struct to hold the file/fd, for use in a list in the current process.
     Increment the fd for future files. Release our lock and return the fd as an int. */
  struct thread_file *new_file = malloc(sizeof(struct thread_file));
  new_file->file_addr = f;
  int fd = thread_current()->cur_fd;
  thread_current()->cur_fd++;
  new_file->file_descriptor = fd;
  list_push_front(&thread_current()->file_descriptors, &new_file->file_elem);
  lock_release(&lock_filesys);
  return fd;
}

/* Returns the size, in bytes, of the file open as fd. */
int filesize(int fd)
{
  /* list element to iterate the list of file descriptors. */
  struct list_elem *temp;

  lock_acquire(&lock_filesys);

  /* If there are no files associated with this thread, return -1 */
  if (list_empty(&thread_current()->file_descriptors))
  {
    lock_release(&lock_filesys);
    return -1;
  }

  /* Check to see if the given fd is open and owned by the current process. If so, return
     the length of the file. */
  for (temp = list_front(&thread_current()->file_descriptors); temp != NULL; temp = temp->next)
  {
    struct thread_file *t = list_entry(temp, struct thread_file, file_elem);
    if (t->file_descriptor == fd)
    {
      lock_release(&lock_filesys);
      return (int)file_length(t->file_addr);
    }
  }

  lock_release(&lock_filesys);

  /* Return -1 if we can't find the file. */
  return -1;
}

/* Reads size bytes from the file open as fd into buffer. Returns the number of bytes actually read
   (0 at end of file), or -1 if the file could not be read (due to a condition other than end of file).
   Fd 0 reads from the keyboard using input_getc(). */
int read(int fd, void *buffer, unsigned length)
{
  /* list element to iterate the list of file descriptors. */
  struct list_elem *temp;

  lock_acquire(&lock_filesys);

  /* If fd is one, then we must get keyboard input. */
  if (fd == 0)
  {
    lock_release(&lock_filesys);
    return (int)input_getc();
  }

  /* We can't read from standard out, or from a file if we have none open. */
  if (fd == 1 || list_empty(&thread_current()->file_descriptors))
  {
    lock_release(&lock_filesys);
    return 0;
  }

  /* Look to see if the fd is in our list of file descriptors. If found,
     then we read from the file and return the number of bytes written. */
  for (temp = list_front(&thread_current()->file_descriptors); temp != NULL; temp = temp->next)
  {
    struct thread_file *t = list_entry(temp, struct thread_file, file_elem);
    if (t->file_descriptor == fd)
    {
      lock_release(&lock_filesys);
      int bytes = (int)file_read(t->file_addr, buffer, length);
      return bytes;
    }
  }

  lock_release(&lock_filesys);

  /* If we can't read from the file, return -1. */
  return -1;
}

/* Changes the next byte to be read or written in open file fd to position,
   expressed in bytes from the beginning of the file. (Thus, a position
   of 0 is the file's start.) */
void seek(int fd, unsigned position)
{
  /* list element to iterate the list of file descriptors. */
  struct list_elem *temp;

  lock_acquire(&lock_filesys);

  /* If there are no files to seek through, then we immediately return. */
  if (list_empty(&thread_current()->file_descriptors))
  {
    lock_release(&lock_filesys);
    return;
  }

  /* Look to see if the given fd is in our list of file_descriptors. IF so, then we
     seek through the appropriate file. */
  for (temp = list_front(&thread_current()->file_descriptors); temp != NULL; temp = temp->next)
  {
    struct thread_file *t = list_entry(temp, struct thread_file, file_elem);
    if (t->file_descriptor == fd)
    {
      file_seek(t->file_addr, position);
      lock_release(&lock_filesys);
      return;
    }
  }

  lock_release(&lock_filesys);

  /* If we can't seek, return. */
  return;
}

/* Returns the position of the next byte to be read or written in open file fd,
   expressed in bytes from the beginning of the file. */
unsigned tell(int fd)
{
  /* list element to iterate the list of file descriptors. */
  struct list_elem *temp;

  lock_acquire(&lock_filesys);

  /* If there are no files in our file_descriptors list, return immediately, */
  if (list_empty(&thread_current()->file_descriptors))
  {
    lock_release(&lock_filesys);
    return -1;
  }

  /* Look to see if the given fd is in our list of file_descriptors. If so, then we
     call file_tell() and return the position. */
  for (temp = list_front(&thread_current()->file_descriptors); temp != NULL; temp = temp->next)
  {
    struct thread_file *t = list_entry(temp, struct thread_file, file_elem);
    if (t->file_descriptor == fd)
    {
      unsigned position = (unsigned)file_tell(t->file_addr);
      lock_release(&lock_filesys);
      return position;
    }
  }

  lock_release(&lock_filesys);

  return -1;
}

/* Closes file descriptor fd. Exiting or terminating a process implicitly closes
   all its open file descriptors, as if by calling this function for each one. */
void close(int fd)
{
  /* list element to iterate the list of file descriptors. */
  struct list_elem *temp;

  lock_acquire(&lock_filesys);

  /* If there are no files in our file_descriptors list, return immediately, */
  if (list_empty(&thread_current()->file_descriptors))
  {
    lock_release(&lock_filesys);
    return;
  }

  /* Look to see if the given fd is in our list of file_descriptors. If so, then we
     close the file and remove it from our list of file_descriptors. */
  for (temp = list_front(&thread_current()->file_descriptors); temp != NULL; temp = temp->next)
  {
    struct thread_file *t = list_entry(temp, struct thread_file, file_elem);
    if (t->file_descriptor == fd)
    {
      file_close(t->file_addr);
      list_remove(&t->file_elem);
      lock_release(&lock_filesys);
      return;
    }
  }

  lock_release(&lock_filesys);

  return;
}

/* Check to make sure that the given pointer is in user space,
   and is not null. We must exit the program and free its resources should
   any of these conditions be violated. */
void check_valid_addr(const void *ptr_to_check)
{
  /* Terminate the program with an exit status of -1 if we are passed
     an argument that is not in the user address space or is null. Also make
     sure that pointer doesn't go beyond the bounds of virtual address space.  */
  if (!is_user_vaddr(ptr_to_check) || ptr_to_check == NULL || ptr_to_check < (void *)0x08048000)
  {
    /* Terminate the program and free its resources */
    exit(-1);
  }
}

/* Ensures that each memory address in a given buffer is in valid user space. */
void check_buffer(void *buff_to_check, unsigned size)
{
  unsigned i;
  char *ptr = (char *)buff_to_check;
  for (i = 0; i < size; i++)
  {
    check_valid_addr((const void *)ptr);
    ptr++;
  }
}

/* Code inspired by GitHub Repo created by ryantimwilson (full link in Design2.txt).
   Get up to three arguments from a programs stack (they directly follow the system
   call argument). */
void get_stack_arguments(struct intr_frame *f, int *args, int num_of_args)
{
  int i;
  int *ptr;
  for (i = 0; i < num_of_args; i++)
  {
    ptr = (int *)f->esp + i + 1;
    check_valid_addr((const void *)ptr);
    args[i] = *ptr;
  }
}

/* Remove mapping M from the virtual address space,
   writing back any pages that have changed. */
static void
unmap (struct mapping *m)
{
  /* Remove this mapping from the list of mappings for this process. */
  list_remove(&m->elem);

  /* For each page in the memory mapped file... */
  for(int i = 0; i < m->page_cnt; i++)
  {
    /* ...determine whether or not the page is dirty (modified). If so, write that page back out to disk. */
    if (pagedir_is_dirty(thread_current()->pagedir, ((const void *) ((m->base) + (PGSIZE * i)))))
    {
      lock_acquire (&fs_lock);
      file_write_at(m->file, (const void *) (m->base + (PGSIZE * i)), (PGSIZE*(m->page_cnt)), (PGSIZE * i));
      lock_release (&fs_lock);
    }
  }

  /* Finally, deallocate all memory mapped pages (free up the process memory). */
  for(int i = 0; i < m->page_cnt; i++)
  {
    page_deallocate((void *) ((m->base) + (PGSIZE * i)));
  }
}

/* Mmap system call. */
static int
sys_mmap (int handle, void *addr)
{
  struct file_descriptor *fd = lookup_fd (handle);
  struct mapping *m = malloc (sizeof *m);
  size_t offset;
  off_t length;

  if (m == NULL || addr == NULL || pg_ofs (addr) != 0)
    return -1;

  m->handle = thread_current ()->next_handle++;
  lock_acquire (&fs_lock);
  m->file = file_reopen (fd->file);
  lock_release (&fs_lock);
  if (m->file == NULL)
    {
      free (m);
      return -1;
    }
  m->base = addr;
  m->page_cnt = 0;
  list_push_front (&thread_current ()->mappings, &m->elem);

  offset = 0;
  lock_acquire (&fs_lock);
  length = file_length (m->file);
  lock_release (&fs_lock);
  while (length > 0)
    {
      struct page *p = page_allocate ((uint8_t *) addr + offset, false);
      if (p == NULL)
        {
          unmap (m);
          return -1;
        }
      p->private = false;
      p->file = m->file;
      p->file_offset = offset;
      p->file_bytes = length >= PGSIZE ? PGSIZE : length;
      offset += p->file_bytes;
      length -= p->file_bytes;
      m->page_cnt++;
    }

  return m->handle;
}

/* Munmap system call. */
static int
sys_munmap (int mapping)
{
  /* Get the map corresponding to the given map id, and attempt to unmap. */
  struct mapping *map = lookup_mapping(mapping);
  unmap(map);
  return 0;
}

/* On thread exit, close all open files and unmap all mappings. */
void
syscall_exit (void)
{
  struct thread *cur = thread_current ();
  struct list_elem *e, *next;

  for (e = list_begin (&cur->fds); e != list_end (&cur->fds); e = next)
    {
      struct file_descriptor *fd = list_entry (e, struct file_descriptor, elem);
      next = list_next (e);
      lock_acquire (&fs_lock);
      file_close (fd->file);
      lock_release (&fs_lock);
      free (fd);
    }

  for (e = list_begin (&cur->mappings); e != list_end (&cur->mappings);
       e = next)
    {
      struct mapping *m = list_entry (e, struct mapping, elem);
      next = list_next (e);
      unmap (m);
    }
}
