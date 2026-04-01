bits 64
default rel

global memcpy_small_fsrm

section .text

memcpy_small_fsrm:
  ; rdi: destination index
  ; rsi: source index
  ; rdx: number of bytes to copy

  ; Just call rep movsb since this has fsrm and enhansed rep movsb
  

  mov rax, rdi         ; return original dst (memcpy semantics)
  mov rcx, rdx
  cld                  ; clear direction flag


  rep movsb
  ret 
