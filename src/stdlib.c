/// @file stdlib.c
/// Implements malloc, free, and heap init



#include "exit_codes.h"
#include "null.h"
#include "stdbool.h"
#include "stdlib.h"
#include "syscalls.h"



// Assume 64 bit architecture (we get at least 64-57=7 bits) (5 level paging worst case)
#define __HEAP_MEMORY_USED_NORMAL (1ull << 63)
#define __HEAP_MEMORY_USED_DUMMY (1ull << 62)
#define __HEAP_MEMORY_USED_MMAP (1ull << 61)
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

// Tracks the last footer
static struct footer_t* bottom_ = NULL;

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
  curr_foot = curr_foot == NULL ? header_of(curr_head) : curr_foot;
  struct header_t** prev_next = prev_head ? &prev_head->next : &head_;
  curr_head->next = *prev_next;
  curr_foot->prev = prev_head;
  *prev_next = curr_head;

  struct footer_t** next_prev = curr_head->next == NULL ? &bottom_ : &footer_of(curr_head->next)->prev;
  *next_prev = curr_head;
}

// Removes the given node form the list and marks it as used
// Either header or footer may be null and the function will compute the other, but they shall not both be null
static void remove_from_list(struct header_t* header, struct footer_t* footer) {
  header = header == NULL ? header_of(footer) : header;
  footer = footer == NULL ? footer_of(header) : footer;
  struct header_t** prev = footer->prev == NULL ? &head_ : &footer->prev->next;
  (*prev) = header->next;
  struct footer_t** next = header->next == NULL ? &bottom_ : &footer_of(header->next)->prev;
  *next = footer->prev;
  header->next = __HEAP_MEMORY_USED_NORMAL;
  footer->prev = __HEAP_MEMORY_USED_NORMAL;
}

static void init(void* head, size_t size);

void __heap_init() {
  void* bottom = brk(0);
  // Initialize 1kb
  void* top = brk(bottom + __HEAP_INITIAL_SIZE);
  if (bottom == top) {
    // Failure
    exit(EXIT_FAILURE_BAD_HEAP_INITIALIZATION);
  }
  init(bottom, __HEAP_INITIAL_SIZE);
}

static void init(void* head, size_t size) {
  // Add a dummy footer at the top
  struct footer_t* top = (struct footer_t*)head;
  top->prev = __HEAP_MEMORY_USED_NORMAL | __HEAP_MEMORY_USED_DUMMY;
  // Add the dummy header at the bottom
  struct header_t* bottom = (struct header_t*)(head + size) - 1;
  bottom->next = __HEAP_MEMORY_USED_NORMAL | __HEAP_MEMORY_USED_DUMMY;
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
  bottom_ = bottom;
}

// Extends the heap by n bytes (n >= 32)
static bool extend(size_t n) {
  void* curr_brk = bottom_ + 2;
  void* new_end = brk(curr_brk + n);
  if (curr_brk == new_end) {
    return false;
  }
  // Two things need to be moved here
  // 1: dummy header
  // 2: last footer (above the header)
  // This call to memcpy is undefined if n is less than 32 beause the regions would overlap. 
  // To combat this, this function will never get called with n < 32
  if (bottom_->prev != __HEAP_MEMORY_USED_NORMAL) {
    // memcpy(bottom_, ((void*)bottom_) + n, sizeof(struct header_t) * 2);
    // Move this to the head of the linked list for efficiency purposes
    // First, remove the current element from the linked list

  } else {
    // Just copy the dummy header
    struct header_t* new_dummy_header = bottom_ + n + 1;
    new_dummy_header = bottom_ + 1;

    // Now create the new header/footer (will be at head)
    struct header_t* new_header = bottom_ + 1;
    new_header->next = head_;
    new_header->size = n - (sizeof(struct header_t) * 2);
    head_ = new_header;

    // Add the footer
    struct footer_t* new_footer = new_dummy_header - 1;
    new_footer->prev = 
    new_footer->size = new_header->size;
    // Set the new footer to the last footer
    bottom_ = new_footer;
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
    n = ((n + 4095) & 0xFFFFFFFFFFFFF000);
    // Linux kernel call
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
      remove_from_list(current, NULL);

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
  // Get the previous footer
  struct footer_t* prev_footer = head - 1; 
  struct header_t* next_header = footer_of(head) + 1;
  if (prev_footer->prev == __HEAP_MEMORY_USED_NORMAL) {

  }
  return;
}