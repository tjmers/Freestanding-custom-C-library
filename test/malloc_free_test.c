#include "../intf/stdlib.h"
#include "../intf/syscalls.h"
#include "../intf/stdint.h"

// Forward declare (no need to put in header)
size_t __heap_free_list_length(void);
size_t __heap_free_total_size(void);

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

// Basic sanity test: allocate and free a single block and
// ensure the free list's aggregate size is restored.
static int test_malloc_free_round_trip(void) {
  size_t initial_nodes = __heap_free_list_length();
  size_t initial_total = __heap_free_total_size();

  void* p = malloc(64);
  if (!p) {
    const char err[] = "malloc returned NULL";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }

  size_t after_malloc_nodes = __heap_free_list_length();
  size_t after_malloc_total = __heap_free_total_size();

  if (after_malloc_total >= initial_total) {
    const char err[] = "free space did not decrease after malloc";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }

  free(p);

  size_t after_free_nodes = __heap_free_list_length();
  size_t after_free_total = __heap_free_total_size();

  if (after_free_total != initial_total) {
    const char err[] = "total free space not restored after free";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }

  if (after_free_nodes != initial_nodes) {
    const char err[] = "free list node count not restored after free";
    report_fail(err, sizeof(err - 1));
    return 1;
  }
  const char str[] = "malloc/free round trip";
  report_ok(str, sizeof(str) - 1);
  return 0;
}

// Allocate multiple blocks and free them in a different order.
// After all frees, the allocator should have the same total free
// space as at the start (no leak) even if fragmentation differs.
static int test_malloc_free_multiple(void) {
  size_t initial_total = __heap_free_total_size();

  void* a = malloc(32);
  void* b = malloc(48);
  void* c = malloc(96);

  if (!a || !b || !c) {
    const char err[] = "malloc returned NULL in multi-allocation test";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }

  free(b);
  free(a);
  free(c);

  size_t final_total = __heap_free_total_size();

  if (final_total != initial_total) {
    const char err[] = "total free space not restored after multi free";
    report_fail(err, sizeof(err) - 1);
    return 1;
  }

  const char str[] = "multiple malloc/free round trip";
  report_ok(str, sizeof(str - 1));
  return 0;
}

int main(void) {
  int rc = 0;

  rc |= test_malloc_free_round_trip();
  rc |= test_malloc_free_multiple();

  return rc;
}

