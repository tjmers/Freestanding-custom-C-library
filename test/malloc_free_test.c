#include "../intf/stdlib.h"
#include "../intf/syscalls.h"
#include "../intf/stdint.h"

// Forward declare (no need to put in header)
size_t __heap_free_list_length(void);
size_t __heap_free_total_size(void);
void __print_heap(void);

static void write_str(const char* s, uint64_t n) {
  write(1, s, n);
}

static void report_fail(const char* msg, uint64_t n) {
  write_str("FAIL: ", 6);
  write_str(msg, n);
  write_str("\n", 1);
}

static void report_ok(const char* msg, uint64_t n) {
  write_str("OK: ", 4);
  write_str(msg, n);
  write_str("\n", 1);
}

static void dbg(const char* s, uint64_t n) {
  write(1, "[DBG] ", 6);
  write(1, s, n);
  write(1, "\n", 1);
}

/* String literal only — sizeof includes '\0', so length is sizeof(s) - 1 */
#if 0
#define DBG(s) dbg((s), sizeof(s) - 1)
#else
#define DBG(s) ((void)s)
#endif

// Basic sanity test: allocate and free a single block and
// ensure the free list's aggregate size is restored.
static int test_malloc_free_round_trip(void) {
  DBG("test_malloc_free_round_trip: enter");

  DBG("test_malloc_free_round_trip: before __heap_free_list_length");
  size_t initial_nodes = __heap_free_list_length();
  DBG("test_malloc_free_round_trip: after __heap_free_list_length");

  DBG("test_malloc_free_round_trip: before __heap_free_total_size");
  size_t initial_total = __heap_free_total_size();
  DBG("test_malloc_free_round_trip: after __heap_free_total_size");

  DBG("test_malloc_free_round_trip: before malloc(64)");
  void* p = malloc(64);
  DBG("test_malloc_free_round_trip: after malloc(64)");
  if (!p) {
    const char err[] = "malloc returned NULL";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }

  DBG("test_malloc_free_round_trip: before __heap_free_list_length (post-malloc)");
  size_t after_malloc_nodes = __heap_free_list_length();
  DBG("test_malloc_free_round_trip: after __heap_free_list_length (post-malloc)");

  DBG("test_malloc_free_round_trip: before __heap_free_total_size (post-malloc)");
  size_t after_malloc_total = __heap_free_total_size();
  DBG("test_malloc_free_round_trip: after __heap_free_total_size (post-malloc)");

  if (after_malloc_total >= initial_total) {
    const char err[] = "free space did not decrease after malloc";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }

  DBG("test_malloc_free_round_trip: before free(p)");
  free(p);
  DBG("test_malloc_free_round_trip: after free(p)");

  DBG("test_malloc_free_round_trip: before __heap_free_list_length (post-free)");
  size_t after_free_nodes = __heap_free_list_length();
  DBG("test_malloc_free_round_trip: after __heap_free_list_length (post-free)");

  DBG("test_malloc_free_round_trip: before __heap_free_total_size (post-free)");
  size_t after_free_total = __heap_free_total_size();
  DBG("test_malloc_free_round_trip: after __heap_free_total_size (post-free)");

  if (after_free_total != initial_total) {
    const char err[] = "total free space not restored after free";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }

  if (after_free_nodes != initial_nodes) {
    const char err[] = "free list node count not restored after free";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }
  DBG("test_malloc_free_round_trip: before report_ok");
  const char str[] = "malloc/free round trip";
  report_ok(str, sizeof(str) - 1);
  DBG("test_malloc_free_round_trip: leave ok");
  return 0;
}

// Allocate multiple blocks and free them in a different order.
// After all frees, the allocator should have the same total free
// space as at the start (no leak) even if fragmentation differs.
static int test_malloc_free_multiple(void) {
  DBG("test_malloc_free_multiple: enter");

  DBG("test_malloc_free_multiple: before __heap_free_total_size");
  size_t initial_total = __heap_free_total_size();
  DBG("test_malloc_free_multiple: after __heap_free_total_size");

  DBG("test_malloc_free_multiple: before malloc(32)");
  void* a = malloc(32);
  DBG("test_malloc_free_multiple: after malloc(32)");

  DBG("test_malloc_free_multiple: before malloc(48)");
  void* b = malloc(48);
  DBG("test_malloc_free_multiple: after malloc(48)");

  DBG("test_malloc_free_multiple: before malloc(96)");
  void* c = malloc(96);
  DBG("test_malloc_free_multiple: after malloc(96)");

  if (!a || !b || !c) {
    const char err[] = "malloc returned NULL in multi-allocation test";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }

  DBG("test_malloc_free_multiple: before free(b)");
  free(b);
  DBG("test_malloc_free_multiple: after free(b)");

  DBG("test_malloc_free_multiple: before free(a)");
  free(a);
  DBG("test_malloc_free_multiple: after free(a)");

  DBG("test_malloc_free_multiple: before free(c)");
  free(c);
  DBG("test_malloc_free_multiple: after free(c)");

  DBG("test_malloc_free_multiple: before __heap_free_total_size (final)");
  size_t final_total = __heap_free_total_size();
  DBG("test_malloc_free_multiple: after __heap_free_total_size (final)");

  if (final_total != initial_total) {
    const char err[] = "total free space not restored after multi free";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }

  DBG("test_malloc_free_multiple: before report_ok");
  const char str[] = "multiple malloc/free round trip";
  report_ok(str, sizeof(str) - 1);
  DBG("test_malloc_free_multiple: leave ok");
  return 0;
}

int main(void) {
  DBG("main: enter");

  int rc = 0;

  DBG("main: before test_malloc_free_round_trip");
  rc |= test_malloc_free_round_trip();
  DBG("main: after test_malloc_free_round_trip");

  DBG("main: before test_malloc_free_multiple");
  rc |= test_malloc_free_multiple();
  DBG("main: after test_malloc_free_multiple");

  DBG("main: return");
  return rc;
}

