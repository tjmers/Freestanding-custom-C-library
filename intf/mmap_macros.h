#ifndef __MMAP_MACROS
#define __MMAP_MACROS
/* Protection flags (prot argument) */
#define PROT_NONE       0x0     // page cannot be accessed
#define PROT_READ       0x1     // page can be read
#define PROT_WRITE      0x2     // page can be written
#define PROT_EXEC       0x4     // page can be executed
#define PROT_GROWSDOWN  0x01000000  // extend change to growsdown vma (stack)
#define PROT_GROWSUP    0x02000000  // extend change to growsup vma

/* Map flags (flags argument) */
#define MAP_SHARED          0x01    // share changes with other mappings
#define MAP_PRIVATE         0x02    // changes are private (copy-on-write)
#define MAP_SHARED_VALIDATE 0x03    // like MAP_SHARED but validate flags
#define MAP_FIXED           0x10    // interpret addr exactly, replacing existing mappings
#define MAP_ANONYMOUS       0x20    // not backed by a file
#define MAP_ANON            MAP_ANONYMOUS
#define MAP_GROWSDOWN       0x0100  // stack-like segment, grows downward
#define MAP_DENYWRITE       0x0800  // ignored
#define MAP_EXECUTABLE      0x1000  // ignored
#define MAP_LOCKED          0x2000  // lock pages in RAM (like mlock)
#define MAP_NORESERVE       0x4000  // don't reserve swap space
#define MAP_POPULATE        0x08000 // prefault page tables
#define MAP_NONBLOCK        0x10000 // do not block on I/O (with MAP_POPULATE)
#define MAP_STACK           0x20000 // hint: suitable for use as a stack
#define MAP_HUGETLB         0x40000 // use huge pages
#define MAP_SYNC            0x80000 // perform synchronous page faults (DAX only)
#define MAP_FIXED_NOREPLACE 0x100000 // like MAP_FIXED but fail if mapping exists
#define MAP_UNINITIALIZED   0x4000000 // don't clear anonymous pages (embedded only)

/* Return value on failure */
#define MAP_FAILED          ((void*)-1)

/* mremap flags */
#define MREMAP_MAYMOVE      0x1     // kernel may relocate the mapping
#define MREMAP_FIXED        0x2     // place mapping at new_address exactly
#define MREMAP_DONTUNMAP    0x4     // don't unmap the source after remapping

/* madvise hints (madv argument) */
#define MADV_NORMAL         0       // no special treatment
#define MADV_RANDOM         1       // expect random page access
#define MADV_SEQUENTIAL     2       // expect sequential page access
#define MADV_WILLNEED       3       // will need these pages soon
#define MADV_DONTNEED       4       // don't need these pages (kernel may free them)
#define MADV_FREE           8       // pages may be freed lazily
#define MADV_REMOVE         9       // remove shared/file-backed pages
#define MADV_DONTFORK       10      // don't copy these pages on fork
#define MADV_DOFORK         11      // undo MADV_DONTFORK
#define MADV_MERGEABLE      12      // enable KSM merging
#define MADV_UNMERGEABLE    13      // disable KSM merging
#define MADV_HUGEPAGE       14      // enable transparent huge pages
#define MADV_NOHUGEPAGE     15      // disable transparent huge pages
#define MADV_DONTDUMP       16      // exclude from core dump
#define MADV_DODUMP         17      // undo MADV_DONTDUMP
#define MADV_HWPOISON       100     // poison a page (testing only, requires CAP_SYS_ADMIN)
#endif