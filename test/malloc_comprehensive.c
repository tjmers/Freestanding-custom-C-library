/// @file malloc_comprehensive.c
/// Comprehensive malloc/free test suite.
///
/// Every test is followed by a full structural check of the heap (see
/// heap_check) and a check that the heap has coalesced back into a single
/// free block, so a test that passes its own assertions but leaks or
/// corrupts metadata is still reported.
///
/// All loops are bounded and heap growth is capped (HEAP_CAP) so that an
/// allocator bug fails the test instead of exhausting system memory.
/// Run under a memory limit anyway, e.g.:
///   (ulimit -v 262144; timeout 10 ./build/test/malloc_comprehensive)

#include "../intf/bytes.h"
#include "../intf/stddef.h"
#include "../intf/stdlib.h"
#include "../intf/string.h"
#include "../intf/syscalls.h"
#include "../intf/stdint.h"

size_t __heap_free_list_length(void);
size_t __heap_free_total_size(void);
uintptr_t __heap_start(void);
uintptr_t __heap_end(void);

// ---------------------------------------------------------------------------
// Output helpers
// ---------------------------------------------------------------------------

static void write_str(const char* s, uint64_t n) { write(1, s, n); }

static void write_cstr(const char* s) { write_str(s, strlen(s)); }

static void write_num(uint64_t v) {
  char buf[32];
  uint32_t len = utoa(&v, buf, sizeof(buf));
  write_str(buf, len);
}

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

static void report_fail_num(const char* msg, uint64_t n, uint64_t v) {
  write_str("FAIL: ", 6);
  write_str(msg, n);
  write_str(" (", 2);
  write_num(v);
  write_str(")\n", 2);
}

static void report_ok(const char* msg, uint64_t n) {
  write_str("OK:   ", 6);
  write_str(msg, n);
  write_str("\n", 1);
}

#define RUN(s)       do { const char __m[] = s; announce(__m,    sizeof(__m) - 1); } while (0)
#define FAIL(s)      do { const char __m[] = s; report_fail(__m, sizeof(__m) - 1); return 1; } while (0)
#define FAIL_N(s, v) do { const char __m[] = s; report_fail_num(__m, sizeof(__m) - 1, (uint64_t)(v)); return 1; } while (0)
#define OK(s)        do { const char __m[] = s; report_ok(__m,   sizeof(__m) - 1); return 0; } while (0)
#define CHECK(c, s)  do { if (!(c)) FAIL(s); } while (0)

// Runs heap_check and fails the current test with its reason
#define CHECK_HEAP()                                         \
  do {                                                       \
    const char* __e = heap_check();                          \
    if (__e) {                                               \
      write_str("FAIL: heap check: ", 18);                   \
      write_cstr(__e);                                       \
      write_str("\n", 1);                                    \
      return 1;                                              \
    }                                                        \
  } while (0)

// ---------------------------------------------------------------------------
// Heap layout mirror
//
// These mirror the block layout in src/stdlib.c and must be kept in sync:
//   [dummy footer][header|payload|footer]...[header|payload|footer][dummy header]
// A used block has USED_NORMAL in both header and footer. A free block's
// header holds the next free block and its footer holds the previous one.
// ---------------------------------------------------------------------------

#define USED_NORMAL         (1ull << 63)
#define META                16u                // sizeof header == sizeof footer
#define OVERHEAD            (2u * META)        // per-block header + footer
#define HEAP_FIXED_OVERHEAD (4u * META)        // sentinels + one block's metadata
#define MMAP_THRESHOLD      ((size_t)1 << 17)  // requests >= this are mmap'd
#define HEAP_CAP            (64 * MB)          // fail instead of growing past this

struct blk {
  uintptr_t info;
  size_t size;
};

static struct blk* footer_at(uintptr_t header) {
  return (struct blk*)(header + META + ((struct blk*)header)->size);
}

enum { MAX_FREE_TRACK = 4096 };
static uintptr_t free_blocks_[MAX_FREE_TRACK];

static bool is_tracked_free(uintptr_t addr, size_t nfree) {
  for (size_t i = 0; i < nfree; ++i) {
    if (free_blocks_[i] == addr) return true;
  }
  return false;
}

