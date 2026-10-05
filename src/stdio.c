#include "../intf/stdio.h"

#include "../intf/exit.h"
#include "../intf/stdarg.h"
#include "../intf/stdint.h"
#include "../intf/stdlib.h"
#include "../intf/string.h"
#include "../intf/syscalls.h"

#include "_stdio.h"

#define __STDIO_NO_BUFFER UINT32_MAX

struct _File {
  int fd_;
  uint32_t buffer_index_;
  char buffer_[__STDIO_BUFFER_SIZE];
};

static FILE stdout_ = { 1, 0, {0} };
static FILE stderr_ = { 2, __STDIO_NO_BUFFER, {0} };


OutputStream stdout = &stdout_;
OutputStream stderr = &stderr_;


// Writes n bytes to the stream. Unlike fputs, the bytes may contain '\0'.
static bool fput_bytes(OutputStream out, const char* str, size_t n) {
  if (out->buffer_index_ == __STDIO_NO_BUFFER) {
    while (n) {
      int written = write(out->fd_, str, n);
      if (written < 0) {
        return false;
      }
      str += written;
      n -= written;
    }
    return true;
  }
  while (n) {
    if (out->buffer_index_ == __STDIO_BUFFER_SIZE && !fflush(out)) {
      return false;
    }
    size_t chunk = __STDIO_BUFFER_SIZE - out->buffer_index_;
    if (chunk > n) {
      chunk = n;
    }
    for (size_t i = 0; i < chunk; ++i) {
      out->buffer_[out->buffer_index_ + i] = str[i];
    }
    out->buffer_index_ += chunk;
    str += chunk;
    n -= chunk;
  }
  return true;
}

// --- Formatted output ---

// Tracks where fprintf writes to and how many bytes it has written (for %n).
typedef struct {
  OutputStream out;
  size_t count;
} fprintf_sink;

typedef union {
  int64_t i;
  uint64_t u;
  const void* p;
  long double f;
} fprintf_data;

static bool sink_bytes(fprintf_sink* sink, const char* str, size_t n) {
  sink->count += n;
  return fput_bytes(sink->out, str, n);
}

static bool sink_char(fprintf_sink* sink, char ch) {
  return sink_bytes(sink, &ch, 1);
}

static bool sink_repeat(fprintf_sink* sink, char ch, size_t n) {
  char chunk[64];
  for (size_t i = 0; i < sizeof(chunk); ++i) {
    chunk[i] = ch;
  }
  while (n) {
    size_t len = n < sizeof(chunk) ? n : sizeof(chunk);
    if (!sink_bytes(sink, chunk, len)) {
      return false;
    }
    n -= len;
  }
  return true;
}

static int parse_number(const char **fmt) {
    int value = 0;

    while (**fmt >= '0' && **fmt <= '9') {
        value = value * 10 + (**fmt - '0');
        (*fmt)++;
    }

    return value;
}

static int get_fprintf_flags(const char** str) {
  int flags = __STDIO_FPRINTF_FORMAT_FLAGS_DEFAULT;
  while (1) {
    switch (**str) {
      case '-':
        flags |= __STDIO_FPRINTF_FORMAT_FLAGS_LEFT_ALIGN;
        break;
      case '+':
        flags |= __STDIO_FPRINTF_FORMAT_FLAGS_SHOW_SIGN;
        break;
      case ' ':
        flags |= __STDIO_FPRINTF_FORMAT_FLAGS_SPACE_IF_NO_SIGN;
        break;
      case '0':
        flags |= __STDIO_FPRINTF_FORMAT_FLAGS_LEADING_ZEROS;
        break;
      case '#':
        flags |= __STDIO_FPRINTF_FORMAT_FLAGS_USE_ALTERNATE;
        break;
      default:
        return flags;
    }
    ++(*str);
  }
  __builtin_unreachable();
}

static int get_length_modifier(const char** str) {
  int length = __STDIO_FPRINTF_FORMAT_LENGTH_DEFAULT;
  switch (**str) {
    case 'h':
      if ((*str)[1] == 'h') {
        ++(*str);
        length = __STDIO_FPRINTF_FORMAT_LENGTH_CHAR;
      } else {
        length = __STDIO_FPRINTF_FORMAT_LENGTH_SHORT;
      }
      break;
    case 'l':
      if ((*str)[1] == 'l') {
        ++(*str);
        length = __STDIO_FPRINTF_FORMAT_LENGTH_LONG_LONG;
      } else {
        length = __STDIO_FPRINTF_FORMAT_LENGTH_LONG;
      }
      break;
    case 'z':
      length = __STDIO_FPRINTF_FORMAT_LENGTH_SIZE_T;
      break;
    case 't':
      length = __STDIO_FPRINTF_FORMAT_LENGTH_PTRDIFF_T;
      break;
    case 'j':
      length = __STDIO_FPRINTF_FORMAT_LENGTH_INTMAX_T;
      break;
    case 'L':
      length = __STDIO_FPRINTF_FORMAT_LENGTH_LONG_DOUBLE;
      break;
    default:
      --(*str);
      break;
  }
  ++(*str);
  return length;
}

