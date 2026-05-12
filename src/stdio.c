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


static int parse_number(const char **fmt) {
    int value = 0;

    while (**fmt >= '0' && **fmt <= '9') {
        value = value * 10 + (**fmt - '0');
        (*fmt)++;
    }

    return value;
}

static int get_fprintf_flags(char** str) {
  int flags = __STDIO_FPRINTF_FORMAT_FLAGS_DEFAULT;
  while (1) {
    switch (**str) {
      case '-':
        flags |= __STDIO_FPRINTF_FORMAT_FLAGS_LEFT_ALIGN;
        break;
      case '+':
        flags |= __STDIO_FPRINTF_FORMAT_FLAGS_SHOW_SIGN;
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
  }
  __builtin_unreachable();
}

static int get_length_modifier(char** str) {
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

static int get_format_specifier(char** str) {
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
    case 'c':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_CHAR;
      break;
    case 's':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_STRING;
      break;
    case 'p':
      fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_POINTER;
      break;
  }
  if (**str >= 'A' && **str <= 'Z') {
    fmt |= __STDIO_FPRINTF_FORMAT_SPECIFIER_UPPERCASE;
  }
  ++(*str);
  return fmt;
}

// This function is really ugly because of default argument promotion and different sizes on different architectures
static uint64_t fprintf_get_data(va_list args, int length, int specifier) {
  uint64_t res = 0;
  switch (specifier) {
    // All pointers
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_STRING:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_POINTER:
      *((char**)&res) = va_arg(args, char*);
      break;
    // All signed integers
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_SIGNED_DECIMAL:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_SCIENTIFIC:
    case (__STDIO_FPRINTF_FORMAT_SPECIFIER_SCIENTIFIC | __STDIO_FPRINTF_FORMAT_SPECIFIER_UPPERCASE):
      switch (length) {
        case __STDIO_FPRINTF_FORMAT_LENGTH_CHAR:
#if MY_SIZEOF_CHAR == MY_SIZEOF_INT && CHAR_MIN == 0
          *((char*)&res) = (char)va_arg(args, unsigned int);
#else
          *((char*)&res) = (char)va_arg(args, int);
#endif
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_SHORT:
          *((short*)&res) = (short)va_arg(args, int);
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_LONG:
          *((long*)&res) = va_arg(args, long);
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_LONG_LONG:
          *((long long*)&res) = va_arg(args, long long);
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_SIZE_T:
#if MY_SIZEOF_SIZE_T < MY_SIZEOF_INT 
          *((size_t*)&res) = (size_t)va_arg(args, unsigned int);
#else
          *((size_t*)&res) = va_arg(args, size_t);
#endif
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_PTRDIFF_T:
#if MY_SIZEOF_PTRDIFF_T < MY_SIZEOF_INT
          *((ptrdiff_t*)&res) = (ptrdiff_t)va_arg(args, int);
#else
          *((ptrdiff_t*)&res) = va_arg(args, ptrdiff_t);
#endif
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_INTMAX_T:
          *((intmax_t*)&res) = va_arg(args, intmax_t);
          break;
        default:
          *((int*)&res) = va_arg(args, int);
          break;
      } 
      break;
    // All unsigned integers
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_OCTAL:
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_HEX:
    case (__STDIO_FPRINTF_FORMAT_SPECIFIER_HEX | __STDIO_FPRINTF_FORMAT_SPECIFIER_UPPERCASE):
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_UNSIGNED_DECIMAL:
      switch (length) {
        case __STDIO_FPRINTF_FORMAT_LENGTH_CHAR:
#if MY_SIZEOF_CHAR == MY_SIZEOF_INT // Unsigned char gets promoted to unsigned int
          *((unsigned char*)&res) = (unsigned char)va_arg(args, unsigned int);
#else
          *((unsigned char*)&res) = (unsigned char)va_arg(args, int);
#endif
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_SHORT:
#if MY_SIZEOF_SHORT == MY_SIZEOF_INT // Gets promoted to unsigned int
          *((unsigned short*)&res) = (unsigned short)va_arg(args, unsigned int);
#else
          *((unsigned short*)&res) = (unsigned short)va_arg(args, int);
#endif
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_LONG:
          *((unsigned long*)&res) = va_arg(args, unsigned long);
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_LONG_LONG:
          *((unsigned long long*)&res) = va_arg(args, unsigned long long);
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_SIZE_T:
#if MY_SIZEOF_SIZE_T < MY_SIZEOF_INT // Gets promoted to int
          *((size_t*)&res) = (size_t)va_arg(args, int);
#elif MY_SIZEOF_SIZE_T == MY_SIZEOF_INT // Gets promoted to unsigned int
          *((size_t*)&res) = (size_t)va_args(args, unsigned int);
#else
          *((size_t*)&res) = va_arg(args, size_t);
#endif
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_PTRDIFF_T:
#if MY_SIZEOF_PTRDIFF_T < MY_SIZEOF_INT
          *((uptrdiff_t*)&res) = (uptrdiff_t)va_arg(args, int);
#elif MY_SIZEOF_PTRDIFF_T == MY_SIZEOF_INT // Gets promoted to unsigned int
          *((uptrdiff_t*)&res) = (uptrdiff_t)va_args(args, unsigned int);
#else
          *((uptrdiff_t*)&res) = va_arg(args, uptrdiff_t);
#endif
          break;
        case __STDIO_FPRINTF_FORMAT_LENGTH_INTMAX_T:
          *((uintmax_t*)&res) = va_arg(args, uintmax_t);
          break;
        default:
          *((unsigned*)&res) = va_arg(args, unsigned);
          break;
      } 
      break;
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_CHAR:
#if MY_SIZEOF_CHAR == MY_SIZEOF_INT && CHAR_MIN == 0
      *((char*)&res) = (char)va_arg(args, unsigned int);
#else
      *((char*)&res) = (char)va_arg(args, int);
#endif
      break;
    case __STDIO_FPRINTF_FORMAT_SPECIFIER_FLOAT:
      if (length == __STDIO_FPRINTF_FORMAT_LENGTH_LONG_DOUBLE) {
        *((long double*)&res) = va_arg(args, long double);
      } else {
        *((double*)&res) = va_arg(args, double);
      }
      break;
    default:
      fputs(stderr, "Something went wrong with fprintf.\n");
      break;
  }
  return res;
}

bool fprintf_output_formatted(OutputStream out, uint64_t data, fprintf_state state) {
  
}


bool fprintf(OutputStream out, const char *__restrict__ format, ...) {
  char* cpy = strdup(format);
  if (!cpy) return false;
  va_list va;
  va_start(va, format);

  char* str = strtokc(cpy, '%');
  fputs(out, str);

  while (!strtok_end()) {
    str = strtokc(NULL, '%');
    // Go past the percent
    // Process the format specifier
    ++str;
    if (*str == '%') {
      // Just print the string before continuing
      if (!fputs(out, str)) {
        free(cpy);
        return false;
      }
      continue;
    }

    fprintf_state state;

    state.flags = get_fprintf_flags(&str);
    state.width = __STDIO_FPRINTF_FORMAT_WIDTH_NOT_SPECIFIED;
    if (*str == '*') {
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
      if (str == '*') {
        state.precision = va_arg(va, int);
        if (state.precision < 0) {
          state.precision = __STDIO_FPRINTF_FORMAT_PRECISION_NOT_SPECIFIED;
        }
      } else {
        state.precision = parse_number(&str);
      }
    }
    state.length = get_length_modifier(&str);
    state.specifier = get_format_specifier(&str);

    uint64_t num = fprintf_get_data(va, state.length, state.specifier);
    // Output the desired data
    if (!fprintf_output_formatted(out, num, state)) {
      free(cpy);
      return false;
    }

    // Print the rest of the string
    if (!fputs(out, str)) {
      free(cpy);
      return false;
    }

  }

  free(cpy);
  return true;
}

bool fputs(OutputStream out, const char* str) {
  if (out->buffer_index_ == __STDIO_NO_BUFFER) {
    return write(out->fd_, str, strlen(str)) >= 0;
  }
  bool status = true;
  while (status && *str) {
    if (out->buffer_index_ == __STDIO_BUFFER_SIZE) {
      status = fflush(out);
    }
    size_t written = strncpy(&out->buffer_[out->buffer_index_], str, __STDIO_BUFFER_SIZE - out->buffer_index_);
    str += written;
    out->buffer_index_ += written;
  }
  return status;
}

bool fputc(OutputStream out, char ch) {
  bool status = true;
  if (out->buffer_index_ == __STDIO_BUFFER_SIZE) {
    // Flush the stream
    status = fflush(out);
  }
  out->buffer_[out->buffer_index_++] = ch;
  return status;
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