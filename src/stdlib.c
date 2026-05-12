/// @file stdlib.c
/// Implements malloc, free, and heap init



#include "../intf/bytes.h"
#include "../intf/error.h"
#include "../intf/exit.h"
#include "../intf/memory.h"
#include "../intf/stddef.h"
#include "../intf/stdlib.h"
#include "../intf/string.h"
#include "../intf/syscalls.h"



// Assume 64 bit architecture (we get at least 64-57=7 bits) (5 level paging worst case)
#define __HEAP_MEMORY_USED_NORMAL (1ull << 63)
#define __HEAP_MEMORY_USED_MMAP (1ull << 62)
// Minimum size (will always be page-aligned so this is actually 4kb)
#define __HEAP_INITIAL_SIZE KB

struct header_t {
  union {
    struct header_t* next;
    uintptr_t magic;
  } info;
  size_t size;
};

struct footer_t {
  union {
    struct header_t* prev;
    uintptr_t magic;
  } info;
  size_t size;
};

// Necessary so that pointer arithmetic doesn't break
// static_assert(sizeof(struct header_t) == sizeof(struct footer_t));

// Tracks the program break
static void* p_brk_ = NULL;

// Only track the top if testing
#ifdef __LIBC_TEST
static uintptr_t heap_start_ = NULL;
#endif

// The head of the linked list of unallocated memory
static struct header_t* head_ = NULL;

static struct header_t* header_of(struct footer_t* footer) {
  return (struct header_t*)((char*)(footer - 1) - footer->size);
}

static struct footer_t* footer_of(struct header_t* header) {
  return (struct footer_t*)((char*)(header + 1) + header->size);
}

/// @brief Inserts the given node into the linked list, after prev
/// @param prev_head The header for the node that will come before the newly inserted node
/// @param curr_head The header for the node to insert
/// @param curr_foot The footer for the node to insert
/// @details Either curr_head or curr_foot must not be null
static void insert_into_list(struct header_t* prev_head, struct header_t* curr_head, struct footer_t* curr_foot) {
    
  // Assign each parameter a non-null value
  curr_head = curr_head == NULL ? header_of(curr_foot) : curr_head;
  curr_foot = curr_foot == NULL ? footer_of(curr_head) : curr_foot;
  struct header_t** prev_next = prev_head ? &prev_head->info.next : &head_;
  curr_head->info.next = *prev_next;
  curr_foot->info.prev = prev_head;
  *prev_next = curr_head;

  if (curr_head->info.next != NULL) {
    footer_of(curr_head->info.next)->info.prev = curr_head;
  }
}

// Removes the given node form the list
// Either header or footer may be null and the function will compute the other, but they shall not both be null
static void remove_from_list(struct header_t* header, struct footer_t* footer) {
  header = header == NULL ? header_of(footer) : header;
  footer = footer == NULL ? footer_of(header) : footer;
  struct header_t* prev = footer->info.prev;
  struct header_t* next = header->info.next;
  if (prev == NULL && next == NULL) {
    head_ = NULL;
  } else if (prev == NULL) {
    head_ = next;
    footer_of(next)->info.prev = NULL;
  } else if (next == NULL) {
    prev->info.next = NULL;
  } else {
    prev->info.next = next;
    footer_of(next)->info.prev = prev;
  }
}

static void init(void* head, size_t size);

void __heap_init() {
  // Move bottom to 16 byte padding so that the padding starts at 16 bytes
  uintptr_t bottom = (uintptr_t)brk(0);
  bottom = (bottom + 15) & ~0xF;
  // Initialize at least 1kb, but always go to the next page
  uintptr_t top = bottom + __HEAP_INITIAL_SIZE + 4095;
  top &= ~0xfff;
  p_brk_ = brk((void*)top);
  if ((uintptr_t)p_brk_ != top) {
    // Failure
    exit(EXIT_FAILURE_BAD_HEAP_INITIALIZATION);
  }

  init((void*)bottom, top - (uintptr_t)bottom);
}

