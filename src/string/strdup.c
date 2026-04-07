#include "../../intf/string.h"

#include "../../intf/stddef.h"
#include "../../intf/stdlib.h"


char* strdup(const char* str) {
  size_t len = strlen(str);

  char* copy = malloc(len);
  if (!copy) return copy;

  if (strcpy(copy, str) != len) {
    free(copy);
    return NULL;
  }
  return copy;
}