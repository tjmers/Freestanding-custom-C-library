bits 64

extern main
extern __heap_init

global _start

section .text


_start:
  ; When the process starts, rsp contains argc, and rsp+8 is the first pointer of argv
  ; Move argc into rdi
  mov rdi, [rsp]
  ; Move argv[0] into rsi
  lea rsi, [rsp + 8]
  ; Move envp[0] into rdx
  ; Note that 8 must be added beacuse of argv's null-terminator
  lea rdx, [rsp + rdi * 8 + 8]
  
  ; Align the stack to 16 bytes
  ; Round down to nearest 16 bytes
  and rsp, ~0xF

  ; Save before call to __heap_init
  push rdi
  push rsi
  push rdx

  call __heap_init

  pop rdx
  pop rsi
  pop rdi

  call main

  mov rdi, rax
  ; Exit
  mov rax, 0x3c

  syscall
