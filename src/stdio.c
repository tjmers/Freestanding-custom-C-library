#include "../intf/stdio.h"

#include "../intf/exit.h"
#include "../intf/stdint.h"
#include "../intf/stdarg.h"
#include "../intf/string.h"
#include "../intf/syscalls.h"

#define __STDIO_NO_BUFFER UINT32_MAX

static FILE stdout_ = { 1, 0 };
static FILE stderr_ = { 2, __STDIO_NO_BUFFER };


OutputStream stdout = &stdout_;
OutputStream stderr = &stderr_;

bool fprintf(OutputStream out, const char *__restrict__ format, ...) {
  va_list va;
  va_start(va, format);

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

bool fflush(OutputStream file) {
  if (file->buffer_index_ == __STDIO_NO_BUFFER, 0) {
    return false;
  }
  uint32_t bytes_to_write = file->buffer_index_;
  file->buffer_index_ = 0;
  return write(file->fd_, file->buffer_, bytes_to_write) >= 0;
}

bool printf(const char *__restrict__ format, ...) {
  va_list va;
  va_start(va, format);

}


static void flush_streams() {
  // Flush stdout
  fflush(stdout);
}

void __stdio_register_streams() {
  // Use atexit_push to make sure that all streams are flushed
  atexit_push(flush_streams); 
}