/// Walks every block between the sentinels and validates the heap.
/// Returns NULL if the heap is consistent, otherwise a description of the
/// first problem found.
static const char* heap_check(void) {
  uintptr_t start = __heap_start();
  uintptr_t end = __heap_end();
  if (start == 0 || end <= start) return "heap bounds invalid";
  if ((start & 15) || (end & 0xfff)) return "heap bounds misaligned";
  if (end - start > HEAP_CAP) return "heap grew past the test cap";
  if (((struct blk*)start)->info != USED_NORMAL) return "bottom sentinel footer clobbered";
  if (((struct blk*)end - 1)->info != USED_NORMAL) return "top sentinel header clobbered";

  // Physical walk
  size_t nfree = 0;
  size_t free_total = 0;
  bool prev_free = false;
  uintptr_t pos = start + META;
  uintptr_t last = end - META;
  while (pos < last) {
    struct blk* h = (struct blk*)pos;
    if (last - pos < OVERHEAD) return "truncated block at heap top";
    if (h->size & 15) return "block size is not a multiple of 16";
    if (h->size > last - pos - OVERHEAD) return "block extends past the top sentinel";
    struct blk* f = footer_at(pos);
    if (f->size != h->size) return "header/footer size mismatch";
    bool used = h->info == USED_NORMAL;
    if (used != (f->info == USED_NORMAL)) return "header/footer used-state mismatch";
    if (!used) {
      if (prev_free) return "two adjacent free blocks (missed coalesce)";
      if (nfree == MAX_FREE_TRACK) return "too many free blocks to track";
      free_blocks_[nfree++] = pos;
      free_total += h->size;
    }
    prev_free = !used;
    pos = (uintptr_t)(f + 1);
  }
  if (pos != last) return "block walk did not end at the top sentinel";

  // Free list links (checked before walking the list so a broken list
  // cannot hang __heap_free_list_length)
  size_t heads = 0;
  for (size_t i = 0; i < nfree; ++i) {
    uintptr_t self = free_blocks_[i];
    uintptr_t next = ((struct blk*)self)->info;
    uintptr_t prev = footer_at(self)->info;
    if (prev == 0) {
      ++heads;
    } else {
      if (!is_tracked_free(prev, nfree)) return "free block prev link is not a free block";
      if (((struct blk*)prev)->info != self) return "prev->next does not point back";
    }
    if (next != 0) {
      if (!is_tracked_free(next, nfree)) return "free block next link is not a free block";
      if (footer_at(next)->info != self) return "next->prev does not point back";
    }
  }
  if (nfree > 0 && heads != 1) return "free list does not have exactly one head";
  if (nfree != __heap_free_list_length()) return "free list length disagrees with heap walk";
  if (free_total != __heap_free_total_size()) return "free list total disagrees with heap walk";
  return NULL;
}

/// Checks that the whole heap is one free block (nothing leaked).
static const char* heap_all_free(void) {
  if (__heap_free_list_length() != 1) return "heap is not a single free block";
  if (__heap_free_total_size() != __heap_end() - __heap_start() - HEAP_FIXED_OVERHEAD) {
    return "single free block does not span the whole heap";
  }
  return NULL;
}

// ---------------------------------------------------------------------------
// Payload and PRNG helpers
// ---------------------------------------------------------------------------

static unsigned char pattern(uintptr_t seed, size_t i) {
  return (unsigned char)((i * 131u) ^ (seed * 0x9Du) ^ (i >> 8) ^ 0x5A);
}

static void fill(void* p, size_t n, uintptr_t seed) {
  unsigned char* b = p;
  for (size_t i = 0; i < n; ++i) b[i] = pattern(seed, i);
}

static bool verify(const void* p, size_t n, uintptr_t seed) {
  const unsigned char* b = p;
  for (size_t i = 0; i < n; ++i) {
    if (b[i] != pattern(seed, i)) return false;
  }
  return true;
}

static bool aligned16(const void* p) { return ((uintptr_t)p & 15) == 0; }

