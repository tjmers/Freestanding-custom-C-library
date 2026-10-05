// printf test driver.
//
// Each case in printf_cases.inc is run in its own process by test/printf/printf_test.sh,
// which compares the bytes written to stdout against the expected output. Cases
// run in separate processes because a broken formatter may hang or crash, and
// the library has no way to capture stdout in-process.
//
// This file builds against mylibc (by default) and against the host glibc
// (with -DPRINTF_TEST_HOST_LIBC, done by printf_test.sh). The glibc build serves
// as a reference that validates the expected strings themselves.
//
// Usage:
//   printf --list           print every case name, one per line
//   printf --expect NAME    write the expected output of NAME
//   printf NAME             run NAME (exits 3 if NAME is unknown)

#ifndef PRINTF_TEST_HOST_LIBC
#include "../../intf/stdio.h"
#include "../../intf/string.h"
#include "../../intf/syscalls.h"
#define T_PUTS(s) fputs(stdout, s)
#define T_PUTC(c) fputc(stdout, c)
#else
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define T_PUTS(s) fputs(s, stdout)
#define T_PUTC(c) fputc(c, stdout)
#endif

// Limits are spelled out so that both builds use identical values.
#define T_INT_MAX 2147483647
#define T_INT_MIN (-2147483647 - 1)
#define T_UINT_MAX 4294967295U
#define T_LONG_MAX 9223372036854775807L
#define T_LONG_MIN (-9223372036854775807L - 1)
#define T_ULONG_MAX 18446744073709551615UL
#define T_LLONG_MAX 9223372036854775807LL
#define T_LLONG_MIN (-9223372036854775807LL - 1)
#define T_ULLONG_MAX 18446744073709551615ULL
#define T_DBL_MAX 1.7976931348623157e308
#define T_DBL_MIN 2.2250738585072014e-308
#define T_DBL_TRUE_MIN 4.9406564584124654e-324
#define T_INF __builtin_inf()
#define T_NAN __builtin_nan("")

typedef __SIZE_TYPE__ t_size;
typedef __PTRDIFF_TYPE__ t_ptrdiff;
typedef __INTMAX_TYPE__ t_intmax;
typedef __UINTMAX_TYPE__ t_uintmax;
typedef __WINT_TYPE__ t_wint;

typedef struct {
  const char* name;
  const char* expected;
  t_size expected_len;
  void (*run)(void);
  void (*expect)(void); // Writes the expected output when it is too large for a literal
  int no_flush;         // Rely on the exit-time flush instead of calling fflush
} printf_case;

// Data referenced by cases in printf_cases.inc.
static const char t_no_nul[3] = { 'a', 'b', 'c' };
static const char t_arr[8] = { 0 };

// Output helpers for the harness itself. These use write() directly so the
// harness never depends on the formatter under test.
static void out(const char* s, t_size n) {
  write(1, s, n);
}

static void out_str(const char* s) {
  out(s, strlen(s));
}

static void out_repeat(char c, t_size n) {
  for (t_size i = 0; i < n; ++i) {
    write(1, &c, 1);
  }
}

static void out_uint(unsigned v) {
  char buf[16];
  int i = sizeof(buf);
  do {
    buf[--i] = (char)('0' + v % 10);
    v /= 10;
  } while (v);
  out(&buf[i], sizeof(buf) - i);
}

// Fills buf with n bytes of a repeating a-z pattern and NUL-terminates it, so
// reordered or dropped chunks show up in the output.
static void fill_pattern(char* buf, t_size n) {
  for (t_size i = 0; i < n; ++i) {
    buf[i] = (char)('a' + i % 26);
  }
  buf[n] = '\0';
}

#define CASE(name, expected, ...) \
  static void run_##name(void) { printf(__VA_ARGS__); }
#define SPECIAL(name, expected)
#define SPECIAL_GEN(name)
#define SPECIAL_NOFLUSH(name, expected)
#include "printf_cases.inc"
#undef CASE
#undef SPECIAL
#undef SPECIAL_GEN
#undef SPECIAL_NOFLUSH

// --- %n: print the text, then print the stored count in a second call ---

static void run_n_basic(void) {
  int n = -1;
  printf("abc%n", &n);
  printf("|%d", n);
}

static void run_n_first(void) {
  int n = -1;
  printf("%nabc", &n);
  printf("|%d", n);
}

static void run_n_only(void) {
  int n = -1;
  printf("%n", &n);
  printf("|%d", n);
}

static void run_n_hh(void) {
  signed char n = -1;
  printf("hello%hhn", &n);
  printf("|%d", n);
}

static void run_n_h(void) {
  short n = -1;
  printf("hello%hn", &n);
  printf("|%d", n);
}

static void run_n_l(void) {
  long n = -1;
  printf("hello%ln", &n);
  printf("|%ld", n);
}

static void run_n_ll(void) {
  long long n = -1;
  printf("hello%lln", &n);
  printf("|%lld", n);
}

static void run_n_z(void) {
  t_ptrdiff n = -1; // The signed type corresponding to size_t
  printf("hello%zn", &n);
  printf("|%ld", (long)n);
}

static void run_n_j(void) {
  t_intmax n = -1;
  printf("hello%jn", &n);
  printf("|%ld", (long)n);
}

static void run_n_t(void) {
  t_ptrdiff n = -1;
  printf("hello%tn", &n);
  printf("|%ld", (long)n);
}

static void run_n_after_padding(void) {
  int n = -1;
  printf("%5d%n", 42, &n);
  printf("|%d", n);
}

static void run_n_after_percent(void) {
  int n = -1;
  printf("%%%n", &n);
  printf("|%d", n);
}

static void run_n_multiple(void) {
  int a = -1, b = -1;
  printf("ab%ncd%n", &a, &b);
  printf("|%d,%d", a, b);
}

