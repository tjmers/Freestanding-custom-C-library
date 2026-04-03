#ifndef __STDLIB_H
#define __SDTLIB_H

#include "error.h"
#include "exit.h"
#include "stdint.h"

void* malloc(size_t n);
void free(void* ptr);

// Retusn the number of bytes written to the buffer
uint32_t itoa(int i, char* buf, size_t buff_size);

uint32_t itoab(int i, char* buf, size_t buff_size);
uint32_t itoax(int i, char* buf, size_t buff_size);


// Conversion functions
int atoi(const char* nptr);

#endif