static bool in_heap(const void* p) {
  uintptr_t a = (uintptr_t)p;
  return a >= __heap_start() && a < __heap_end();
}

static uint64_t rng_state_;

static void rng_seed(uint64_t s) { rng_state_ = s ? s : 0x9E3779B97F4A7C15ull; }

static uint64_t rng(void) {
  uint64_t x = rng_state_;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  rng_state_ = x;
  return x;
}

static size_t rng_range(size_t lo, size_t hi) { return lo + (size_t)(rng() % (hi - lo + 1)); }

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// The heap starts as a single free block spanning the initial arena.
static int test_initial_state(void) {
  RUN("test_initial_state");
  CHECK_HEAP();
  const char* e = heap_all_free();
  CHECK(e == NULL, "fresh heap is not a single free block");
  OK("test_initial_state");
}


// free(NULL) is a no-op.
static int test_free_null(void) {
  RUN("test_free_null");
  size_t len = __heap_free_list_length();
  size_t total = __heap_free_total_size();
  free(NULL);
  CHECK(__heap_free_list_length() == len, "free(NULL) changed the free list length");
  CHECK(__heap_free_total_size() == total, "free(NULL) changed the free total");
  OK("test_free_null");
}


// malloc(0) may return NULL or a unique pointer, but either must be freeable.
static int test_malloc_zero(void) {
  RUN("test_malloc_zero");
  void* p = malloc(0);
  void* q = malloc(0);
  if (p) CHECK(aligned16(p), "malloc(0) returned a misaligned pointer");
  if (p && q) CHECK(p != q, "two live malloc(0) calls returned the same pointer");
  CHECK_HEAP();
  free(p);
  free(q);
  OK("test_malloc_zero");
}


// Every size from 1 to 1024 returns a 16-byte aligned pointer whose first
// and last bytes are writable.
static int test_alignment_every_small_size(void) {
  RUN("test_alignment_every_small_size");
  for (size_t n = 1; n <= 1024; ++n) {
    unsigned char* p = malloc(n);
    if (!p) FAIL_N("malloc returned NULL for size", n);
    if (!aligned16(p)) FAIL_N("misaligned pointer for size", n);
    p[0] = 0xAB;
    p[n - 1] = 0xCD;
    free(p);
  }
  CHECK_HEAP();
  OK("test_alignment_every_small_size");
}


// Hold one live allocation of every size 1..300 at once. Each payload gets
// a distinct pattern, and all are verified only after every allocation is
// made, so any overlap or metadata write into a payload is caught. The
// address ranges are also checked to be pairwise disjoint.
static int test_size_sweep_live(void) {
  RUN("test_size_sweep_live");
  enum { N = 300 };
  unsigned char* ptrs[N];

  for (size_t i = 0; i < N; ++i) {
    size_t n = i + 1;
    ptrs[i] = malloc(n);
    if (!ptrs[i]) FAIL_N("malloc returned NULL for size", n);
    if (!aligned16(ptrs[i])) FAIL_N("misaligned pointer for size", n);
    if (!in_heap(ptrs[i])) FAIL_N("pointer outside the heap for size", n);
    fill(ptrs[i], n, i);
  }

  for (size_t i = 0; i < N; ++i) {
    if (!verify(ptrs[i], i + 1, i)) FAIL_N("payload corrupted for size", i + 1);
  }

  for (size_t i = 0; i < N; ++i) {
    uintptr_t a0 = (uintptr_t)ptrs[i], a1 = a0 + i + 1;
    for (size_t j = i + 1; j < N; ++j) {
      uintptr_t b0 = (uintptr_t)ptrs[j], b1 = b0 + j + 1;
      if (a0 < b1 && b0 < a1) FAIL_N("overlapping live allocations at index", i);
    }
  }
  CHECK_HEAP();

  // Free odds then evens so both forward and backward coalescing happen
  for (size_t i = 1; i < N; i += 2) free(ptrs[i]);
  CHECK_HEAP();
  for (size_t i = 0; i < N; i += 2) free(ptrs[i]);
  OK("test_size_sweep_live");
}


