#include "stdlib.h"
#include "string.h"
#include "syscalls.h"

void __assert_fail(const char *expr, const char *file, unsigned int line,
                   const char *func) {
  static const char m1[] = "Assertion failed: ";
  static const char m2[] = ", file ";
  static const char m3[] = ", line ";
  static const char m4[] = ", function ";
  static const char m5[] = ".\n";
  char buf[32];
  uint32_t n;

  write(2, m1, sizeof(m1) - 1);
  write(2, expr, strlen(expr));
  write(2, m2, sizeof(m2) - 1);
  write(2, file, strlen(file));
  write(2, m3, sizeof(m3) - 1);
  int64_t tmp_line = (int64_t)line;
  n = itoa(&tmp_line, buf, sizeof buf);
  write(2, buf, n);
  write(2, m4, sizeof(m4) - 1);
  write(2, func, strlen(func));
  write(2, m5, sizeof(m5) - 1);
  abort();
}
