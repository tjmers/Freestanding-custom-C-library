#include <memory.h>
#include <syscalls.h>
#include <stdlib.h>


int main() {
  // Create an array of size 1024 and memcpy onto it many many times

  char normal[1024];

  struct tms begin;
  struct tms end;
  times(&begin);



  for (int i = 0; i < 10000000; ++i) {
    char compare_against[1024];
    memcpy_small(compare_against, normal, 1024);
  }

  times(&end);

  int64_t time_taken = end.tms_utime - begin.tms_utime;

  char buf[64];
  int n = itoa(&time_taken, buf, 64);
  buf[n++] = '\n';

  write(1, buf, n);
}