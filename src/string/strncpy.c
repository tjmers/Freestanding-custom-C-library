#include "string.h"

size_t strncpy(char *__restrict__ dst, const char *__restrict__ src, size_t n) {

  size_t bytes_copied = 0;

  while (*src && bytes_copied < n) {
    *dst = *src;
    ++dst;
    ++src;
    ++bytes_copied;
  }
  return bytes_copied;
}