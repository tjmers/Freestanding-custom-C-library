#ifndef __STDIO_H
#define __STDIO_H

#include "stdbool.h"
#include "stdint.h"

#define __STDIO_BUFFER_SIZE 1024

typedef struct _File {
  int fd_;
  uint32_t buffer_index_;
  char buffer_[__STDIO_BUFFER_SIZE];
} FILE, *OutputStream;

extern OutputStream stdout;
extern OutputStream stderr;

#define puts(str) (fputs(stdout, str))
#define putc(str) (fputc(stdout, str))
#define printf(format, ...) fprintf(stdout, format, ##__VA_ARGS__)

bool fflush(OutputStream out);
bool fprintf(OutputStream out, const char *__restrict__format, ...);
bool fputs(OutputStream out, const char* str);
bool fputc(OutputStream out, char ch);
bool fputi(OutputStream out, int i);

void __stdio_register_streams();


#endif