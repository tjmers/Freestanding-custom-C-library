section .note.GNU-stack noalloc noexec nowrite progbits
bits 64
default rel

global strlen

section .text

strlen:

  ; rdi - string to find the length of
  ; return value will return from rax

  ; Let ymm1 be 0 for the duration of this function
  vpxor ymm1, ymm1, ymm1
  



  ; Step 1: Align
  push rbx

  ; Round rdi down to the nearest 32 bytes
  mov rsi, rdi
  and rsi, ~0b11111

  ; Load into ymm0
  vmovdqa ymm0, [rsi]
  
  ; Compare
  vpcmpeqb ymm0, ymm0, ymm1

  ; Move back to scalar world
  vpmovmskb ebx, ymm0

  ; Count trailing zeros
  tzcnt ecx, ebx

  
  ; If the number of trailing zeros is greater than or equal to the number of bytes that were compared ( 32 - (rdi & rsi) ) then return early
  test rcx, rcx
  jnz .alignment_return
  mov rax, rcx


  ; Step 2: Loop over full chunks
.loop_start:

  add rsi, 32
  ; At this point, rsi is the aligned address that we need to work with
  vmovdqa ymm0, [rsi]

  ; Compare with the zeros
  vpcmpeqb ymm0, ymm0, ymm1

  vpmovmskb ebx, ymm0

  tzcnt ecx, ebx

  test rcx, rcx
  ; If this isn't zero, we are done
  add rax, 32
  jz .loop_start

  ; Subtract the trailing zeros
  sub rax, ecx

  pop rbx

  ret

.alignment_return:
  ; rsi & rdi is the amount that was taken away when aligned
  and rsi, rdi
  mov r8, 32
  sub r8, rsi

  
  cmp ecx, r8
  mov rax, ecx
  cmovl rax, 0
  pop rbx
  ret