// Returns __STDIO_FPRINTF_FORMAT_SPECIFIER_DEFAULT for an unknown conversion.
static int get_format_specifier(const char** str) {
  int fmt = __STDIO_FPRINTF_FORMAT_SPECIFIER_DEFAULT;
  switch (**str) {
    case 'd':
    case 'i':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_SIGNED_DECIMAL;
      break;
    case 'u':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_UNSIGNED_DECIMAL;
      break;
    case 'o':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_OCTAL;
      break;
    case 'X':
    case 'x':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_HEX;
      break;
    case 'F':
    case 'f':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_FLOAT;
      break;
    case 'e':
    case 'E':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_SCIENTIFIC;
      break;
    case 'g':
    case 'G':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_SMART;
      break;
    case 'a':
    case 'A':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_HEX_FLOAT;
      break;
    case 'c':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_CHAR;
      break;
    case 's':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_STRING;
      break;
    case 'p':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_POINTER;
      break;
    case 'n':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_COUNT;
      break;
    default:
      return fmt;
  }
  if (**str >= 'A' && **str <= 'Z') {
    fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_UPPERCASE;
  }
  ++(*str);
  return fmt;
}

// This function is really ugly because of default argument promotion and different sizes on different architectures.
// Signed values are sign extended into data->i and unsigned values zero extended into data->u.
// The va_list is taken by pointer so that the caller's list advances on every architecture.
static void fprintf_get_data(va_list* args, int length, int specifier, fprintf_data* res) {
  res->u = 0;
  switch (specifier & ~__STDIO_FPRINTF_FORMAT_SPECIFIER_UPPERCASE) {
    // All pointers
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_STRING:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_POINTER:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_COUNT:
      res->p = va_arg(*args, const void*);
      break;
    // All signed integers
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_SIGNED_DECIMAL:
      switch (length) {
        case __STDIO_FPRINTF_FORMAT_LENGTH_CHAR:
          res->i = (signed char)va_arg(*args, int);
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_SHORT:
          res->i = (short)va_arg(*args, int);
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_LONG:
          res->i = va_arg(*args, long);
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_LONG_LONG:
          res->i = va_arg(*args, long long);
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_SIZE_T:
        case __STDIO_FPRINTF_FORMAT_LENGTH_PTRDIFF_T:
#if MY_SIZEOF_PTRDIFF_T < MY_SIZEOF_INT
          res->i = (ptrdiff_t)va_arg(*args, int);
#else
          res->i = va_arg(*args, ptrdiff_t);
#endif
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_INTMAX_T:
          res->i = va_arg(*args, intmax_t);
          break;
        default:
          res->i = va_arg(*args, int);
          break;
      }
      break;
    // All unsigned integers
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_OCTAL:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_HEX:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_UNSIGNED_DECIMAL:
      switch (length) {
        case __STDIO_FPRINTF_FORMAT_LENGTH_CHAR:
          res->u = (unsigned char)va_arg(*args, unsigned int);
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_SHORT:
          res->u = (unsigned short)va_arg(*args, unsigned int);
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_LONG:
          res->u = va_arg(*args, unsigned long);
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_LONG_LONG:
          res->u = va_arg(*args, unsigned long long);
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_SIZE_T:
#if MY_SIZEOF_SIZE_T < MY_SIZEOF_INT // Gets promoted to int
          res->u = (size_t)va_arg(*args, int);
#elif MY_SIZEOF_SIZE_T == MY_SIZEOF_INT // Gets promoted to unsigned int
          res->u = (size_t)va_arg(*args, unsigned int);
#else
          res->u = va_arg(*args, size_t);
#endif
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_PTRDIFF_T:
#if MY_SIZEOF_PTRDIFF_T < MY_SIZEOF_INT
          res->u = (uptrdiff_t)va_arg(*args, int);
#elif MY_SIZEOF_PTRDIFF_T == MY_SIZEOF_INT // Gets promoted to unsigned int
          res->u = (uptrdiff_t)va_arg(*args, unsigned int);
#else
          res->u = va_arg(*args, uptrdiff_t);
#endif
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_INTMAX_T:
          res->u = va_arg(*args, uintmax_t);
          break;
        default:
          res->u = va_arg(*args, unsigned);
          break;
      }
      break;
    // Both char and wint_t are promoted to (unsigned) int
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_CHAR:
      res->u = va_arg(*args, unsigned int);
      break;
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_FLOAT:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_SCIENTIFIC:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_SMART:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_HEX_FLOAT:
      if (length == __STDIO_FPRINTF_FORMAT_LENGTH_LONG_DOUBLE) {
        res->f = va_arg(*args, long double);
      } else {
        res->f = va_arg(*args, double);
      }
      break;
  }
}

