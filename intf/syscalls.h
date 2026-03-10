// These are wrappers for linux syscalls


#ifndef __SYSCALLS_H
#define __SYSCALLS_H

#include "stdint.h"
#include "mmap_macros.h"

void write(unsigned int fd, const char* buf, size_t count);
void* brk(void* brk);
void* mmap(size_t length, void* addr, int prot, int flags, int fd, int64_t offset);
void munmap(uint64_t addr, size_t len);
void exit(int error_code);


#endif