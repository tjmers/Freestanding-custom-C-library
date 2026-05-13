# Freestanding custom C library

A freestanding C library designed for efficiency. Targets Linux x86_64, builds with `-nostdlib -fno-builtin`, and ships its own program entry point so binaries link against this library alone — no system libc required.

## Overview

The library provides a subset of the C standard library plus thin wrappers over Linux syscalls. A custom `_start` (in `src/entry.asm`) initializes the heap and stdio streams, forwards `argc`/`argv`/`envp` to `main`, and routes the return value through `exit` so `atexit` callbacks run.

## Building

```
make           # builds the library, the test-instrumented library, and all tests
make static    # build only build/libmylibc.a
make tests     # build test binaries under build/test/
make clean
```

Requires `gcc` and `nasm`. Test binaries are linked against an instrumented variant of the library (`-D__LIBC_TEST`) so internal helpers can be exercised from tests.

## Usage

Headers live in `intf/`. Link against `build/libmylibc.a` with `-nostdlib`:

```
gcc -nostdlib -fno-builtin -Iintf your_program.c -Lbuild -lmylibc -o your_program
```

Minimal program:

```c
#include "stdio.h"

int main(void) {
  puts("Hello from mylibc!\n");
  return 0;
}
```

## Working features

### `stdio.h`
- Buffered output streams with `stdout` and `stderr`.
- `printf` / `fprintf` with a resumable formatter that flushes through a 1 KB buffer.
- `puts`, `putc`, `fputs`, `fputc`, `fputi`, `fflush`.

### `stdlib.h`
- `malloc` / `free` backed by a free-list heap initialized at startup via `brk`.
- Integer-to-string conversions with a resumable API (the value is taken by pointer and decremented as digits are emitted, so partially filled buffers can be flushed and resumed):
  - `itoa`, `itoab`, `itoao`, `itoax` (signed; decimal, binary, octal, hex)
  - `utoa`, `utoab`, `utoao`, `utoax` (unsigned)
- `atoi` for string-to-int parsing.
- `__malloc_lock` / `__malloc_unlock` hooks for future thread safety.

### `string.h`
- `strlen`, `strcpy`, `strncpy`, `strcmp`, `strdup`.
- `strtokc` / `strtok_end` — a tokenizer variant that splits on a single character.

### `memory.h`
- Size-specialized copy/move:
  - `memcpy_small` / `memmove_small` for sub-KB copies (stack-buffered move).
  - `memcpy_large` / `memmove_large` for larger copies (heap-buffered move).
- An assembly fast path (`src/memcpy_small_fsrm.asm`) for CPUs supporting Fast Short REP MOV.

### `syscalls.h`
Direct Linux syscall wrappers (implemented in `src/syscalls.asm`):
- I/O: `write`
- Memory: `brk`, `mmap`, `munmap` (with full `PROT_*` / `MAP_*` / `MADV_*` macros in `mmap_macros.h`)
- Process: `fork`, `execve`, `getpid`, `terminate`, `abort`, `times`

### `exit.h`
- `exit(int)` runs registered callbacks then terminates.
- `atexit_push` / `atexit_pop` over a fixed stack of up to 64 callbacks.

### `assert.h`
- `assert` and `static_assert`, with `NDEBUG` honored.

### Other headers
- `stdint.h`, `stddef.h`, `stdbool.h` — fixed-width integer, size, and boolean types.
- `stdarg.h` — `va_list` / `va_start` / `va_arg` / `va_copy` via compiler builtins.
- `error.h` — `error_t` and shared error codes.
- `bytes.h` — `KB` / `MB` / `GB` / `TB` / `PB` size constants.

## Differences from the C standard library

This library intentionally diverges from the standard in several places where a different signature or behavior was a better fit for a freestanding target. Be aware of these before substituting it for a system libc.

### Signatures and return values