// Writes the padding in front of a field of len bytes (prefix included), then the prefix.
// Zero padding goes between the prefix and the body, space padding goes before the prefix.
static bool fprintf_field_begin(fprintf_sink* sink, const fprintf_state* state, size_t len,
                                const char* prefix, size_t prefix_len, bool zero_pad) {
  size_t pad = (size_t)state->width > len ? (size_t)state->width - len : 0;
  if (state->flags & __STDIO_FPRINTF_FORMAT_FLAGS_LEFT_ALIGN) {
    return sink_bytes(sink, prefix, prefix_len);
  }
  if (zero_pad) {
    return sink_bytes(sink, prefix, prefix_len) && sink_repeat(sink, '0', pad);
  }
  return sink_repeat(sink, ' ', pad) && sink_bytes(sink, prefix, prefix_len);
}

// Writes the padding after a left aligned field of len bytes.
static bool fprintf_field_end(fprintf_sink* sink, const fprintf_state* state, size_t len) {
  if (!(state->flags & __STDIO_FPRINTF_FORMAT_FLAGS_LEFT_ALIGN) || (size_t)state->width <= len) {
    return true;
  }
  return sink_repeat(sink, ' ', (size_t)state->width - len);
}

// Returns the sign character a signed conversion should print, or 0 for none.
static char fprintf_sign(const fprintf_state* state, bool negative) {
  if (negative) {
    return '-';
  }
  if (state->flags & __STDIO_FPRINTF_FORMAT_FLAGS_SHOW_SIGN) {
    return '+';
  }
  if (state->flags & __STDIO_FPRINTF_FORMAT_FLAGS_SPACE_IF_NO_SIGN) {
    return ' ';
  }
  return 0;
}

static bool fprintf_output_integer(fprintf_sink* sink, const fprintf_data* data, const fprintf_state* state) {
  int specifier = state->specifier & ~__STDIO_FPRINTF_FORMAT_SPECIFIER_UPPERCASE;
  bool upper = (state->specifier & __STDIO_FPRINTF_FORMAT_SPECIFIER_UPPERCASE) != 0;
  bool alt = (state->flags & __STDIO_FPRINTF_FORMAT_FLAGS_USE_ALTERNATE) != 0;
  const char* hex_digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

  uint64_t value = data->u;
  bool negative = false;
  unsigned base = 10;
  if (specifier == __STDIO_FPRINTF_FORMAT_SPECIFIER_SIGNED_DECIMAL) {
    negative = data->i < 0;
    // Negate as unsigned so INT64_MIN works
    value = negative ? 0 - data->u : data->u;
  } else if (specifier == __STDIO_FPRINTF_FORMAT_SPECIFIER_OCTAL) {
    base = 8;
  } else if (specifier == __STDIO_FPRINTF_FORMAT_SPECIFIER_HEX || specifier == __STDIO_FPRINTF_FORMAT_SPECIFIER_POINTER) {
    base = 16;
  }

  char digits[24];
  size_t n_digits = 0;
  // A precision of 0 prints no digits for the value 0
  if (value != 0 || state->precision != 0) {
    size_t i = sizeof(digits);
    do {
      digits[--i] = hex_digits[value % base];
      value /= base;
    } while (value);
    n_digits = sizeof(digits) - i;
    for (size_t j = 0; j < n_digits; ++j) {
      digits[j] = digits[i + j];
    }
  }

  size_t zeros = 0;
  if (state->precision > 0 && (size_t)state->precision > n_digits) {
    zeros = (size_t)state->precision - n_digits;
  }

  char prefix[2] = { 0 };
  size_t prefix_len = 0;
  if (specifier == __STDIO_FPRINTF_FORMAT_SPECIFIER_SIGNED_DECIMAL) {
    char sign = fprintf_sign(state, negative);
    if (sign) {
      prefix[prefix_len++] = sign;
    }
  } else if (specifier == __STDIO_FPRINTF_FORMAT_SPECIFIER_OCTAL) {
    // The alternate form makes the first digit a zero
    if (alt && zeros == 0 && (n_digits == 0 || digits[0] != '0')) {
      zeros = 1;
    }
  } else if (base == 16 && data->u != 0 && (alt || specifier == __STDIO_FPRINTF_FORMAT_SPECIFIER_POINTER)) {
    prefix[prefix_len++] = '0';
    prefix[prefix_len++] = upper ? 'X' : 'x';
  }

  bool zero_pad = (state->flags & __STDIO_FPRINTF_FORMAT_FLAGS_LEADING_ZEROS) &&
                  state->precision == __STDIO_FPRINTF_FORMAT_PRECISION_NOT_SPECIFIED;
  size_t len = prefix_len + zeros + n_digits;
  return fprintf_field_begin(sink, state, len, prefix, prefix_len, zero_pad) &&
         sink_repeat(sink, '0', zeros) &&
         sink_bytes(sink, digits, n_digits) &&
         fprintf_field_end(sink, state, len);
}

