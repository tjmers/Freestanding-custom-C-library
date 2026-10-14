#ifndef __STDDEF_H
#define __STDDEF_H

#ifdef __cplusplus
extern "C" {
#endif

// Bools
#include "stdbool.h"

// Int types
#include "stdint.h"

// Null Definition
#define NULL 0

// Linux definitions
typedef int32_t pid_t;
typedef int64_t off_t;



#ifdef __cplusplus
}
#endif

#endif
