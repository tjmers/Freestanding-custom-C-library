#ifndef __STDLIB_H
#define __SDTLIB_H

#include "error.h"
#include "exit.h"
#include "stdbool.h"
#include "stdint.h"

void* malloc(size_t n);
void free(void* ptr);

// Returns the number of bytes written to the buffer.
// Resumable: takes the integer by pointer and updates it to the remaining
// value not yet written (0 when complete). Useful for buffered streams.
uint32_t itoa(int64_t* i, char* buf, size_t buff_size);

uint32_t itoab(int64_t* i, char* buf, size_t buff_size);
uint32_t itoao(int64_t* i, char* buf, size_t buff_size);
uint32_t itoax(int64_t* i, char* buf, size_t buff_size, bool upper);

uint32_t utoa(uint64_t* i, char* buf, size_t buff_size);
uint32_t utoab(uint64_t* i, char* buf, size_t buff_size);
uint32_t utoao(uint64_t* i, char* buf, size_t buff_size);
uint32_t utoax(uint64_t* i, char* buf, size_t buff_size, bool upper);


// Conversion functions
int atoi(const char* nptr);

void __malloc_lock(void);
void __malloc_unlock(void);

#endif