static bool fprintf_output_string(fprintf_sink* sink, const char* str, size_t len, const fprintf_state* state) {
  return fprintf_field_begin(sink, state, len, "", 0, false) &&
         sink_bytes(sink, str, len) &&
         fprintf_field_end(sink, state, len);
}

// Wide characters are converted as in the C locale, where only ASCII is representable.
static bool fprintf_output_wide_string(fprintf_sink* sink, const __WCHAR_TYPE__* str, const fprintf_state* state) {
  size_t len = 0;
  while ((state->precision < 0 || len < (size_t)state->precision) && str[len]) {
    if ((unsigned)str[len] >= 0x80) {
      return false;
    }
    ++len;
  }
  if (!fprintf_field_begin(sink, state, len, "", 0, false)) {
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    if (!sink_char(sink, (char)str[i])) {
      return false;
    }
  }
  return fprintf_field_end(sink, state, len);
}

// --- Floating point ---

// Exact decimal expansion of a binary floating point value.
// m * 2^e is written as m * 2^e (e >= 0) or m * 5^-e / 10^-e (e < 0), so the decimal digits of
// the big integer m * 2^e or m * 5^-e are exactly the digits of the value. The largest such
// integer comes from the smallest long double denormal, 2^64 * 5^16445, which has 11515 digits.
#define __STDIO_FP_BIG_WORDS 1300
#define __STDIO_FP_BIG_BASE 1000000000u

typedef struct {
  char digits[__STDIO_FP_BIG_WORDS * 9];
  int count; // Number of significant digits, without trailing zeros. 0 for the value zero.
  int exp10; // The value is digits[0].digits[1]digits[2]... * 10^exp10
} fp_decimal;

typedef enum { FP_FINITE, FP_INF, FP_NAN } fp_class;

typedef struct {
  fp_class cls;
  bool negative;
  uint64_t mantissa; // The value is mantissa * 2^exp2
  int exp2;
  int hex_frac_bits; // Bits after the point in %a. The digit before the point holds the rest.
} fp_parts;

static fp_parts fp_split(long double value, bool is_long_double) {
  fp_parts parts;
  if (is_long_double) {
    // x87 extended precision: 64 bit mantissa with an explicit integer bit, 15 bit exponent
    union {
      long double f;
      struct {
        uint64_t mantissa;
        uint16_t sign_exp;
      } bits;
    } u;
    u.f = value;
    int exp = u.bits.sign_exp & 0x7FFF;
    parts.negative = u.bits.sign_exp >> 15;
    parts.mantissa = u.bits.mantissa;
    parts.hex_frac_bits = 60;
    if (exp == 0x7FFF) {
      parts.cls = (parts.mantissa << 1) ? FP_NAN : FP_INF;
    } else {
      parts.cls = FP_FINITE;
    }
    parts.exp2 = (exp ? exp : 1) - 16383 - 63;
  } else {
    union {
      double f;
      uint64_t bits;
    } u;
    u.f = (double)value;
    int exp = (u.bits >> 52) & 0x7FF;
    parts.negative = u.bits >> 63;
    parts.mantissa = u.bits & ((1ull << 52) - 1);
    parts.hex_frac_bits = 52;
    if (exp == 0x7FF) {
      parts.cls = parts.mantissa ? FP_NAN : FP_INF;
    } else {
      parts.cls = FP_FINITE;
    }
    if (exp) {
      parts.mantissa |= 1ull << 52;
    }
    parts.exp2 = (exp ? exp : 1) - 1023 - 52;
  }
  return parts;
}

static int fp_big_mul(uint32_t* big, int n, uint32_t factor) {
  uint64_t carry = 0;
  for (int i = 0; i < n; ++i) {
    uint64_t x = (uint64_t)big[i] * factor + carry;
    big[i] = (uint32_t)(x % __STDIO_FP_BIG_BASE);
    carry = x / __STDIO_FP_BIG_BASE;
  }
  while (carry) {
    big[n++] = (uint32_t)(carry % __STDIO_FP_BIG_BASE);
    carry /= __STDIO_FP_BIG_BASE;
  }
  return n;
}

