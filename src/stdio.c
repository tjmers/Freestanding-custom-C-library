#include "stdio.h"

#include "stdint.h"
#include "stdarg.h"
#include "syscalls.h"

static char buffer[1024];
static uint32_t index;

int fprintf(int file, const char*__restrict format, ...) {

}

int printf(const char *__restrict__ format, ...) {
  va_list va;
  va_start(va, format);

}