static void init(void* head, size_t size) {
  // Add a dummy footer at the top
#ifdef __LIBC_TEST
  heap_start_ = (uintptr_t)head;
#endif
  struct footer_t* top = (struct footer_t*)head;
  top->info.magic = __HEAP_MEMORY_USED_NORMAL;
  // Add the dummy header at the bottom
  struct header_t* bottom = (struct header_t*)((char*)head + size) - 1;
  bottom->info.magic = __HEAP_MEMORY_USED_NORMAL;
  // Add the first header
  struct header_t* first_header = (struct header_t*)top + 1;
  first_header->info.next = NULL;
  first_header->size = size - (sizeof(struct header_t) * 4);
  // Add the first header's footer
  struct footer_t* first_footer = (struct footer_t*)bottom - 1;
  first_footer->size = first_header->size;
  first_footer->info.prev = NULL;
  // Set heap top
  head_ = first_header;
#ifdef __LIBC_TEST
  // Write to console
  const char pre[] = "Heap initialized to ";
  const char post[] = " bytes.\n";
  char buffer[64];
  strcpy(buffer, pre);
  uint32_t index = sizeof(pre) - 1;
  int64_t tmp_size = (int64_t)size;
  index += itoa(&tmp_size, &buffer[index], 64 - index);
  strcpy(&buffer[index], post);
  write(1, buffer, index + sizeof(post) - 1);
#endif
}

// Extends the heap by n bytes (n >= 32)
static bool extend(size_t n) {
  uintptr_t new_end = (uintptr_t)p_brk_ + n;

  // Pad new_end to the nearest 4kb
  new_end += 4095;
  new_end &= ~0xfff;

  struct footer_t* bottom = (struct footer_t*)p_brk_ - 2;
  new_end = (uintptr_t)brk((void*)new_end);
  if ((uintptr_t)p_brk_ == new_end) {
    // brk fail
    return false;
  }
  p_brk_ = (void*)new_end;
  // Two things need to be moved here
  // 1: dummy header
  // 2: last footer (above the header)
  // This call to memcpy is undefined if n is less than 32 beause the regions would overlap. 
  // To combat this, this function will never get called with n < 32 (prevented since its always aligned to 4kb)
  // Conver p_brk to get bottom
  if (bottom->info.magic != __HEAP_MEMORY_USED_NORMAL) {
    memcpy_small(bottom, ((char*)bottom) + n, sizeof(struct header_t) * 2);
    // Move this to the head of the linked list for efficiency purposes
    // First, remove the current element from the linked list

  } else {
    // Just copy the dummy header
    struct header_t* new_dummy_header = (struct header_t*)((char*)bottom + n) + 1;
    new_dummy_header = (struct header_t*)bottom + 1;

    // Now create the new header/footer (will be at head)
    struct header_t* new_header = (struct header_t*)bottom + 1;
    new_header->info.next = head_;
    new_header->size = n - (sizeof(struct header_t) * 2);
    head_ = new_header;

    // Add the footer
    struct footer_t* new_footer = (struct footer_t*)new_dummy_header - 1;
    new_footer->info.prev = NULL;
    new_footer->size = new_header->size;
    // Set the new footer to the last footer
    bottom = new_footer;
  }
  return true;
}

