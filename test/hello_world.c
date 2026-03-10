#include "../intf/syscalls.h"

int main() {
  write(1, "Hello, World!\n", sizeof("Hello, World!"));
  return 0;
}