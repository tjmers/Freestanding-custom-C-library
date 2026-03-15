bits 64
default rel

global memcpy_small

section .text

memcpy_small:
  ; rdi: destination index
  ; rsi: source index
  ; rdx: number of bytes to copy

  ; Just call rep movsb since this has fsrm and enhansed rep movsb
  

  mov rcx, rdx
  xor df, df


  rep movsb
  ; Return the number of bytes copied
  mov rax, rdx
  sub rax, rcx

  ret 
