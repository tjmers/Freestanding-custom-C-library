// This is simple for testing and learning and will just be run in the console and the output observed for correctness.
#include "../intf/assert.h"
#include "../intf/stdio.h"
#include "../intf/stdlib.h"
#include "../intf/stdarg.h"

void printints(int n, ...);


int main() {
  printints(5, 1, 2, 3, 4, 5);
  return 0;
}


void printints(int n, ...) {
  va_list args;
  va_start(args, n);
  // Buffer for printing to console
  char buffer[16];
  for (int i = 0; i < n; ++i) {
    int next = va_arg(args, int);
    // Print to the console
    int64_t tmp = (int64_t)next;
    int written = (int)itoa(&tmp, buffer, 16);
    buffer[written] = '\0';
    puts(buffer);
    putc(',');
  }
  putc('\n');
}
