// These are currently very basic implementations. If more optimized versions are needed, this will likely be rewritten in x86 assembly 
#include "string.h"

size_t strlen(const char* str) {
  size_t len = 0;
  while (*str) {
    ++len;
    ++str;
  }
  return len;
}