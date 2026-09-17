#include "memory.h"



void* (*memcpy_small)(void *__restrict__, void *__restrict__, size_t) = memcpy_large;

// Optimized for different archetectuires
void* memcpy_large(void *__restrict__ dst, void *__restrict__ src, size_t n) {
  void* ret = dst;
  (void)dst;
  (void)src;
  (void)n;
#if defined(__AVX512F__)
  // AVX-512 foundation available

  // Copy unaligned prefix to bring dst to 64-byte alignment
  size_t align = (64 - ((size_t)dst & 63)) & 63;
  if (align > n) align = n;
  __asm__ volatile (
      "rep movsb"
      : "+D"(dst), "+S"(src), "+c"(align)
      :
      : "memory"
  );
  n -= align;

  // Aligned 64-byte loop
  while (n >= 64) {
    __asm__ volatile (
      "vmovdqu64  (%1), %%zmm0\n\t"
      "vmovdqa64  %%zmm0, (%0)\n\t"
      :
      : "r"(dst), "r"(src)
      : "memory", "zmm0"
    );
    dst += 64;
    src += 64;
    n   -= 64;
  }

  // Remaining bytes
  __asm__ volatile (
    "rep movsb"
    : "+D"(dst), "+S"(src), "+c"(n)
    :
    : "memory"
  );

  __asm__ volatile ("vzeroupper");

#elif defined(__AVX2__)
  // AVX2 available

  // Copy unaligned prefix to bring dst to 32-byte alignment
  size_t align = (31 - ((size_t)dst & 31)) & 31;
  if (align > n) align = n;
  __asm__ volatile (
      "rep movsb"
      : "+D"(dst), "+S"(src), "+c"(align)
      :
      : "memory"
  );
  n -= align;

  // Aligned 32-byte loop
  while (n >= 32) {
    __asm__ volatile (
      "vmovdqu64  (%1), %%ymm0\n\t"
      "vmovdqa64  %%ymm0, (%0)\n\t"
      :
      : "r"(dst), "r"(src)
      : "memory", "ymm0"
    );
    dst += 64;
    src += 64;
    n   -= 64;
  }

  // Remaining bytes
  __asm__ volatile (
    "rep movsb"
    : "+D"(dst), "+S"(src), "+c"(n)
    :
    : "memory"
  );

  __asm__ volatile ("vzeroupper");

#elif defined(__SSE2__)
  // SSE2 (baseline on x86-64, always present)
  char* s = (char*)src;
  char* d = (char*)dst;
  for (size_t i = 0; i < n; ++i) {
    d[i] = s[i];
  }
#else
  // scalar fallback
  __asm__ volatile (
    "rep movsb"
    : "+D"(dst), "+S"(src), "+c"(n)
    :
    : "memory"
  );
#endif
  return ret;
}