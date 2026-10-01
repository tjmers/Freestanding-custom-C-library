/// @file malloc_free_break.c
/// Tests that exercise realistic malloc/free usage patterns the existing
/// suite does not cover. Each one targets behaviour that crosses the
/// boundary of the initial heap arena (i.e. forces extend()) or that
/// stresses the coalescing logic over many operations. None of these
/// are standard-corner-case probes (no malloc(SIZE_MAX), no freeing
/// random pointers); they are the kinds of allocation patterns a real
/// program might produce.

#include "../intf/stdlib.h"
#include "../intf/syscalls.h"
#include "../intf/stdint.h"

size_t __heap_free_list_length(void);
size_t __heap_free_total_size(void);
uintptr_t __heap_start(void);
uintptr_t __heap_end(void);

static void write_str(const char* s, uint64_t n) { write(1, s, n); }

static void announce(const char* msg, uint64_t n) {
  write_str("RUN:  ", 6);
  write_str(msg, n);
  write_str("\n", 1);
}

static void report_fail(const char* msg, uint64_t n) {
  write_str("FAIL: ", 6);
  write_str(msg, n);
  write_str("\n", 1);
}

static void report_ok(const char* msg, uint64_t n) {
  write_str("OK:   ", 6);
  write_str(msg, n);
  write_str("\n", 1);
}

#define RUN(s)  do { const char __m[] = (s); announce(__m,     sizeof(__m) - 1); } while (0)
#define FAIL(s) do { const char __m[] = (s); report_fail(__m,  sizeof(__m) - 1); return 1; } while (0)
#define OK(s)   do { const char __m[] = (s); report_ok(__m,    sizeof(__m) - 1); return 0; } while (0)


// Allocate a single block whose size exceeds the initial 4 KB heap
// arena (~4032 bytes free) but stays below the 128 KB mmap threshold.
// This forces extend() on a heap whose top is a free block (the
// initial state), exercising the heap-growth code path that small
// allocations never reach.
static int test_single_alloc_forces_extend(void) {
  RUN("test_single_alloc_forces_extend");

  uintptr_t end_before = __heap_end();
  void* p = malloc(8000);
  if (!p) FAIL("malloc(8000) returned NULL");

  uintptr_t end_after = __heap_end();
  if (end_after <= end_before) {
    FAIL("heap end did not advance after large malloc");
  }

  uintptr_t pi = (uintptr_t)p;
  if (pi < __heap_start() || pi + 8000 > end_after) {
    FAIL("malloc result lies outside the heap range");
  }

  free(p);
  OK("test_single_alloc_forces_extend");
}


// Fill the request range of a large allocation with a known byte
// pattern and read it back. If the allocator placed metadata such
// that any byte of the user payload overlaps a header or footer,
// later writes to neighbouring blocks will alter the pattern.
static int test_large_alloc_payload_integrity(void) {
  RUN("test_large_alloc_payload_integrity");

  size_t n = 8000;
  unsigned char* p = malloc(n);
  if (!p) FAIL("malloc(8000) returned NULL");

  for (size_t i = 0; i < n; ++i) {
    p[i] = (unsigned char)((i * 31u) ^ 0xA5u);
  }

  // Force another allocation between write and read so the second
  // alloc's bookkeeping has a chance to clobber the first payload if
  // metadata overlaps user data.
  void* sentinel = malloc(64);
  if (!sentinel) FAIL("sentinel malloc(64) returned NULL");
  for (int i = 0; i < 64; ++i) ((unsigned char*)sentinel)[i] = 0xCC;

  for (size_t i = 0; i < n; ++i) {
    if (p[i] != (unsigned char)((i * 31u) ^ 0xA5u)) {
      FAIL("payload byte changed between write and read");
    }
  }

  free(sentinel);
  free(p);
  OK("test_large_alloc_payload_integrity");
}