void* malloc(size_t n) {
  __malloc_lock();
malloc_after_lock:
  // Align to 16 bytes
  n += 15;
  n &= ~0xF;

  // n >= 128KB (2^17)
  if (n >= (1 << 17)) {
    // Just call mmap
    // Must also allocate a header for free
    n += sizeof(struct header_t);
    // Now round up to nearest 4KB (page size)
    n = ((n + 4095) & 0xFFFFFFFFFFFFF000ull);
    // Call mmap
    void* ptr = mmap(n, NULL, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
      __malloc_unlock();
      return NULL;
    }
    struct header_t* header = ptr;
    header->info.magic = __HEAP_MEMORY_USED_MMAP;
    // This is typically not how it works (it would usually be n - sizeof(struct header_t)) - but since headers of mmaped memory are seperate from normal heap memory this is okay.
    header->size = n;

    // There is no footer by design
    __malloc_unlock();
    return (void*)((char*)ptr + sizeof(struct header_t));
  }
  // Check if there is enough space in the heap
  struct header_t* current = head_;
  // Allocates in the first spot that fits it
  while (current) {
    if (current->size >= n) {
      // Allocate here
      if (current->size - n > sizeof(struct header_t) + sizeof(struct footer_t) + 8) { // 8 acts as a buffer so that there's not just random tiny chunks (should probably be larger) (it doesn't even actually do anything beacuse its all 16-byte aligned)
        // It can be broken up into a smaller chunk (do it)
        struct header_t* next_header = (struct header_t*)((char*)(current + 2) + n); // + 2 because of header and footer
        // Insert into linked list
        // First update the sizes so that footer_of and header_of work correctly
        size_t total_block_size = current->size;
        current->size = n;
        footer_of(current)->size = n;
        next_header->size = total_block_size - n - (sizeof(struct header_t) + sizeof(struct footer_t));
        struct footer_t* next_footer = footer_of(next_header);
        next_footer->size = next_header->size;
        insert_into_list(current, next_header, next_footer);
      }
      // Remove the current from the linked list and mark as used
      struct footer_t* footer = footer_of(current);
      remove_from_list(current, footer);
      current->info.magic = __HEAP_MEMORY_USED_NORMAL;
      footer->info.magic = __HEAP_MEMORY_USED_NORMAL;
      // Return the memory after the header
      __malloc_unlock();
      return current + 1;
    }
    current = current->info.next;
  }
  // No available space
  // Grow the heap
  if (n < 32) {
    n = 32;
  }
  if (!extend(n)) {
    __malloc_unlock();
    return NULL;
  }
  goto malloc_after_lock;
}

void free(void* ptr) {
  if (!ptr) return;
  struct header_t* head = ptr;
  // Move to the actual header
  --head;

  if (head->info.magic == __HEAP_MEMORY_USED_MMAP) {
    munmap(head, head->size);
    return;
  }


  // Free is strict in that it is UB to free twice or to free random stuff, so just assume that head->next == __HEAP_MEMORY_USED_NORMAL

  // Coalesce
  // Get the previous footer and next header
  struct footer_t* prev_footer = (struct footer_t*)head - 1; 
  struct header_t* next_header = (struct header_t*)footer_of(head) + 1;

  // Add the freed node to the head of the linked list
  insert_into_list(NULL, head, NULL);
  if (prev_footer->info.magic != __HEAP_MEMORY_USED_NORMAL) {
    struct header_t* new_head = header_of(prev_footer);
    // This memory block cannot be the head of the list since a node was just added to the head
    remove_from_list(new_head, prev_footer);
    new_head->size += head_->size + 2 * sizeof(struct header_t);
    struct footer_t* new_footer = footer_of(new_head);
    new_footer->size = new_head->size;
    new_footer->info.prev = NULL;
    new_head->info.next = head_->info.next;
    // Move the head back
    head_ = new_head;
    if (head_->info.next) {
      footer_of(head_->info.next)->info.prev = head_;
    }
  }
  if (next_header->info.magic != __HEAP_MEMORY_USED_NORMAL) {
    // Remove this memory block from the list and add the size of it to head
    remove_from_list(next_header, NULL);
    head_->size += next_header->size + 2 * sizeof(struct header_t);
    struct footer_t* new_footer = footer_of(head_);
    new_footer->size = head_->size;
    // (its the head of the list)
    new_footer->info.prev = NULL;
  }
}

__attribute__((weak)) void __malloc_lock(void) {}
__attribute__((weak)) void __malloc_unlock(void) {}

