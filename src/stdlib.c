/// @file stdlib.c
/// Implements malloc, free, and heap init



#include "error.h"
#include "exit_codes.h"
#include "stddef.h"
#include "stdlib.h"
#include "syscalls.h"



// Assume 64 bit architecture (we get at least 64-57=7 bits) (5 level paging worst case)
#define __HEAP_MEMORY_USED_NORMAL (1ull << 63)
#define __HEAP_MEMORY_USED_MMAP (1ull << 62)
#define __HEAP_INITIAL_SIZE 1024

struct header_t {
  struct header_t* next;
  size_t size;
};

struct footer_t {
  struct header_t* prev;
  size_t size;
};

// Necessary so that pointer arithmetic doesn't break
// static_assert(sizeof(struct header_t) == sizeof(struct footer_t));

// Tracks the program break
static void* p_brk_ = NULL;

// The head of the linked list of unallocated memory
static struct header_t* head_ = NULL;

static struct header_t* header_of(struct footer_t* footer) {
  return (void*)(footer - 1) - footer->size;
}

static struct footer_t* footer_of(struct header_t* header) {
  return (void*)(header + 1) + header->size;
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
  struct header_t** prev_next = prev_head ? &prev_head->next : &head_;
  curr_head->next = *prev_next;
  curr_foot->prev = prev_head;
  *prev_next = curr_head;

  if (curr_head->next != NULL) {
    footer_of(curr_head->next)->prev = curr_head;
  }
}

// Removes the given node form the list and marks it as used
// Either header or footer may be null and the function will compute the other, but they shall not both be null
static void remove_from_list(struct header_t* header, struct footer_t* footer) {
  header = header == NULL ? header_of(footer) : header;
  footer = footer == NULL ? footer_of(header) : footer;
  struct header_t** prev = footer->prev == NULL ? &head_ : &footer->prev->next;
  (*prev) = header->next;
  if (header->next != NULL) {
    footer_of(header->next)->prev = footer->prev;
  }
}

static void init(void* head, size_t size);

void __heap_init() {
  // Move bottom to 16 byte padding so that the padding starts at 16 bytes
  uintptr_t bottom = brk(0);
  bottom = (bottom + 15) & ~0xF;
  // Initialize at least 1kb, but always go to the next page
  uintptr_t top = bottom + __HEAP_INITIAL_SIZE + 4095;
  top &= ~0xfff;
  p_brk_ = brk(top);
  if (p_brk_ != top) {
    // Failure
    exit(EXIT_FAILURE_BAD_HEAP_INITIALIZATION);
  }

  init(bottom, top - (uintptr_t)bottom);
}

static void init(void* head, size_t size) {
  // Add a dummy footer at the top
  struct footer_t* top = (struct footer_t*)head;
  top->prev = __HEAP_MEMORY_USED_NORMAL;
  // Add the dummy header at the bottom
  struct header_t* bottom = (struct header_t*)(head + size) - 1;
  bottom->next = __HEAP_MEMORY_USED_NORMAL;
  // Add the first header
  ++top;
  top->prev = NULL;
  top->size = size - (sizeof(struct header_t) * 2 + sizeof(struct footer_t));
  // Add the first header's footer
  --bottom;
  bottom->size = top->size;
  bottom->next = top->prev;
  // Set heap top and bottom
  head_ = top;
}

// Extends the heap by n bytes (n >= 32)
static bool extend(size_t n) {
  uintptr_t new_end = p_brk_ + n;

  // Pad new_end to the nearest 4kb
  new_end += 4095;
  new_end &= ~0xfff;

  struct footer_t* bottom = (struct footer_t*)p_brk_ - 2;
  new_end = brk(new_end);
  if (p_brk_ == new_end) {
    // brk fail
    return false;
  }
  p_brk_ = new_end;
  // Two things need to be moved here
  // 1: dummy header
  // 2: last footer (above the header)
  // This call to memcpy is undefined if n is less than 32 beause the regions would overlap. 
  // To combat this, this function will never get called with n < 32 (prevented since its always aligned to 4kb)
  // Conver p_brk to get bottom
  if (bottom->prev != __HEAP_MEMORY_USED_NORMAL) {
    memcpy_small(bottom, ((void*)bottom) + n, sizeof(struct header_t) * 2);
    // Move this to the head of the linked list for efficiency purposes
    // First, remove the current element from the linked list

  } else {
    // Just copy the dummy header
    struct header_t* new_dummy_header = bottom + n + 1;
    new_dummy_header = bottom + 1;

    // Now create the new header/footer (will be at head)
    struct header_t* new_header = bottom + 1;
    new_header->next = head_;
    new_header->size = n - (sizeof(struct header_t) * 2);
    head_ = new_header;

    // Add the footer
    struct footer_t* new_footer = new_dummy_header - 1;
    new_footer->prev = NULL;
    new_footer->size = new_header->size;
    // Set the new footer to the last footer
    bottom = new_footer;
  }
  return true;
}