static void fp_to_decimal(uint64_t mantissa, int exp2, fp_decimal* dec) {
  dec->count = 0;
  dec->exp10 = 0;
  if (mantissa == 0) {
    return;
  }
  while (!(mantissa & 1)) {
    mantissa >>= 1;
    ++exp2;
  }

  // Little endian base 10^9
  uint32_t big[__STDIO_FP_BIG_WORDS];
  int n = 0;
  while (mantissa) {
    big[n++] = (uint32_t)(mantissa % __STDIO_FP_BIG_BASE);
    mantissa /= __STDIO_FP_BIG_BASE;
  }
  if (exp2 > 0) {
    int k = exp2;
    for (; k >= 29; k -= 29) {
      n = fp_big_mul(big, n, 1u << 29);
    }
    n = fp_big_mul(big, n, 1u << k);
  } else {
    int k = -exp2;
    for (; k >= 13; k -= 13) {
      n = fp_big_mul(big, n, 1220703125u); // 5^13
    }
    uint32_t factor = 1;
    while (k--) {
      factor *= 5;
    }
    n = fp_big_mul(big, n, factor);
  }

  int len = 0;
  char top[10];
  int top_len = 0;
  for (uint32_t w = big[n - 1]; w; w /= 10) {
    top[top_len++] = (char)('0' + w % 10);
  }
  while (top_len) {
    dec->digits[len++] = top[--top_len];
  }
  for (int i = n - 2; i >= 0; --i) {
    uint32_t w = big[i];
    for (int j = 8; j >= 0; --j) {
      dec->digits[len + j] = (char)('0' + w % 10);
      w /= 10;
    }
    len += 9;
  }

  dec->exp10 = len - 1 + (exp2 < 0 ? exp2 : 0);
  while (dec->digits[len - 1] == '0') {
    --len;
  }
  dec->count = len;
}

// Rounds to keep significant digits, with ties to even. keep may be 0 or negative.
static void fp_round(fp_decimal* dec, int keep) {
  if (keep >= dec->count) {
    return;
  }
  if (keep < 0) {
    dec->count = 0;
    return;
  }
  char next = dec->digits[keep];
  // Trailing zeros are stripped, so any digit after next is nonzero
  bool round_up = next > '5' ||
                  (next == '5' && (keep + 1 < dec->count || (keep > 0 && (dec->digits[keep - 1] - '0') % 2)));
  dec->count = keep;
  if (round_up) {
    int i = keep - 1;
    while (i >= 0 && dec->digits[i] == '9') {
      --i;
    }
    if (i < 0) {
      dec->digits[0] = '1';
      dec->count = 1;
      ++dec->exp10;
    } else {
      ++dec->digits[i];
      dec->count = i + 1;
    }
  }
  while (dec->count > 0 && dec->digits[dec->count - 1] == '0') {
    --dec->count;
  }
}

// The digit at 10^power
static char fp_digit_at(const fp_decimal* dec, int power) {
  int i = dec->exp10 - power;
  return (i >= 0 && i < dec->count) ? dec->digits[i] : '0';
}

static size_t fp_count_digits(unsigned value) {
  size_t n = 1;
  while (value >= 10) {
    value /= 10;
    ++n;
  }
  return n;
}

// %f with the value already rounded to precision digits after the point.
static bool fp_output_fixed(fprintf_sink* sink, const fprintf_state* state, const fp_decimal* dec,
                            int precision, const char* prefix, size_t prefix_len) {
  bool point = precision > 0 || (state->flags & __STDIO_FPRINTF_FORMAT_FLAGS_USE_ALTERNATE) != 0;
  int int_digits = (dec->count > 0 && dec->exp10 > 0) ? dec->exp10 + 1 : 1;
  size_t len = prefix_len + (size_t)int_digits + point + (size_t)precision;
  bool zero_pad = (state->flags & __STDIO_FPRINTF_FORMAT_FLAGS_LEADING_ZEROS) != 0;
  if (!fprintf_field_begin(sink, state, len, prefix, prefix_len, zero_pad)) {
    return false;
  }
  for (int power = int_digits - 1; power >= 0; --power) {
    if (!sink_char(sink, fp_digit_at(dec, power))) {
      return false;
    }
  }
  if (point && !sink_char(sink, '.')) {
    return false;
  }
  for (int power = -1; power >= -precision; --power) {
    // Past the last significant digit only zeros remain
    if (dec->exp10 - power >= dec->count) {
      if (!sink_repeat(sink, '0', (size_t)(precision + power + 1))) {
        return false;
      }
      break;
    }
    if (!sink_char(sink, fp_digit_at(dec, power))) {
      return false;
    }
  }
  return fprintf_field_end(sink, state, len);
}

