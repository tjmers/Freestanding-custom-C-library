#ifndef __ASSERT_H
#define __ASSERT_H

#undef assert
#undef static_assert

#if !defined(__cplusplus) // static_assert wouldn't work if C++ is begin used (its a keyword)
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) || defined(__GNUC__)
#ifndef static_assert
#define static_assert _Static_assert
#endif
#endif
#endif

#ifdef NDEBUG
#define assert(ignore) ((void)0)
#else
void __assert_fail(const char *expr, const char *file, unsigned int line,
                   const char *func);
#define assert(expr)                                                           \
  ((void)((expr) ? 0 : (__assert_fail(#expr, __FILE__, __LINE__, __func__), 0)))
#endif

#endif
