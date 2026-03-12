#ifndef __STDIO_H
#define __STDIO_H

#define stdin 0
#define stdout 1
#define stderr 2


void fflush(int file);
int fprintf(int file, const char *__restrict__format, ...);
inline int printf(const char *__restrict__ format, ...);


#endif