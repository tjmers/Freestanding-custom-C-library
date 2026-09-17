section .note.GNU-stack noalloc noexec nowrite progbits
bits 64
default rel

extern main
extern __heap_init
extern __stdio_register_streams
extern enable_fsrm
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
  ; Check if the FSRM memcpy should be used
  
  ; Make sure that leaf 7 exists
  jmp .call_main
  xor eax, eax
  cpuid
  cmp eax, 0x7
  jl .call_main

  ; Check edx bit 4
  mov eax, 0x7
  xor ecx, ecx
  cpuid
  test edx, 0x10
  jz .call_main
  call enable_fsrm

.call_main:
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