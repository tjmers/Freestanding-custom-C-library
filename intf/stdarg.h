#ifndef __VARARGS_H
#define __VARARGS_H

typedef __builtin_va_list va_list;

#define va_start(ap, last) (__builtin_va_start(ap, last))
#define va_arg(ap, type) (__builtin_va_arg(ap, type))
#define va_copy(v, l) (__builtin_va_copy(v, l))

#endif