#ifndef __ERROR_H
#define __ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

typedef uint32_t error_t;

#define ERROR_OK 0
#define ERROR_NOT_ENOUGH_MEMORY 1
#define ERROR_BUFFER_OVERFLOW 2

#ifdef __cplusplus
}
#endif

#endif
