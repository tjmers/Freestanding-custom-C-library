#ifndef __EXIT_CODES_H
#define __EXIT_CODES_H

#include "stdbool.h"
#include "syscalls.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define EXIT_FAILURE_BAD_HEAP_INITIALIZATION 2

#define ATEXIT_MAX_CALLBACKS 64

typedef void (*callback_t)(void);

bool atexit_push(callback_t func);

callback_t atexit_pop();

void exit(int exit_code);

#endif // __EXIT_CODES_H