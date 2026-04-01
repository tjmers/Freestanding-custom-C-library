#include "memory.h"

// If FSRM is enabled, we provide an optimized NASM implementation and use a C
// wrapper to keep the public symbol name stable.
#ifdef FSRM
extern void* memcpy_small_fsrm(void *__restrict__ dst, void *__restrict__ src, size_t n);
#endif

void* memcpy_small(void *__restrict__ dst, void *__restrict__ src, size_t n) {
#ifdef FSRM
  return memcpy_small_fsrm(dst, src, n);
#else
  void* ret = dst;
  __asm__ volatile (
    "rep movsb"
    : "+D"(dst), "+S"(src), "+c"(n)
    :
    : "memory"
  );
  return ret;
#endif
}

// Optimized for different archetectuires
void* memcpy_large(void *__restrict__ dst, void *__restrict__ src, size_t n) {
  void* ret = dst;
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