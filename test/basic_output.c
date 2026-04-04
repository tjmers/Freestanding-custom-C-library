#include "../intf/stdio.h"

#include "../intf/syscalls.h"

int main() {
  puts("Hello, World!\n");

  terminate(0);
  return 0;
}