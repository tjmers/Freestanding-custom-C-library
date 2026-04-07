#ifndef __STRING_H
#define __STRING_H

#include "stdbool.h"
#include "stdint.h"

size_t strlen(const char* str);
size_t strcpy(char *__restrict__ dst, const char *__restrict__ src);
size_t strncpy(char *__restrict__ dst, const char *__restrict__ src, size_t n);
ptrdiff_t strcmp(const char *__restrict__ s1, const char *__restrict__ s2);
char* strtokc(char* begin, char ch);
bool strtok_end();
char* strdup(const char* str);


#endif