// Pour small allocations into the heap until extend() is required and
// keep going past that boundary. Every returned pointer must lie
// inside the (now-grown) heap and must be distinct from every prior
// allocation. After freeing them all, coalescing should reduce the
// free list to a single block.
static int test_exhaust_then_keep_allocating(void) {
  RUN("test_exhaust_then_keep_allocating");

  enum { CAP = 200 };
  void* ptrs[CAP];
  int n = 0;

  while (n < CAP) {
    void* p = malloc(64);
    if (!p) FAIL("malloc(64) returned NULL during exhaustion phase");
    uintptr_t pi = (uintptr_t)p;
    if (pi < __heap_start() || pi >= __heap_end()) {
      FAIL("malloc returned pointer outside heap range");
    }
    for (int j = 0; j < n; ++j) {
      if (ptrs[j] == p) FAIL("malloc returned a duplicate pointer");
    }
    ptrs[n++] = p;
  }

  for (int i = 0; i < n; ++i) free(ptrs[i]);

  if (__heap_free_list_length() != 1) {
    FAIL("free list did not coalesce to a single block after mass free");
  }
  OK("test_exhaust_then_keep_allocating");
}


// Allocate, free, re-allocate the same large size. This is the
// classic "scratch buffer that grows once and is reused" pattern,
// which any program that handles variable-size inputs will produce.
// After the first cycle the heap has been extended, so the second
// alloc should be satisfied from the existing free list rather than
// forcing another grow.
static int test_grow_free_regrow(void) {
  RUN("test_grow_free_regrow");

  size_t initial_free = __heap_free_total_size();

  void* p1 = malloc(7000);
  if (!p1) FAIL("first malloc(7000) returned NULL");

  uintptr_t mid_end = __heap_end();

  free(p1);

  void* p2 = malloc(7000);
  if (!p2) FAIL("second malloc(7000) returned NULL");

  if (__heap_end() != mid_end) {
    FAIL("heap grew on second alloc even though space was freed");
  }

  free(p2);

  size_t final_free = __heap_free_total_size();
  if (final_free < initial_free) {
    FAIL("free total shrunk after grow/free/regrow cycle");
  }
  OK("test_grow_free_regrow");
}


// Mix small and large allocations so the heap-growth path is taken
// with a non-empty free list and non-empty in-flight set. Freeing
// across that boundary in arbitrary order should leave the heap
// total unchanged.
static int test_mixed_sizes_with_extend(void) {
  RUN("test_mixed_sizes_with_extend");

  size_t initial_free = __heap_free_total_size();

  void* small_a = malloc(96);
  void* small_b = malloc(96);
  void* big     = malloc(6000);
  void* small_c = malloc(96);
  if (!small_a || !small_b || !big || !small_c) {
    FAIL("malloc returned NULL during mixed-size sequence");
  }

  free(big);
  free(small_b);
  free(small_a);
  free(small_c);

  size_t final_free = __heap_free_total_size();
  if (final_free < initial_free) {
    FAIL("free total shrunk after mixed-size sequence");
  }
  OK("test_mixed_sizes_with_extend");
}


// Allocate many blocks of identical size and free them in strict
// reverse order. Every free except the last should be able to
// coalesce forward into the previously-freed neighbour, so the
// free list should collapse back to a single node.
static int test_reverse_free_full_coalesce(void) {
  RUN("test_reverse_free_full_coalesce");

  size_t initial_free  = __heap_free_total_size();
  size_t initial_nodes = __heap_free_list_length();

  enum { CAP = 100 };
  void* ptrs[CAP];
  int n = 0;
  while (n < CAP) {
    void* p = malloc(48);
    if (!p) FAIL("malloc(48) returned NULL while building chain");
    ptrs[n++] = p;
  }

  for (int i = n - 1; i >= 0; --i) free(ptrs[i]);

  size_t final_free  = __heap_free_total_size();
  size_t final_nodes = __heap_free_list_length();

  if (final_free < initial_free) {
    FAIL("reverse free leaked total bytes");
  }
  if (final_nodes != initial_nodes) {
    FAIL("reverse free did not coalesce back to initial node count");
  }
  OK("test_reverse_free_full_coalesce");
}


int main(void) {
  int rc = 0;
  rc |= test_single_alloc_forces_extend();
  rc |= test_large_alloc_payload_integrity();
  rc |= test_grow_free_regrow();
  rc |= test_mixed_sizes_with_extend();
  rc |= test_exhaust_then_keep_allocating();
  rc |= test_reverse_free_full_coalesce();
  return rc;
}
