bits 64

global write
global brk
global mmap
global munmap
global exit


section .text

write:
  mov rax, 0x1
  syscall
  ret

brk:
  mov rax, 0xc
  syscall
  ; This version of brk just returns the pointer, not 0 or 1
  ret

mmap:
  mov rax, 0x9
  mov r10, rcx
  syscall
  ret

munmap:
  mov rax, 0x0b
  syscall
  ret

exit:
  mov rax, 0x3c
  syscall
  ret

fork:
  mov rax, 0x39
  syscall
  ret

execve:
  mov rax, 0x3b
  syscall
  ret

getpid:
  mov rax, 0x27
  syscall
  ret