// Writing exactly n bytes into an n-byte allocation must not disturb its
// neighbours or the heap metadata. Covers sizes on and around the 16-byte
// rounding boundaries.
static int test_payload_boundaries(void) {
  RUN("test_payload_boundaries");
  static const size_t sizes[] = {1, 15, 16, 17, 31, 32, 33, 47, 48, 49,
                                 63, 64, 65, 100, 255, 256, 257, 4000, 4096, 4097};
  for (size_t k = 0; k < sizeof(sizes) / sizeof(sizes[0]); ++k) {
    size_t n = sizes[k];
    unsigned char* x = malloc(n);
    unsigned char* y = malloc(n);
    unsigned char* z = malloc(n);
    if (!x || !y || !z) FAIL_N("malloc returned NULL for size", n);
    fill(x, n, 1);
    fill(z, n, 3);
    fill(y, n, 2);
    if (!verify(x, n, 1)) FAIL_N("left neighbour clobbered for size", n);
    if (!verify(z, n, 3)) FAIL_N("right neighbour clobbered for size", n);
    CHECK_HEAP();
    free(y);
    CHECK_HEAP();
    if (!verify(x, n, 1) || !verify(z, n, 3)) FAIL_N("free clobbered a neighbour for size", n);
    free(x);
    free(z);
  }
  OK("test_payload_boundaries");
}


// A hole between two live blocks is reused for a same-size request and
// for a smaller one, without growing the heap.
static int test_reuse_freed_hole(void) {
  RUN("test_reuse_freed_hole");
  uintptr_t end = __heap_end();
  void* a = malloc(200);
  void* b = malloc(200);
  void* c = malloc(200);
  CHECK(a && b && c, "malloc returned NULL");

  free(b);
  void* d = malloc(200);
  CHECK(d == b, "same-size request did not reuse the freed hole");
  free(d);
  void* e = malloc(64);
  CHECK(e == b, "smaller request was not carved from the freed hole");
  CHECK_HEAP();
  CHECK(__heap_end() == end, "heap grew even though a hole was available");

  free(e);
  free(a);
  free(c);
  OK("test_reuse_freed_hole");
}


// Three adjacent blocks freed in all six orders must always coalesce back
// to the starting state.
static int test_coalesce_all_orders_of_three(void) {
  RUN("test_coalesce_all_orders_of_three");
  static const int orders[6][3] = {
    {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0},
  };
  size_t len = __heap_free_list_length();
  size_t total = __heap_free_total_size();
  uintptr_t end = __heap_end();

  for (int o = 0; o < 6; ++o) {
    void* blocks[3];
    blocks[0] = malloc(64);
    blocks[1] = malloc(128);
    blocks[2] = malloc(256);
    if (!blocks[0] || !blocks[1] || !blocks[2]) FAIL_N("malloc returned NULL in order", o);
    for (int k = 0; k < 3; ++k) {
      free(blocks[orders[o][k]]);
      const char* e = heap_check();
      if (e) {
        write_str("FAIL: heap check: ", 18);
        write_cstr(e);
        write_str("\n", 1);
        FAIL_N("heap inconsistent mid-sequence in order", o);
      }
    }
    if (__heap_free_list_length() != len) FAIL_N("free list length not restored in order", o);
    if (__heap_free_total_size() != total) FAIL_N("free total not restored in order", o);
  }
  CHECK(__heap_end() == end, "heap grew during small coalesce test");
  OK("test_coalesce_all_orders_of_three");
}


// Two separated holes stay separate; freeing the block between them
// merges all three into one free block.
static int test_coalesce_bridging_block(void) {
  RUN("test_coalesce_bridging_block");
  CHECK(heap_all_free() == NULL, "precondition: heap not fully free");
  void* b[5];
  for (int i = 0; i < 5; ++i) {
    b[i] = malloc(96);
    if (!b[i]) FAIL_N("malloc returned NULL at index", i);
  }
  // Only the trailing remainder is free now
  CHECK(__heap_free_list_length() == 1, "expected only the trailing free block");

  free(b[1]);
  free(b[3]);
  CHECK(__heap_free_list_length() == 3, "non-adjacent holes should not merge");
  CHECK_HEAP();

  free(b[2]);
  CHECK(__heap_free_list_length() == 2, "freeing the bridge did not merge both holes");
  CHECK_HEAP();

  free(b[0]);
  CHECK(__heap_free_list_length() == 2, "block 0 did not merge into the hole after it");
  free(b[4]);
  CHECK(__heap_free_list_length() == 1, "block 4 did not merge on both sides");
  OK("test_coalesce_bridging_block");
}