// %e with the value already rounded to precision + 1 significant digits.
static bool fp_output_exponent(fprintf_sink* sink, const fprintf_state* state, const fp_decimal* dec,
                               int precision, const char* prefix, size_t prefix_len) {
  bool point = precision > 0 || (state->flags & __STDIO_FPRINTF_FORMAT_FLAGS_USE_ALTERNATE) != 0;
  bool upper = (state->specifier & __STDIO_FPRINTF_FORMAT_SPECIFIER_UPPERCASE) != 0;
  int exp10 = dec->count ? dec->exp10 : 0;
  unsigned exp_abs = exp10 < 0 ? (unsigned)-exp10 : (unsigned)exp10;
  size_t exp_digits = fp_count_digits(exp_abs);
  if (exp_digits < 2) {
    exp_digits = 2;
  }

  size_t len = prefix_len + 1 + point + (size_t)precision + 2 + exp_digits;
  bool zero_pad = (state->flags & __STDIO_FPRINTF_FORMAT_FLAGS_LEADING_ZEROS) != 0;
  if (!fprintf_field_begin(sink, state, len, prefix, prefix_len, zero_pad)) {
    return false;
  }
  if (!sink_char(sink, dec->count ? dec->digits[0] : '0')) {
    return false;
  }
  if (point && !sink_char(sink, '.')) {
    return false;
  }
  int frac_digits = dec->count - 1 < precision ? dec->count - 1 : precision;
  if (frac_digits > 0 && !sink_bytes(sink, &dec->digits[1], (size_t)frac_digits)) {
    return false;
  }
  if (frac_digits < 0) {
    frac_digits = 0;
  }
  if (!sink_repeat(sink, '0', (size_t)(precision - frac_digits))) {
    return false;
  }

  char exp_buf[16];
  size_t i = sizeof(exp_buf);
  for (size_t d = 0; d < exp_digits; ++d) {
    exp_buf[--i] = (char)('0' + exp_abs % 10);
    exp_abs /= 10;
  }
  exp_buf[--i] = exp10 < 0 ? '-' : '+';
  exp_buf[--i] = upper ? 'E' : 'e';
  return sink_bytes(sink, &exp_buf[i], sizeof(exp_buf) - i) &&
         fprintf_field_end(sink, state, len);
}

static bool fp_output_hex(fprintf_sink* sink, const fprintf_state* state, const fp_parts* parts,
                          const char* sign, size_t sign_len) {
  bool upper = (state->specifier & __STDIO_FPRINTF_FORMAT_SPECIFIER_UPPERCASE) != 0;
  const char* hex_digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  int frac_bits = parts->hex_frac_bits;
  int frac_hex_digits = frac_bits / 4;

  uint64_t lead = parts->mantissa >> frac_bits;
  uint64_t frac = parts->mantissa & ((1ull << frac_bits) - 1);
  int exp = parts->mantissa ? parts->exp2 + frac_bits : 0;

  int precision = state->precision;
  if (precision < 0) {
    // Just enough digits to show the value exactly
    precision = frac_hex_digits;
    while (precision > 0 && !((frac >> (4 * (frac_hex_digits - precision))) & 0xF)) {
      --precision;
    }
  } else if (precision < frac_hex_digits) {
    int drop = 4 * (frac_hex_digits - precision);
    uint64_t rest = frac & ((1ull << drop) - 1);
    uint64_t half = 1ull << (drop - 1);
    uint64_t kept = (lead << (4 * precision)) | (frac >> drop);
    if (rest > half || (rest == half && (kept & 1))) {
      ++kept;
    }
    lead = kept >> (4 * precision);
    frac = (kept & ((1ull << (4 * precision)) - 1)) << drop;
    if (lead > 0xF) {
      lead >>= 4;
      exp += 4;
    }
  }

  char prefix[4];
  size_t prefix_len = 0;
  for (size_t i = 0; i < sign_len; ++i) {
    prefix[prefix_len++] = sign[i];
  }
  prefix[prefix_len++] = '0';
  prefix[prefix_len++] = upper ? 'X' : 'x';

  bool point = precision > 0 || (state->flags & __STDIO_FPRINTF_FORMAT_FLAGS_USE_ALTERNATE) != 0;
  unsigned exp_abs = exp < 0 ? (unsigned)-exp : (unsigned)exp;
  size_t len = prefix_len + 1 + point + (size_t)precision + 2 + fp_count_digits(exp_abs);
  bool zero_pad = (state->flags & __STDIO_FPRINTF_FORMAT_FLAGS_LEADING_ZEROS) != 0;
  if (!fprintf_field_begin(sink, state, len, prefix, prefix_len, zero_pad) ||
      !sink_char(sink, hex_digits[lead & 0xF])) {
    return false;
  }
  if (point && !sink_char(sink, '.')) {
    return false;
  }
  for (int i = 0; i < precision; ++i) {
    char digit = i < frac_hex_digits ? hex_digits[(frac >> (4 * (frac_hex_digits - 1 - i))) & 0xF] : '0';
    if (!sink_char(sink, digit)) {
      return false;
    }
  }

  char exp_buf[16];
  size_t i = sizeof(exp_buf);
  do {
    exp_buf[--i] = (char)('0' + exp_abs % 10);
    exp_abs /= 10;
  } while (exp_abs);
  exp_buf[--i] = exp < 0 ? '-' : '+';
  exp_buf[--i] = upper ? 'P' : 'p';
  return sink_bytes(sink, &exp_buf[i], sizeof(exp_buf) - i) &&
         fprintf_field_end(sink, state, len);
}

