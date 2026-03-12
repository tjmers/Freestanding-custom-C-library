// These are wrappers for linux syscalls


#ifndef __SYSCALLS_H
#define __SYSCALLS_H

#include "stddef.h"
#include "mmap_macros.h"

void write(unsigned int fd, const char* buf, size_t count);
void* brk(void* brk);
void* mmap(size_t length, void* addr, int prot, int flags, int fd, int64_t offset);
void munmap(uint64_t addr, size_t len);
void exit(int error_code);
pid_t fork();
void execve(const char* filename, const char *const *argv, const char *const *envp);
pid_t getpid();


#endif