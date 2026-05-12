// This file is completely vibe coded.
/// @file conversions.c
/// Implements integer/string conversion helpers.

#include "../intf/stdint.h"
#include "../intf/stdbool.h"
#include "../intf/stdlib.h"

uint32_t itoa(int64_t* i, char* buffer, size_t buff_size) {
  if (i == NULL || buffer == NULL || buff_size == 0) {
    return 0;
  }

  // Special-case 0. This is "complete" in one write.
  if (*i == 0) {
    buffer[0] = '0';
    return 1;
  }

  uint32_t w = 0;

  // Resumable state encoding to preserve leading zeros in the remaining suffix:
  // When we can't finish, we store: *i = INT64_MIN + (10^m + suffix), where m
  // is the number of digits remaining (0..18) and suffix is the remaining value
  // (0..10^m-1).
  //
  // Encoded values live near INT64_MIN (INT64_MIN + x).
  // IMPORTANT: INT64_MIN itself is a valid input value (not an encoding marker).
  const int64_t enc_limit = INT64_MIN + (int64_t)(1ull << 61);
  bool encoded = (*i > INT64_MIN) && (*i <= enc_limit);
  uint64_t v = 0;
  uint32_t digits = 0;
  uint64_t div = 1;

  if (encoded) {
    uint64_t u = (uint64_t)(*i - INT64_MIN);
    // Find base = 10^m (highest power of 10 <= u).
    uint64_t base = 1;
    uint32_t m = 0;
    while (base <= u / 10) {
      base *= 10;
      ++m;
    }
    v = u - base;    // remaining suffix value (may be 0)
    digits = m;      // number of digits remaining (may be 0)
    div = base / 10; // 10^(m-1)
    if (digits == 0) {
      *i = (int64_t)v;
      return 0;
    }
  } else {
    // Work with an unsigned magnitude so INT64_MIN is supported.
    bool neg = (*i < 0);
    if (neg) {
      if (buff_size < 2) {
        return 0;
      }
      buffer[w++] = '-';
      --buff_size;

      int64_t sv = *i;
      v = (uint64_t)(-(sv + 1)) + 1; // abs(sv) as uint64_t
    } else {
      v = (uint64_t)(*i);
    }

    // Count digits and compute highest power of 10.
    uint64_t t = v;
    while (t) {
      ++digits;
      t /= 10;
    }
    div = 1;
    for (uint32_t j = 1; j < digits; ++j) {
      div *= 10;
    }
  }

  uint32_t k = digits;
  if (k > buff_size) {
    k = (uint32_t)buff_size;
  }

  for (uint32_t j = 0; j < k; ++j) {
    uint64_t d = v / div;
    buffer[w++] = (char)('0' + (char)d);
    v %= div;
    div /= 10;
  }

  uint32_t remaining_digits = digits - k;
  if (remaining_digits == 0) {
    *i = 0;
    return w;
  }

  uint64_t base = 1;
  for (uint32_t j = 0; j < remaining_digits; ++j) {
    base *= 10;
  }
  *i = INT64_MIN + (int64_t)(base + v);
  return w;
}

static uint32_t utoa_generic(uint64_t* i, char* buffer, size_t buff_size, uint32_t radix,
                            const char* digits, uint32_t max_rem_digits) {
  if (i == NULL || buffer == NULL || buff_size == 0) {
    return 0;
  }
  if (*i == 0) {
    buffer[0] = '0';
    return 1;
  }

  // Encoded values live near UINT64_MAX: UINT64_MAX - (radix^m + suffix).
  const uint64_t enc_span = (1ull << 61);
  const uint64_t enc_start = UINT64_MAX - enc_span;
  bool encoded = (*i >= enc_start);

  uint64_t v = 0;
  uint32_t digits_total = 0;
  uint64_t div = 1;

  if (encoded) {
    uint64_t u = UINT64_MAX - (*i); // radix^m + suffix
    uint64_t base = 1;
    uint32_t m = 0;
    while (base <= u / radix) {
      base *= radix;
      ++m;
    }
    v = u - base;
    digits_total = m;
    div = base / radix;
    if (digits_total == 0) {
      *i = v;
      return 0;
    }
  } else {
    v = *i;
    uint64_t t = v;
    while (t) {
      ++digits_total;
      t /= radix;
    }
    div = 1;
    for (uint32_t j = 1; j < digits_total; ++j) {
      div *= radix;
    }
  }

  if (!encoded && digits_total > max_rem_digits + buff_size) {
    return 0;
  }

  uint32_t k = digits_total;
  if (k > buff_size) {
    k = (uint32_t)buff_size;
  }

  uint32_t w = 0;
  for (uint32_t j = 0; j < k; ++j) {
    uint64_t d = v / div;
    buffer[w++] = digits[d];
    v %= div;
    div /= radix;
  }

  uint32_t remaining = digits_total - k;
  if (remaining == 0) {
    *i = 0;
    return w;
  }

  uint64_t base = 1;
  for (uint32_t j = 0; j < remaining; ++j) {
    base *= radix;
  }
  *i = UINT64_MAX - (base + v);
  return w;
}