static bool fprintf_output_float(fprintf_sink* sink, const fprintf_data* data, const fprintf_state* state) {
  int specifier = state->specifier & ~__STDIO_FPRINTF_FORMAT_SPECIFIER_UPPERCASE;
  bool upper = (state->specifier & __STDIO_FPRINTF_FORMAT_SPECIFIER_UPPERCASE) != 0;
  bool alt = (state->flags & __STDIO_FPRINTF_FORMAT_FLAGS_USE_ALTERNATE) != 0;
  fp_parts parts = fp_split(data->f, state->length == __STDIO_FPRINTF_FORMAT_LENGTH_LONG_DOUBLE);

  char sign[1];
  size_t sign_len = 0;
  char sign_char = fprintf_sign(state, parts.negative);
  if (sign_char) {
    sign[sign_len++] = sign_char;
  }

  if (parts.cls != FP_FINITE) {
    const char* text;
    if (parts.cls == FP_INF) {
      text = upper ? "INF" : "inf";
    } else {
      text = upper ? "NAN" : "nan";
    }
    size_t len = sign_len + 3;
    return fprintf_field_begin(sink, state, len, sign, sign_len, false) &&
           sink_bytes(sink, text, 3) &&
           fprintf_field_end(sink, state, len);
  }

  if (specifier == __STDIO_FPRINTF_FORMAT_SPECIFIER_HEX_FLOAT) {
    return fp_output_hex(sink, state, &parts, sign, sign_len);
  }

  fp_decimal dec;
  fp_to_decimal(parts.mantissa, parts.exp2, &dec);
  int precision = state->precision < 0 ? 6 : state->precision;

  if (specifier == __STDIO_FPRINTF_FORMAT_SPECIFIER_FLOAT) {
    if (dec.count) {
      fp_round(&dec, dec.exp10 + 1 + precision);
    }
    return fp_output_fixed(sink, state, &dec, precision, sign, sign_len);
  }
  if (specifier == __STDIO_FPRINTF_FORMAT_SPECIFIER_SCIENTIFIC) {
    fp_round(&dec, precision + 1);
    return fp_output_exponent(sink, state, &dec, precision, sign, sign_len);
  }

  // %g: precision is the number of significant digits, and the exponent after rounding
  // decides between the %f and %e styles
  if (precision == 0) {
    precision = 1;
  }
  fp_round(&dec, precision);
  int exp10 = dec.count ? dec.exp10 : 0;
  if (exp10 < precision && exp10 >= -4) {
    int frac_digits = precision - 1 - exp10;
    if (!alt) {
      // Drop trailing zeros
      int needed = dec.count - 1 - exp10;
      if (needed < frac_digits) {
        frac_digits = needed > 0 ? needed : 0;
      }
    }
    return fp_output_fixed(sink, state, &dec, frac_digits, sign, sign_len);
  }
  int frac_digits = precision - 1;
  if (!alt && dec.count - 1 < frac_digits) {
    frac_digits = dec.count > 1 ? dec.count - 1 : 0;
  }
  return fp_output_exponent(sink, state, &dec, frac_digits, sign, sign_len);
}

static void fprintf_store_count(const fprintf_data* data, int length, size_t count) {
  switch (length) {
    case __STDIO_FPRINTF_FORMAT_LENGTH_CHAR:
      *(signed char*)data->p = (signed char)count;
      break;
    case __STDIO_FPRINTF_FORMAT_LENGTH_SHORT:
      *(short*)data->p = (short)count;
      break;
    case __STDIO_FPRINTF_FORMAT_LENGTH_LONG:
      *(long*)data->p = (long)count;
      break;
    case __STDIO_FPRINTF_FORMAT_LENGTH_LONG_LONG:
      *(long long*)data->p = (long long)count;
      break;
    case __STDIO_FPRINTF_FORMAT_LENGTH_SIZE_T:
    case __STDIO_FPRINTF_FORMAT_LENGTH_PTRDIFF_T:
      *(ptrdiff_t*)data->p = (ptrdiff_t)count;
      break;
    case __STDIO_FPRINTF_FORMAT_LENGTH_INTMAX_T:
      *(intmax_t*)data->p = (intmax_t)count;
      break;
    default:
      *(int*)data->p = (int)count;
      break;
  }
}

static bool fprintf_output_formatted(fprintf_sink* sink, const fprintf_data* data, const fprintf_state* state) {
  bool wide = state->length == __STDIO_FPRINTF_FORMAT_LENGTH_LONG;
  switch (state->specifier & ~__STDIO_FPRINTF_FORMAT_SPECIFIER_UPPERCASE) {
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_SIGNED_DECIMAL:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_UNSIGNED_DECIMAL:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_OCTAL:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_HEX:
      return fprintf_output_integer(sink, data, state);
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_POINTER:
      if (!data->p) {
        return fprintf_output_string(sink, "(nil)", 5, state);
      }
      return fprintf_output_integer(sink, data, state);
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_FLOAT:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_SCIENTIFIC:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_SMART:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_HEX_FLOAT:
      return fprintf_output_float(sink, data, state);
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_CHAR: {
      if (wide && data->u >= 0x80) {
        return false;
      }
      char ch = (char)data->u;
      return fprintf_output_string(sink, &ch, 1, state);
    }
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_STRING: {
      if (wide && data->p) {
        return fprintf_output_wide_string(sink, data->p, state);
      }
      const char* str = data->p ? data->p : "(null)";
      // Never read past precision bytes, the string need not be terminated
      size_t len = 0;
      while ((state->precision < 0 || len < (size_t)state->precision) && str[len]) {
        ++len;
      }
      return fprintf_output_string(sink, str, len, state);
    }
  }
  return false;
}