| Function | Standard | This library |
| --- | --- | --- |
| `strcpy` | `char *strcpy(char *dst, const char *src)` — returns `dst` | `size_t strcpy(char *dst, const char *src)` — returns bytes copied |
| `strncpy` | `char *strncpy(char *dst, const char *src, size_t n)` | `size_t strncpy(...)` — returns bytes copied |
| `strcmp` | returns `int` | returns `ptrdiff_t` |
| `printf` / `fprintf` | return `int` (chars written, negative on error) | return `bool` (success / failure) |
| `fputs` | `int fputs(const char *s, FILE *stream)` | `bool fputs(OutputStream out, const char *s)` — **stream comes first** |
| `fputc` | `int fputc(int c, FILE *stream)` | `bool fputc(OutputStream out, char c)` — **stream comes first**, no `EOF` return |
| `brk` | returns `int` (0 / -1) | returns `void*` — the new program break |
| `atexit` | `int atexit(void (*)(void))` | `bool atexit_push(callback_t)` plus `atexit_pop()`, with a fixed cap of 64 |

### Behavioral differences

- `strcpy` / `strncpy` do **not** write a terminating NUL into the destination — they copy only the non-NUL bytes from the source. Callers that need a C string must add the NUL themselves.
- `puts` does **not** append a newline (it is a thin macro over `fputs(stdout, s)`).
- `printf` / `fprintf` write through a 1 KB per-stream buffer; output is not visible until the buffer fills or `fflush` is called. The standard `_start` shim registers a flush via `atexit` so normal program exit drains `stdout`.
- `stderr` is configured as unbuffered (writes go straight to `write(2)`); `stdout` is line-bufferless but flushes on full buffer or exit.
- `abort` raises `SIGKILL`-via-`kill(getpid(), SIGABRT)` and falls back to `terminate(134)` if the signal is caught — it does not invoke any installed signal handler in a special way beyond delivering `SIGABRT`.
- `malloc` / `free` use a single free-list heap grown with `brk`; there is no `realloc`, `calloc`, or alignment guarantee beyond the natural one of the underlying allocator.
- There is no `fopen` / `fclose` / `fread` — only the preconfigured `stdout` and `stderr` streams exist. File I/O has to go through the raw `write` syscall wrapper.

### Renamed or split APIs

- `memcpy` is split into `memcpy_small` (sub-KB, stack-friendly) and `memcpy_large`. Likewise `memmove_small` / `memmove_large`. There is no single-entry `memcpy`/`memmove`.
- `strtok` is replaced by `strtokc(begin, ch)` + `strtok_end()`. It splits on a single character rather than a delimiter set, and completion is queried explicitly instead of via a `NULL` return.
- `itoa` / `utoa` are non-standard and use a **resumable** API: the value is passed by pointer and updated to the unwritten remainder, so partial buffers can be flushed and the call resumed. Base variants are suffixed: `itoab` / `itoao` / `itoax` (binary, octal, hex), and the hex form takes an `upper` flag.

### Types

- `FILE` is opaque; the public alias is `OutputStream` (`typedef struct _File FILE, *OutputStream`). It is always passed as a pointer.
- `error_t` (`uint32_t`) and a small set of `ERROR_*` codes live in `error.h`; library functions that don't return `bool` do not currently use it as an out-parameter, but it is reserved for that purpose.

## Project layout

```
intf/            public headers
src/             library implementation (C + NASM)
  string/        string.h implementations
test/            test programs (one per concern)
build/           library and test build artifacts (gitignored)
build_test/      test-instrumented build of the library
```

## Tests

The `test/` directory contains standalone programs covering output, printf formatting, malloc/free round trips, string routines, conversions, and varargs. Each test links against `libmylibc_test.a` and is built into `build/test/<name>`.

## Needs Improvement

The following sections are currently wildly inefficient. They were implemented to work, but need to be improved in terms of efficiency.

- Vectorization functions (important - hot path)
  - memcpy
  - strcpy
  - strlen
- Printf isn't very efficient but it shouldn't be a hot path.
