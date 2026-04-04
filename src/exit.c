#include "../intf/exit.h"

#include "../intf/stdlib.h"
#include "../intf/string.h"


static callback_t atexit[ATEXIT_MAX_CALLBACKS] = {0};
static int n_callbacks = 0;

bool atexit_push(void (*func)(void)) {
  if (n_callbacks == ATEXIT_MAX_CALLBACKS) {
    return false;
  }

  atexit[n_callbacks++] = func;

  return true;
}

callback_t atexit_pop() {
  return atexit[n_callbacks--];
}

void exit(int exit_code) {
  // Log info when under test
#ifdef __LIBC_TEST
  char buffer[64];
  int i = 0;
  const char first[] = "Calling ";
  strcpy(buffer, first);
  i += sizeof(first) - 1;
  i += itoa((int)n_callbacks, &buffer[i], 64 - i);
  const char last[] = " callbacks.\n";
  strcpy(&buffer[i], last);
  i += sizeof(last) - 1;

  write(1, buffer, i);
#endif // __LIBC_TEST

  // Call all atexit functions in LIFO
  for (int j = n_callbacks - 1; j >= 0; --j) {
    atexit[j]();
  }

  // Call terminate
  terminate(exit_code);
  __builtin_unreachable(); 
}