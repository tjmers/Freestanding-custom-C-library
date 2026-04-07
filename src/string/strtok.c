#include "../../intf/string.h"

#include "../../intf/stddef.h"

static char* prev = NULL;
static char missing_char = 0;
static bool at_end = true;

char* strtokc(char* str, char ch) {
  if (at_end && !str) return NULL;
  char* begin = str ? str : prev;
  *begin = str ? *begin : missing_char;
  char* end = begin + 1;
  while (*end != ch && *end != '\0') {
    ++end;
  }
  if (*end == '\0') {
    at_end = true;
    return begin;
  }
  at_end = false;

  missing_char = *end;
  *end = '\0';
  prev = end;

  return begin;
}

bool strtok_end() {
  return at_end;
}