// With a single free block of size S: malloc(S) consumes it exactly,
// sizes within a split threshold also take the whole block, and none of
// these grow the heap.
static int test_exact_and_near_fit(void) {
  RUN("test_exact_and_near_fit");
  CHECK(heap_all_free() == NULL, "precondition: heap not fully free");
  size_t s = __heap_free_total_size();
  if (s >= MMAP_THRESHOLD) FAIL("precondition: free block too large for this test");
  uintptr_t end = __heap_end();

  static const size_t shrink[] = {0, 16, 32};
  for (size_t k = 0; k < sizeof(shrink) / sizeof(shrink[0]); ++k) {
    size_t n = s - shrink[k];
    void* p = malloc(n);
    if (!p) FAIL_N("malloc returned NULL for size", n);
    if (__heap_end() != end) FAIL_N("heap grew for a request that fits, size", n);
    if (__heap_free_list_length() != 0) FAIL_N("near-exact fit left a free fragment, size", n);
    fill(p, n, k);
    CHECK_HEAP();
    if (!verify(p, n, k)) FAIL_N("payload corrupted for size", n);
    free(p);
    CHECK_HEAP();
  }

  // Large enough remainder: must split
  void* p = malloc(s - 64);
  CHECK(p != NULL, "malloc returned NULL for split case");
  CHECK(__heap_free_list_length() == 1, "remainder was not split off");
  CHECK(__heap_end() == end, "heap grew for a request that fits");
  CHECK_HEAP();
  free(p);
  OK("test_exact_and_near_fit");
}


// When the top block is free, a request larger than it must grow that
// block in place (its payload address is the start of the old free block)
// rather than leaving it stranded.
static int test_grow_in_place_when_top_free(void) {
  RUN("test_grow_in_place_when_top_free");
  CHECK(heap_all_free() == NULL, "precondition: heap not fully free");
  size_t s = __heap_free_total_size();
  uintptr_t end = __heap_end();
  void* expected = (void*)(__heap_start() + META + META);

  size_t n = s + 4096;
  if (n >= MMAP_THRESHOLD) FAIL("precondition: heap too large for this test");
  unsigned char* p = malloc(n);
  CHECK(p != NULL, "malloc returned NULL");
  CHECK(__heap_end() > end, "heap did not grow");
  CHECK(p == expected, "top free block was not grown in place");
  fill(p, n, 7);
  CHECK_HEAP();
  CHECK(verify(p, n, 7), "payload corrupted");
  free(p);
  OK("test_grow_in_place_when_top_free");
}


// When the top block is in use, growth creates a new block where the old
// top sentinel was. Freeing across that boundary must coalesce.
static int test_extend_with_used_top_block(void) {
  RUN("test_extend_with_used_top_block");
  CHECK(heap_all_free() == NULL, "precondition: heap not fully free");
  size_t s = __heap_free_total_size();
  uintptr_t end = __heap_end();

  unsigned char* a = malloc(s);
  CHECK(a != NULL, "malloc returned NULL for the whole block");
  CHECK(__heap_free_list_length() == 0, "precondition: heap should be full");
  fill(a, s, 11);

  unsigned char* b = malloc(64);
  CHECK(b != NULL, "malloc returned NULL after the heap was full");
  CHECK(__heap_end() > end, "heap did not grow");
  CHECK((uintptr_t)b == end, "new block does not start at the old top sentinel");
  fill(b, 64, 12);
  CHECK_HEAP();
  CHECK(verify(a, s, 11), "growth clobbered the block below it");

  free(a);
  CHECK(__heap_free_list_length() == 2, "expected a hole below b and a remainder above");
  CHECK_HEAP();
  CHECK(verify(b, 64, 12), "free clobbered a neighbour");
  free(b);
  OK("test_extend_with_used_top_block");
}


