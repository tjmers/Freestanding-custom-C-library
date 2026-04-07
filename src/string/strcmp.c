#include "../intf/string.h"

ptrdiff_t strcmp(const char *__restrict__ s1, const char *__restrict__ s2) {
  while (*s1 && *s2) {
    if (*s1 != *s2) {
      return *s1 - *s2;
    }

    ++s1;
    ++s2;
  }
  return *s1 - *s2;
}