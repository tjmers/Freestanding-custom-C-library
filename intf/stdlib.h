#ifndef __STDLIB_H
#define __SDTLIB_H

#include "error.h"
#include "exit.h"
#include "stdint.h"

void* malloc(size_t n);
void free(void* ptr);

// Returns the number of bytes written to the buffer.
// Resumable: takes the integer by pointer and updates it to the remaining
// value not yet written (0 when complete). Useful for buffered streams.
uint32_t itoa(int64_t* i, char* buf, size_t buff_size);

uint32_t itoab(int i, char* buf, size_t buff_size);
uint32_t itoax(int i, char* buf, size_t buff_size);


// Conversion functions
int atoi(const char* nptr);

void __malloc_lock(void);
void __malloc_unlock(void);

#endif