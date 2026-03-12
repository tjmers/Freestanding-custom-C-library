#ifndef __MEMORY_H
#define __MEMORY_H

#include "stdint.h"

// Meant for small copies (less than 1KB)
void* memcpy_small(void *__restrict__ dst, void *__restrict__ src, size_t n);

// Meant for larger copies
void* memcpy_large(void *__restrict__ dst, void *__restrict__ src, size_t n);

// Not restrict beacuse they may overlap
// Move src into a stack allocated buffer before into dst
void* memmove_small(void* dst, void* src, size_t n);


// Not restrict beacuse they may overlap
// Move src into a heap allocated buffer before dst
void* memmove_large(void* dst, void* src, size_t n);

#endif