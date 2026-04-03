#ifndef __STRING_H
#define __STRING_H

#include "stdint.h"

size_t strlen(const char* str);
size_t strcpy(char *__restrict__ dst, const char *__restrict__ src);
size_t strncpy(char *__restrict__ dst, const char *__restrict__ src, size_t n);


#endif