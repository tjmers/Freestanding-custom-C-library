#ifndef __MEMORY_H
#define __MEMORY_H

#include "stdint.h"

// Meant for larger copies
void* memcpy_large(void *__restrict__ dst, void *__restrict__ src, size_t n);

// Meant for small copies (less than 1KB)
// Its actually just memcpy large  unless fsrm is supported (typical) then rep movsb is used instead (see init.c)
extern void* (*memcpy_small)(void *__restrict__, void *__restrict__, size_t);


// Not restrict beacuse they may overlap
// Move src into a heap allocated buffer before dst
void* memmove_large(void* dst, void* src, size_t n);

// Implemented in assembly in src/memcpy_small_fsrm.asm
// Returns dst
void* memcpy_small_fsrm(void* src, void* dst, size_t n);

#endif