void* malloc(size_t n) {
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
    void* ptr = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return NULL;
    struct header_t* header = ptr;
    header->next = __HEAP_MEMORY_USED_MMAP;
    header->size = n;

    // There is no footer by design
    return ptr + sizeof(struct header_t);
  }
  // Check if there is enough space in the heap
  struct header_t* current = head_;
  // Allocates in the first spot that fits it
  while (current) {
    if (current->size >= n) {
      // Allocate here
      if (current->size - n > sizeof(struct header_t) + sizeof(struct footer_t) + 8) { // 8 acts as a buffer so that there's not just random tiny chunks (should probably be larger)
        // It can be broken up into a smaller chunk (do it)
        struct header_t* next_header = (void*)(current + 2) + n; // + 2 because of header and footer
        // Insert into linked list
        // First update the sizes so that footer_of and header_of work correctly
        current->size = n;
        footer_of(current)->size = n;
        next_header->size = current->size - n - (sizeof(struct header_t) + sizeof(struct footer_t));
        struct footer_t* next_footer = footer_of(next_header);
        next_footer->size = next_header->size;
        insert_into_list(current, next_header, next_footer);
      }
      // Remove the current from the linked list and mark as used
      struct footer_t* footer = footer_of(current);
      remove_from_list(current, footer);
      current->next = __HEAP_MEMORY_USED_NORMAL;
      footer->prev = __HEAP_MEMORY_USED_NORMAL;
      // Return the memory after the header
      return current + 1;
    }
    current = current->next;
  }
  // No available space
  // Grow the heap
  if (n < 32) {
    n = 32;
  }
  extend(n);
  return malloc(n);
}


void free(void* ptr) {
  if (!ptr) return;
  struct header_t* head = ptr;
  // Move to the actual header
  --head;

  if (head->next == __HEAP_MEMORY_USED_MMAP) {
    munmap(head, head->size);
    return;
  }


  // Free is strict in that it is UB to free twice or to free random stuff, so just assume that head->next == __HEAP_MEMORY_USED_NORMAL

  // Coalesce
  // Get the previous footer and next header
  struct footer_t* prev_footer = head - 1; 
  struct header_t* next_header = footer_of(head) + 1;

  // Add the freed node to the head of the linked list
  insert_into_list(NULL, head, NULL);
  if (prev_footer->prev != __HEAP_MEMORY_USED_NORMAL) {
    struct header_t* new_head = header_of(prev_footer);
    // This memory block cannot be the head of the list since
    remove_from_list(new_head, prev_footer);
    new_head->size += head_->size + 2 * sizeof(struct header_t);
    footer_of(new_head)->size = new_head->size;
    new_head->next = head_->next;
    head_ = new_head;
  }
  if (next_header->next != __HEAP_MEMORY_USED_NORMAL) {
    // Remove this memory block from the list and add the size of it to head
    head_->size += next_header->size + 2 * sizeof(struct header_t);
    remove_from_list(next_header, NULL);
    footer_of(head_)->size = next_header->size;
  }
  return;
}

uint32_t itoa(int i, char* buffer, size_t buff_size) {
  const char int_min[] = "-2147483684";
  if (i == INT32_MIN) {
    // This one needs to be treated specially since its all messed up

    const char* c = int_min;
    if (buff_size < sizeof(int_min) - 1) {
      return 0;
    }
    memcpy_small(buffer, int_min, sizeof(int_min) - 1);
  }

  char* buff_start = buffer;

  if (buff_size && i < 0) {
    *buffer = '-';
    --buff_size;
    ++buffer;
    i = -i;
  }

  while (buff_size && i) {
    *buffer = (i % 10);
    ++buffer;
    i /= 10;
    --buff_size;
  }
  return buffer - buff_start;
}

#ifdef __LIBC_TEST
size_t __heap_free_list_length(void) {
  size_t count = 0;
  struct header_t* curr = head_;
  while (curr) {
    ++count;
    curr = curr->next;
  }
  return count;
}

size_t __heap_free_total_size(void) {
  size_t total = 0;
  struct header_t* curr = head_;
  while (curr) {
    total += curr->size;
    curr = curr->next;
  }
  return total;
}
#endif
