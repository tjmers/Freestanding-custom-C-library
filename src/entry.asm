bits 64
default rel

extern main
extern __heap_init
extern __stdio_register_streams
extern exit

global _start:weak

section .text


_start:
  ; When the process starts, rsp contains argc, and rsp+8 is the first pointer of argv
  ; Move argc into rdi
  mov rbp, rsp

  and rsp, ~0xF
  ; Initialize heap
  call __stdio_register_streams
  call __heap_init
  ; Move arguments into registers before calling main
  mov rdi, [rbp]
  lea rsi, [rbp + 8]
  lea rdx, [rbp + rdi * 8 + 8]

  call main

  mov rdi, rax
  call exit

  ; mov rdi, [rsp]
  ; ; Move argv[0] into rsi
  ; lea rsi, [rsp + 8]
  ; ; Move envp[0] into rdx
  ; ; Note that 8 must be added beacuse of argv's null-terminator
  ; lea rdx, [rsp + rdi * 8 + 8]
  
  ; ; Align the stack to 16 bytes
  ; ; Round down to nearest 16 bytes
  ; and rsp, ~0xF

  ; ; Save before call to __heap_init
  ; push rdi
  ; push rsi
  ; push rdx

  ; call __heap_init

  ; pop rdx
  ; pop rsi
  ; pop rdi

  ; call main
  ; mov rdi, rax
  ; call exit