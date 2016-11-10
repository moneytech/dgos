
#include "types.h"

/// Failure return value for memory mapping functions
#define MAP_FAILED ((void*)-1)

///
/// mmap __prot

#define PROT_READ           0x01
#define PROT_WRITE          0x02
#define PROT_EXEC           0x04

///
/// mmap __flags

/// Map at specified address, otherwise, just fail
#define MAP_FIXED           0x00000001

/// Lock the mapped region in memory
#define MAP_LOCKED          0x00000002

/// Ignored. Hint that pages will be used as stacks
/// Just in case platform needs to use special memory
/// area for stacks
#define MAP_STACK           0x00000004

/// Mapping grows downward in memory
#define MAP_GROWSDOWN       0x00000008

/// Use huge pages
#define MAP_HUGETLB         0x00000010

/// Don't commit swap space for uncommitted pages,
/// just SIGSEGV if an uncommitted page is touched
/// when there is insufficient space
#define MAP_NORESERVE       0x00000020

/// Don't do demand paging, actually commit pages up front
/// Hint file mappings to read ahead
#define MAP_POPULATE        0x00000040

/// If allowed, skip clearing the pages to zero
#define MAP_UNINITIALIZED   0x00000080

/// Map below 2GB
#define MAP_32BIT           0x00000100

/// Not file backed
#define MAP_ANONYMOUS       0x00000200

/// Kernel only: Disable caching
#define MAP_NOCACHE         0x40000000

/// Kernel only: The provided address is a physical address
#define MAP_PHYSICAL        0x80000000

/// Ignored. Redundant.
#define MAP_DENYWRITE       0
#define MAP_EXECUTABLE      0
#define MAP_FILE            0
#define MAP_NONBLOCK        0

/// Map a range of address space
/// __addr, hint, map memory as near as possible to that address,
/// unless MAP_FIXED is used
/// __len, size, in bytes
/// __prot, bitmask, PROT_EXEC PROT_READ PROT_WRITE PROT_NONE
/// __flags, bitmask, MAP_*
/// __fd, file descriptor, file backed mapping
/// __offset, file pointer, position in file to start mapping
void *mmap(
        void *__addr,
        size_t __len,
        int __prot,
        int __flags,
        int __fd,
        off_t __offset);

/// Unmap a range of address space
/// Unmap the range from __addr to __len
int munmap(
        void *__addr,
        size_t __len);

/// Permit a mapping to be moved to a new address
/// Without this flag, mremap fails if it cannot be resized
#define MREMAP_MAYMOVE

/// Move the mapping to the specified __new_address
/// Fail if not possible
#define MREMAP_FIXED

/// Move and/or resize a range of address space
/// __old_address, address of mapping to be remapped
/// __old_size, the quantity of address space to be moved
/// __new_size, the quantity of address space needed after the move
/// __flags, bitmaps, MREMAP_*
/// __new_address, ignored unless MREMAP_FIXED was specified
void *mremap(
        void *__old_address,
        size_t __old_size,
        size_t __new_size,
        int __flags,
        ... /* void *__new_address */);

/// Change protection on a range of memory
/// __addr, address, start of range
/// __len, bytes, length of range
/// __prot, bitfield, PROT_*
int mprotect(void *__addr, size_t __len, int __prot);

/// Advise normal behavior
#define MADV_NORMAL         0

/// Advise that read-ahead is probably useful
#define MADV_SEQUENTIAL     1

/// Advise that read-ahead probably won't help
#define MADV_RANDOM         101

/// Advise that this range is going to be needed soon
#define MADV_WILLNEED       2

/// Advise that this region can be discarded, and replaced
/// with zero pages or reread from the backing file if it
/// is touched again
#define MADV_DONTNEED       102

/// Advise that this region can be discarded and deleted
/// from the backing file
#define MADV_REMOVE         7

/// Advise that this region should be cloned when forking
#define MADV_DOFORK         3

/// Advise that this region should not be cloned when forking
#define MADV_DONTFORK       103

/// Advise that there is something wrong with the RAM in this region
#define MADV_HWPOISON       8

