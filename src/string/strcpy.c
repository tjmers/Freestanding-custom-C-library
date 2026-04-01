#include "string.h"

size_t strcpy(char *__restrict__ dst, const char *__restrict__ src) {

  size_t bytes_copied = 0;

  while (*src) {
    *dst = *src;
    ++dst;
    ++src;
    ++bytes_copied;
  }
  return bytes_copied;
}