static uint32_t itoa_generic(int64_t* i, char* buffer, size_t buff_size, uint32_t radix,
                             const char* digits, uint32_t max_rem_digits) {
  if (i == NULL || buffer == NULL || buff_size == 0) {
    return 0;
  }
  if (*i == 0) {
    buffer[0] = '0';
    return 1;
  }

  const int64_t enc_limit_local = INT64_MIN + (int64_t)(1ull << 61);
  bool encoded = (*i > INT64_MIN) && (*i <= enc_limit_local);

  uint32_t w = 0;
  uint64_t v = 0;
  uint32_t digits_total = 0;
  uint64_t div = 1;

  if (encoded) {
    uint64_t u = (uint64_t)(*i - INT64_MIN); // radix^m + suffix
    uint64_t base = 1;
    uint32_t m = 0;
    while (base <= u / radix) {
      base *= radix;
      ++m;
    }
    v = u - base;
    digits_total = m;
    div = base / radix;
    if (digits_total == 0) {
      *i = (int64_t)v;
      return 0;
    }
  } else {
    bool neg = (*i < 0);
    if (neg) {
      if (buff_size < 2) {
        return 0;
      }
      buffer[w++] = '-';
      --buff_size;
      int64_t sv = *i;
      v = (uint64_t)(-(sv + 1)) + 1; // abs as u64
    } else {
      v = (uint64_t)(*i);
    }

    uint64_t t = v;
    while (t) {
      ++digits_total;
      t /= radix;
    }
    div = 1;
    for (uint32_t j = 1; j < digits_total; ++j) {
      div *= radix;
    }
  }

  if (!encoded && digits_total > max_rem_digits + buff_size) {
    return 0;
  }

  uint32_t k = digits_total;
  if (k > buff_size) {
    k = (uint32_t)buff_size;
  }
  for (uint32_t j = 0; j < k; ++j) {
    uint64_t d = v / div;
    buffer[w++] = digits[d];
    v %= div;
    div /= radix;
  }

  uint32_t remaining = digits_total - k;
  if (remaining == 0) {
    *i = 0;
    return w;
  }

  uint64_t base = 1;
  for (uint32_t j = 0; j < remaining; ++j) {
    base *= radix;
  }
  *i = INT64_MIN + (int64_t)(base + v);
  return w;
}

uint32_t utoa(uint64_t* i, char* buf, size_t buff_size) {
  return utoa_generic(i, buf, buff_size, 10u, "0123456789", 18u);
}
uint32_t utoab(uint64_t* i, char* buf, size_t buff_size) {
  return utoa_generic(i, buf, buff_size, 2u, "01", 60u);
}
uint32_t utoao(uint64_t* i, char* buf, size_t buff_size) {
  return utoa_generic(i, buf, buff_size, 8u, "01234567", 20u);
}
uint32_t utoax(uint64_t* i, char* buf, size_t buff_size, bool upper) {
  return utoa_generic(i, buf, buff_size, 16u, upper ? "0123456789ABCDEF" : "0123456789abcdef", 15u);
}

uint32_t itoab(int64_t* i, char* buf, size_t buff_size) {
  return itoa_generic(i, buf, buff_size, 2u, "01", 60u);
}
uint32_t itoao(int64_t* i, char* buf, size_t buff_size) {
  return itoa_generic(i, buf, buff_size, 8u, "01234567", 20u);
}
uint32_t itoax(int64_t* i, char* buf, size_t buff_size, bool upper) {
  return itoa_generic(i, buf, buff_size, 16u, upper ? "0123456789ABCDEF" : "0123456789abcdef", 15u);
}

int atoi(const char* nptr) {
  if (nptr == NULL) {
    return 0;
  }
  while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n' || *nptr == '\r' || *nptr == '\f' || *nptr == '\v') {
    ++nptr;
  }
  int sign = 1;
  if (*nptr == '-') {
    sign = -1;
    ++nptr;
  } else if (*nptr == '+') {
    ++nptr;
  }
  int val = 0;
  while (*nptr >= '0' && *nptr <= '9') {
    val = val * 10 + (*nptr - '0');
    ++nptr;
  }
  return sign * val;
}