static void run_n_after_string(void) {
  int n = -1;
  printf("%s%n!", "hello", &n);
  printf("|%d", n);
}

static void run_n_no_side_effect_on_neighbour(void) {
  int guard[3] = { 111, -1, 333 };
  printf("xy%n", &guard[1]);
  printf("|%d,%d,%d", guard[0], guard[1], guard[2]);
}

// --- Buffering and stream behaviour (stdout uses a 1 KB buffer) ---

static char big[3001];

static void run_buf_long_literal(void) {
  // A 3000-byte format with a conversion at the very end.
  fill_pattern(big, 2998);
  big[2998] = '%';
  big[2999] = 'd';
  big[3000] = '\0';
  printf(big, 7);
}

static void expect_buf_long_literal(void) {
  fill_pattern(big, 2998);
  out(big, 2998);
  out_str("7");
}

static void run_buf_long_arg(void) {
  fill_pattern(big, 3000);
  printf("[%s]", big);
}

static void expect_buf_long_arg(void) {
  fill_pattern(big, 3000);
  out_str("[");
  out(big, 3000);
  out_str("]");
}

static void run_buf_long_arg_prec(void) {
  fill_pattern(big, 3000);
  printf("[%.2500s]", big);
}

static void expect_buf_long_arg_prec(void) {
  fill_pattern(big, 3000);
  out_str("[");
  out(big, 2500);
  out_str("]");
}

static void run_buf_many_calls(void) {
  for (int i = 0; i < 2000; ++i) {
    printf("%d\n", i);
  }
}

static void expect_buf_many_calls(void) {
  for (unsigned i = 0; i < 2000; ++i) {
    out_uint(i);
    out_str("\n");
  }
}

static void run_buf_mixed_puts(void) {
  printf("a%d", 1);
  T_PUTS("b");
  printf("%c", 'c');
  T_PUTC('d');
  printf("e\n");
}

static void run_buf_flush_between(void) {
  printf("first %d,", 1);
  fflush(stdout);
  printf("second %d", 2);
}

static void run_buf_boundary_pad(void) {
  // Fill the buffer to one byte short of 1 KB, then pad across the boundary.
  fill_pattern(big, 1023);
  printf("%s", big);
  printf("%5d|", 42);
}

static void expect_buf_boundary_pad(void) {
  fill_pattern(big, 1023);
  out(big, 1023);
  out_str("   42|");
}

static void run_buf_exact_1024(void) {
  fill_pattern(big, 1024);
  printf("%s", big);
  printf("tail");
}

static void expect_buf_exact_1024(void) {
  fill_pattern(big, 1024);
  out(big, 1024);
  out_str("tail");
}

static void run_buf_huge_width(void) {
  printf("%2000d|", 7);
}

static void expect_buf_huge_width(void) {
  out_repeat(' ', 1999);
  out_str("7|");
}

static void run_buf_huge_width_left(void) {
  printf("%-2000d|", 7);
}

static void expect_buf_huge_width_left(void) {
  out_str("7");
  out_repeat(' ', 1999);
  out_str("|");
}

static void run_buf_huge_precision(void) {
  printf("%.1500d", 7);
}

static void expect_buf_huge_precision(void) {
  out_repeat('0', 1499);
  out_str("7");
}

static void run_buf_huge_float_precision(void) {
  printf("%.1100f", 1.0);
}

static void expect_buf_huge_float_precision(void) {
  out_str("1.");
  out_repeat('0', 1100);
}

static void run_buf_huge_string_width(void) {
  printf("%1500s|", "x");
}

static void expect_buf_huge_string_width(void) {
  out_repeat(' ', 1499);
  out_str("x|");
}

static void run_buf_no_flush(void) {
  printf("no flush %d\n", 1);
}

static void run_buf_no_flush_partial_line(void) {
  printf("partial %s", "line");
}

#define CASE(name, expected, ...) \
  { #name, expected, sizeof(expected) - 1, run_##name, 0, 0 },
#define SPECIAL(name, expected) \
  { #name, expected, sizeof(expected) - 1, run_##name, 0, 0 },
#define SPECIAL_GEN(name) \
  { #name, 0, 0, run_##name, expect_##name, 0 },
#define SPECIAL_NOFLUSH(name, expected) \
  { #name, expected, sizeof(expected) - 1, run_##name, 0, 1 },
static const printf_case cases[] = {
#include "printf_cases.inc"
};
#undef CASE
#undef SPECIAL
#undef SPECIAL_GEN
#undef SPECIAL_NOFLUSH

#define CASE_COUNT (sizeof(cases) / sizeof(cases[0]))

static const printf_case* find_case(const char* name) {
  for (t_size i = 0; i < CASE_COUNT; ++i) {
    if (strcmp(cases[i].name, name) == 0) {
      return &cases[i];
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  if (argc == 2 && strcmp(argv[1], "--list") == 0) {
    for (t_size i = 0; i < CASE_COUNT; ++i) {
      out_str(cases[i].name);
      out_str("\n");
    }
    return 0;
  }

  if (argc == 3 && strcmp(argv[1], "--expect") == 0) {
    const printf_case* c = find_case(argv[2]);
    if (!c) {
      return 3;
    }
    if (c->expect) {
      c->expect();
    } else {
      out(c->expected, c->expected_len);
    }
    return 0;
  }

  if (argc == 2) {
    const printf_case* c = find_case(argv[1]);
    if (!c) {
      return 3;
    }
    c->run();
    if (!c->no_flush) {
      fflush(stdout);
    }
    return 0;
  }

  static const char usage[] = "usage: printf --list | --expect NAME | NAME\n";
  write(2, usage, sizeof(usage) - 1);
  return 2;
}