// Requests at or above the mmap threshold bypass the heap entirely;
// requests just under it come from the heap.
static int test_mmap_path(void) {
  RUN("test_mmap_path");
  static const size_t sizes[] = {
    MMAP_THRESHOLD, MMAP_THRESHOLD - 1, MMAP_THRESHOLD + 1, MB, 3 * MB + 5,
  };
  size_t len = __heap_free_list_length();
  size_t total = __heap_free_total_size();
  uintptr_t end = __heap_end();

  for (size_t k = 0; k < sizeof(sizes) / sizeof(sizes[0]); ++k) {
    size_t n = sizes[k];
    unsigned char* p = malloc(n);
    if (!p) FAIL_N("malloc returned NULL for size", n);
    if (!aligned16(p)) FAIL_N("misaligned mmap pointer for size", n);
    if (in_heap(p)) FAIL_N("large request was served from the brk heap, size", n);
    fill(p, n, k);
    if (!verify(p, n, k)) FAIL_N("mmap payload corrupted for size", n);
    if (__heap_end() != end) FAIL_N("mmap request moved the heap end, size", n);
    if (__heap_free_list_length() != len || __heap_free_total_size() != total) {
      FAIL_N("mmap request changed the heap free list, size", n);
    }
    free(p);
    if (__heap_free_list_length() != len || __heap_free_total_size() != total) {
      FAIL_N("freeing an mmap block changed the heap free list, size", n);
    }
  }

  // Largest size that rounds below the threshold stays in the heap
  size_t n = MMAP_THRESHOLD - 16;
  unsigned char* p = malloc(n);
  CHECK(p != NULL, "malloc returned NULL just under the mmap threshold");
  CHECK(in_heap(p), "request just under the threshold was not served from the heap");
  fill(p, n, 99);
  CHECK_HEAP();
  CHECK(verify(p, n, 99), "payload corrupted just under the threshold");
  free(p);
  OK("test_mmap_path");
}


// Thousands of 1-byte allocations: all distinct, aligned and independent,
// freed in a scattered order.
static int test_many_tiny(void) {
  RUN("test_many_tiny");
  enum { N = 2000 };
  unsigned char* ptrs[N];
  for (size_t i = 0; i < N; ++i) {
    ptrs[i] = malloc(1);
    if (!ptrs[i]) FAIL_N("malloc(1) returned NULL at index", i);
    if (!aligned16(ptrs[i])) FAIL_N("misaligned pointer at index", i);
    *ptrs[i] = pattern(i, 0);
  }
  for (size_t i = 0; i < N; ++i) {
    if (*ptrs[i] != pattern(i, 0)) FAIL_N("tiny payload corrupted at index", i);
    for (size_t j = i + 1; j < N; ++j) {
      if (ptrs[i] == ptrs[j]) FAIL_N("duplicate pointer at index", i);
    }
  }
  CHECK_HEAP();
  // 7 is coprime with N, so this visits every index exactly once
  for (size_t k = 0; k < N; ++k) free(ptrs[(k * 7) % N]);
  OK("test_many_tiny");
}


// Repeating an identical workload must not keep growing the heap: once
// everything is freed the heap returns to the same state.
static int test_steady_state_no_growth(void) {
  RUN("test_steady_state_no_growth");
  enum { N = 64, ROUNDS = 30 };
  void* ptrs[N];
  size_t sizes[N];
  size_t order[N];
  uintptr_t end_after_first = 0;

  for (int r = 0; r < ROUNDS; ++r) {
    rng_seed(12345);
    for (size_t i = 0; i < N; ++i) {
      sizes[i] = rng_range(1, 1500);
      ptrs[i] = malloc(sizes[i]);
      if (!ptrs[i]) FAIL_N("malloc returned NULL in round", r);
      fill(ptrs[i], sizes[i], i);
      order[i] = i;
    }
    // Free order varies by round
    rng_seed(1000 + (uint64_t)r);
    for (size_t i = N - 1; i > 0; --i) {
      size_t j = rng_range(0, i);
      size_t t = order[i]; order[i] = order[j]; order[j] = t;
    }
    for (size_t i = 0; i < N; ++i) {
      size_t k = order[i];
      if (!verify(ptrs[k], sizes[k], k)) FAIL_N("payload corrupted in round", r);
      free(ptrs[k]);
    }
    if (r == 0) {
      end_after_first = __heap_end();
    } else if (__heap_end() != end_after_first) {
      FAIL_N("heap kept growing under a repeated workload, round", r);
    }
    const char* e = heap_all_free();
    if (e) FAIL_N("heap not fully coalesced after round", r);
  }
  OK("test_steady_state_no_growth");
}


