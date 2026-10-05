#include <syscalls.h>
#include <stdlib.h>


void reset_buf(char* buf) {
  for (int i = 0; i < 64; ++i) {
    buf[i] = 0;
  }
}

int main() {

  struct tms start_tms;

  clock_t start = times(&start_tms);
  struct timespec wait = {5ll, 0ll};
  nanosleep(&wait, NULL);


  struct tms end_tms;
  clock_t end = times(&end_tms);

  char buf[64];
  reset_buf(buf);

  int64_t user = end_tms.tms_utime - start_tms.tms_utime;
  int64_t system = end_tms.tms_stime - start_tms.tms_stime;
  write(1, "User: ", 6);
  int n = itoa(&user, buf, 64);
  buf[n++] = '\n';
  write(1, buf, n);
  reset_buf(buf);
  write(1, "System: ", 8);
  n = itoa(&system, buf, 64);
  buf[n++] = '\n';
  write(1, buf, n);
  reset_buf(buf);
  int64_t clock_ticks = end - start;
  write(1, "Clock ticks: ", 13);
  n = itoa(&clock_ticks, buf, 64);
  buf[n++] = '\n';
  write(1, buf, n);
}