bool fprintf(OutputStream out, const char *__restrict__ format, ...) {
  va_list va;
  va_start(va, format);

  fprintf_sink sink = { out, 0 };
  bool status = true;
  const char* str = format;

  while (status && *str) {
    if (*str != '%') {
      // Print everything up to the next conversion
      const char* begin = str;
      while (*str && *str != '%') {
        ++str;
      }
      status = sink_bytes(&sink, begin, str - begin);
      continue;
    }

    const char* conversion = str;
    ++str;
    if (*str == '%') {
      status = sink_char(&sink, '%');
      ++str;
      continue;
    }

    fprintf_state state;

    state.flags = get_fprintf_flags(&str);
    if (*str == '*') {
      ++str;
      state.width = va_arg(va, int);
      if (state.width < 0) {
        state.width = -state.width;
        state.flags |= __STDIO_FPRINTF_FORMAT_FLAGS_LEFT_ALIGN;
      }
    } else {
      state.width = parse_number(&str);
    }
    state.precision = __STDIO_FPRINTF_FORMAT_PRECISION_NOT_SPECIFIED;
    if (*str == '.') {
      ++str;
      if (*str == '*') {
        ++str;
        state.precision = va_arg(va, int);
        if (state.precision < 0) {
          state.precision = __STDIO_FPRINTF_FORMAT_PRECISION_NOT_SPECIFIED;
        }
      } else {
        state.precision = parse_number(&str);
      }
    }
    // The left align flag overrides the leading zeros flag
    if (state.flags & __STDIO_FPRINTF_FORMAT_FLAGS_LEFT_ALIGN) {
      state.flags &= ~__STDIO_FPRINTF_FORMAT_FLAGS_LEADING_ZEROS;
    }
    state.length = get_length_modifier(&str);
    state.specifier = get_format_specifier(&str);

    if (state.specifier == __STDIO_FPRINTF_FORMAT_SPECIFIER_DEFAULT) {
      // Unknown conversion, print it as is
      if (*str) {
        ++str;
      }
      status = sink_bytes(&sink, conversion, str - conversion);
      continue;
    }

    fprintf_data data;
    fprintf_get_data(&va, state.length, state.specifier, &data);
    if (state.specifier == __STDIO_FPRINTF_FORMAT_SPECIFIER_COUNT) {
      fprintf_store_count(&data, state.length, sink.count);
      continue;
    }
    // Output the desired data
    status = fprintf_output_formatted(&sink, &data, &state);
  }

  va_end(va);
  return status;
}

bool fputs(OutputStream out, const char* str) {
  return fput_bytes(out, str, strlen(str));
}

bool fputc(OutputStream out, char ch) {
  return fput_bytes(out, &ch, 1);
}

bool fputi(OutputStream out, int i) {
  int64_t v = (int64_t)i;
  if (out->buffer_index_ == __STDIO_NO_BUFFER) {
    char tmp[32];
    while (true) {
      uint32_t n = itoa(&v, tmp, sizeof(tmp));
      if (n == 0) {
        return false;
      }
      if (write(out->fd_, tmp, n) < 0) {
        return false;
      }
      if (v == 0) {
        return true;
      }
    }
  }

  while (true) {
    if (out->buffer_index_ == __STDIO_BUFFER_SIZE) {
      if (!fflush(out)) {
        return false;
      }
    }

    uint32_t remaining = __STDIO_BUFFER_SIZE - out->buffer_index_;
    uint32_t n = itoa(&v, &out->buffer_[out->buffer_index_], remaining);
    out->buffer_index_ += n;

    if (v == 0) {
      return true;
    }
    if (n == 0) {
      if (!fflush(out)) {
        return false;
      }
    }
  }
}

bool fflush(OutputStream file) {
  if (file->buffer_index_ == __STDIO_NO_BUFFER) {
    return false;
  }
  uint32_t bytes_to_write = file->buffer_index_;
  file->buffer_index_ = 0;
  return write(file->fd_, file->buffer_, bytes_to_write) >= 0;
}

static void flush_streams() {
  // Flush stdout
  fflush(stdout);
}

void __stdio_register_streams() {
  // Use atexit_push to make sure that all streams are flushed
  atexit_push(flush_streams);
}
