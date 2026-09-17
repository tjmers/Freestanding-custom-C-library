#include "memory.h"
#include "syscalls.h"


void enable_fsrm() {
  memcpy_small = memcpy_small_fsrm;
  write(1, "FSRM detected\n", 14);
}