/// Advise that we should pretend that there is something wrong with
/// the RAM in this region
#define MADV_SOFT_OFFLINE   108

/// Advise that this page should be considered in background page
/// deduplication
#define MADV_MERGEABLE      4

/// Advise that this page should not be considered in background page
/// deduplication
#define MADV_UNMERGEABLE    104

/// Advise that this region is a good candidate for huge pages
#define MADV_HUGEPAGE       5

/// Advise that this region is not a good candidate for huge pages
#define MADV_NOHUGEPAGE     105

// Advise that this region is probably useful in core dumps
#define MADV_DODUMP         6

/// Advise that this region is probably not useful in core dumps
#define MADV_DONTDUMP       106

/// Set hints about a range of address space
/// __addr, address, start of range
/// __len, size, of range
/// __advice, bitfield, MADV_*
int madvise(void *__addr, size_t __len, int __advice);

/// Lock a range of address space in memory
int mlock(
        void const *addr,
        size_t len);

/// Unlock a range of address space
int munlock(
        const void *addr,
        size_t len);

/// Lock all process address space in memory
#define MCL_CURRENT

/// Lock future address space operations in memory
#define MCL_FUTURE

/// Lock all process memory
int mlockall(int flags);

/// Unlock all process memory
int munlockall(void);

/// Set the program break to the specified value
int brk(void *addr);

/// Adjust the program break by specified value
void *sbrk(intptr_t increment);

/// Absolute minimum limits
#define _POSIX_ARG_MAX (4096)
#define _POSIX_CHILD_MAX (25)
#define _POSIX_HOST_NAME_MAX (255)
#define _POSIX_LOGIN_NAME_MAX (9)
#define _POSIX_OPEN_MAX (20)
#define _POSIX2_RE_DUP_MAX (255)
#define _POSIX_STREAM_MAX (8)
#define _POSIX_SYMLOOP_MAX (8)
#define _POSIX_TTY_NAME_MAX (9)
#define _POSIX_TZNAME_MAX (6)

/// Maximum number of arguments to exec functions
#define _SC_ARG_MAX
#define ARG_MAX _SC_ARG_MAX

/// Maximum number of processes per user
#define _SC_CHILD_MAX
#define CHILD_MAX _SC_CHILD_MAX

/// Maximum length of host name, not including null
#define _SC_HOST_NAME_MAX
#define HOST_NAME_MAX _SC_HOST_NAME_MAX

/// Maximum length of a login name
#define _SC_LOGIN_NAME_MAX
#define LOGIN_NAME_MAX _SC_LOGIN_NAME_MAX

/// Number of clock ticks per second. Must be 1000000
#define _SC_CLK_TCK

/// Maximum number of open files
#define _SC_OPEN_MAX
#define OPEN_MAX _SC_OPEN_MAX

/// Size of page in bytes. Must be >= 1
#define _SC_PAGESIZE
#define PAGESIZE _SC_PAGESIZE

/// The number of repeated occurrences of a BRE premitted by regex and regcomp
#define _SC_RE_DUP_MAX
#define RE_DUP_MAX _SC_RE_DUP_MAX

/// Maximum number of streams that a process can have open
#define _SC_STREAM_MAX
#define STREAM_MAX _SC_STREAM_MAX

/// Maximum number of symbolic links to permit in a path resolution
#define _SC_SYMLOOP_MAX
#define SYMLOOP_MAX _SC_SYMLOOP_MAX

/// Maximum length of a terminal device name
#define _SC_TTY_NAME_MAX
#define TTY_NAME_MAX _SC_TTY_NAME_MAX

/// Maximum length of a timezone name
#define _SC_TZNAME_MAX
#define TZNAME_MAX _SC_TZNAME_MAX

/// Year and month of POSIX revision 199009L
#define _SC_VERSION
#define _POSIX_VERSION _SC_VERSION

/// The number of physical pages of memory
#define _SC_PHYS_PAGES

/// The number of available physical pages of memory
#define _SC_AVPHYS_PAGES

/// The number of processors
#define _SC_NPROCESSORS_CONF

/// The number of online processors
#define _SC_NPROCESSORS_ONLN

/// Query system configuration
long sysconf(int __name);