#ifdef __LIBC_TEST
size_t __heap_free_list_length(void) {
  size_t count = 0;
  struct header_t* curr = head_;
  while (curr) {
    ++count;
    curr = curr->info.next;
  }
  return count;
}

size_t __heap_free_total_size(void) {
  size_t total = 0;
  struct header_t* curr = head_;
  while (curr) {
    total += curr->size;
    curr = curr->info.next;
  }
  return total;
}

uintptr_t __heap_start(void) {
  return (uintptr_t)heap_start_;
}

uintptr_t __heap_end(void) {
  return (uintptr_t)p_brk_;
}

void __print_heap(void) {
  struct header_t* curr_head = (struct header_t*)heap_start_;
  struct header_t* prev_head = NULL;
  ++curr_head;
  const char sep[] = "----------------\n";
  write(1, sep, sizeof(sep) - 1);
  while ((uintptr_t)(curr_head + 1) != (uintptr_t)p_brk_) {
    struct footer_t* curr_foot = footer_of(curr_head);
    // Print information about the current head
    // First address
    // (This function does not need to be efficient)
    char buffer[256];
    const char first[] = "[start + ";
    uint32_t i = 0;
    strcpy(buffer, first);
    i += sizeof(first) - 1;
    // Copy in the current address
    uintptr_t curr_addr = (uintptr_t)curr_head;
    int64_t offset = (int64_t)(curr_addr - (uintptr_t)heap_start_);
    i += itoa(&offset, &buffer[i], 256 - i);
    const char second[] = "] Size: ";
    strcpy(&buffer[i], second);
    i += sizeof(second) - 1;
    int64_t sz = (int64_t)curr_head->size;
    i += itoa(&sz, &buffer[i], 256 - i);


    if (curr_head->info.magic == __HEAP_MEMORY_USED_NORMAL) {
      const char third[] = ", memory in use.";
      strcpy(&buffer[i], third);
      i += sizeof(third) - 1;
    } else if (curr_head->info.next == NULL) {
      const char third[] = ", next: [0]";
      strcpy(&buffer[i], third);
      i += sizeof(third) - 1;
    } else {
      const char third[] = ", next: [start + ";
      strcpy(&buffer[i], third);
      i += sizeof(third) - 1;
      uintptr_t next_addr = (uintptr_t)curr_head->info.next;
      int64_t next_diff = (int64_t)(next_addr - (uintptr_t)heap_start_);
      i += itoa(&next_diff, &buffer[i], 256 - i);
      buffer[i++] = ']';
    }
    if (curr_head->info.magic != __HEAP_MEMORY_USED_NORMAL) {
      if (curr_foot->info.prev == NULL) {
        const char fourth[] = ", prev: [0]";
        strcpy(&buffer[i], fourth);
        i += sizeof(fourth) - 1;
      } else {
        const char fourth[] = ", prev: [start + ";
        strcpy(&buffer[i], fourth);
        i += sizeof(fourth) - 1;
        uintptr_t next_addr = (uintptr_t)curr_foot->info.prev;
        int64_t next_diff = (int64_t)(next_addr - (uintptr_t)heap_start_);
        i += itoa(&next_diff, &buffer[i], 256 - i);
        buffer[i++] = ']';
      }
    }
    if (curr_head == head_) {
      const char head_str[] = " <------ HEAD";
      strcpy(&buffer[i], head_str);
      i += sizeof(head_str) - 1;
    }
    buffer[i++] = '\n';
    write(1, buffer, i);
    
    // Update curr_head
    prev_head = curr_head;
    curr_head = (struct header_t*)footer_of(curr_head) + 1;
    if (curr_head == prev_head) {
      // Infinite loop detected (very basic, does not use a real algorithm to determine if there is a loop with fast/slow pointer)
      write(1, "Infinite loop detected -- terminating function.\n", 48);
      write(1, sep, sizeof(sep) - 1);
      return;
    }
  }
  write(1, sep, sizeof(sep) - 1);
}
#endif