// Randomised malloc/free churn across small, medium and mmap-sized
// requests. Every payload is verified when it is freed and the heap is
// checked periodically.
static int test_random_stress(void) {
  RUN("test_random_stress");
  enum { SLOTS = 128, OPS = 20000, CHECK_EVERY = 256 };
  unsigned char* ptrs[SLOTS];
  size_t sizes[SLOTS];
  uintptr_t seeds[SLOTS];
  // static so it starts zeroed without a fill loop (gcc would turn that
  // into a memset call, which this libc does not provide)
  static bool live[SLOTS];

  rng_seed(0xC0FFEEull);
  for (size_t op = 0; op < OPS; ++op) {
    size_t s = rng_range(0, SLOTS - 1);
    if (live[s]) {
      if (!verify(ptrs[s], sizes[s], seeds[s])) FAIL_N("payload corrupted before free, op", op);
      free(ptrs[s]);
      live[s] = false;
    } else {
      size_t roll = rng_range(0, 999);
      size_t n;
      if (roll < 5) {
        n = rng_range(MMAP_THRESHOLD, 2 * MMAP_THRESHOLD);
      } else if (roll < 100) {
        n = rng_range(513, 8192);
      } else {
        n = rng_range(1, 512);
      }
      ptrs[s] = malloc(n);
      if (!ptrs[s]) FAIL_N("malloc returned NULL, op", op);
      if (!aligned16(ptrs[s])) FAIL_N("misaligned pointer, op", op);
      sizes[s] = n;
      seeds[s] = op;
      fill(ptrs[s], n, op);
      live[s] = true;
    }
    if (op % CHECK_EVERY == 0) {
      const char* e = heap_check();
      if (e) {
        write_str("FAIL: heap check: ", 18);
        write_cstr(e);
        write_str("\n", 1);
        FAIL_N("heap inconsistent during stress, op", op);
      }
    }
  }

  for (size_t s = 0; s < SLOTS; ++s) {
    if (!live[s]) continue;
    if (!verify(ptrs[s], sizes[s], seeds[s])) FAIL_N("payload corrupted at teardown, slot", s);
    free(ptrs[s]);
  }
  OK("test_random_stress");
}


// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

typedef int (*test_fn)(void);

// Runs a test and then verifies the heap is consistent and fully free.
static int run_test(test_fn fn) {
  int rc = fn();
  const char* e = heap_check();
  if (!e) e = heap_all_free();
  if (e) {
    write_str("FAIL: post-test heap check: ", 28);
    write_cstr(e);
    write_str("\n", 1);
    rc = 1;
  }
  return rc;
}

int main(void) {
  static const test_fn tests[] = {
    test_initial_state,
    test_free_null,
    test_malloc_zero,
    test_alignment_every_small_size,
    test_size_sweep_live,
    test_payload_boundaries,
    test_reuse_freed_hole,
    test_coalesce_all_orders_of_three,
    test_coalesce_bridging_block,
    test_exact_and_near_fit,
    test_grow_in_place_when_top_free,
    test_extend_with_used_top_block,
    test_mmap_path,
    test_many_tiny,
    test_steady_state_no_growth,
    test_random_stress,
  };
  size_t count = sizeof(tests) / sizeof(tests[0]);
  size_t failed = 0;
  for (size_t i = 0; i < count; ++i) {
    if (run_test(tests[i])) ++failed;
  }

  write_num(count - failed);
  write_str("/", 1);
  write_num(count);
  write_str(" tests passed\n", 14);
  return failed != 0;
}
