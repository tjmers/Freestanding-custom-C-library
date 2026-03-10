#ifndef MY_STDINT_H
#define MY_STDINT_H

/* Detect compiler built-in type sizes */
#if defined(__SIZEOF_CHAR__)
  #define MY_SIZEOF_CHAR __SIZEOF_CHAR__
#else
  #define MY_SIZEOF_CHAR 1
#endif

#if defined(__SIZEOF_SHORT__)
  #define MY_SIZEOF_SHORT __SIZEOF_SHORT__
#else
  #define MY_SIZEOF_SHORT 2
#endif

#if defined(__SIZEOF_INT__)
  #define MY_SIZEOF_INT __SIZEOF_INT__
#else
  #define MY_SIZEOF_INT 4
#endif

#if defined(__SIZEOF_LONG__)
  #define MY_SIZEOF_LONG __SIZEOF_LONG__
#else
  #define MY_SIZEOF_LONG 4
#endif

#if defined(__SIZEOF_LONG_LONG__)
  #define MY_SIZEOF_LONG_LONG __SIZEOF_LONG_LONG__
#else
  #define MY_SIZEOF_LONG_LONG 8
#endif

/* 8-bit integers */
#if MY_SIZEOF_CHAR == 1
typedef signed char int8_t;
typedef unsigned char uint8_t;
#else
#error "No 8-bit type available"
#endif

/* 16-bit integers */
#if MY_SIZEOF_SHORT == 2
typedef short int16_t;
typedef unsigned short uint16_t;
#elif MY_SIZEOF_INT == 2
typedef int int16_t;
typedef unsigned int uint16_t;
#else
#error "No 16-bit type available"
#endif

/* 32-bit integers */
#if MY_SIZEOF_INT == 4
typedef int int32_t;
typedef unsigned int uint32_t;
#elif MY_SIZEOF_LONG == 4
typedef long int32_t;
typedef unsigned long uint32_t;
#else
#error "No 32-bit type available"
#endif

/* 64-bit integers */
#if MY_SIZEOF_LONG_LONG == 8
typedef long long int64_t;
typedef unsigned long long uint64_t;
#elif MY_SIZEOF_LONG == 8
typedef long int64_t;
typedef unsigned long uint64_t;
#else
#error "No 64-bit type available"
#endif

typedef uint64_t size_t;

/* Limits */
#define INT8_MIN  (-128)
#define INT8_MAX  (127)
#define UINT8_MAX (255U)

#define INT16_MIN  (-32768)
#define INT16_MAX  (32767)
#define UINT16_MAX (65535U)

#define INT32_MIN  (-2147483647-1)
#define INT32_MAX  (2147483647)
#define UINT32_MAX (4294967295U)

#define INT64_MIN  (-9223372036854775807LL-1)
#define INT64_MAX  (9223372036854775807LL)
#define UINT64_MAX (18446744073709551615ULL)

#endif /* MY_STDINT_H */