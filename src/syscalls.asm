section .note.GNU-stack noalloc noexec nowrite progbits
bits 64

global write
global brk
global mmap
global munmap
global terminate
global abort
global $times


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

abort:
  ; Before `call`, RSP must be 16-byte aligned (SysV AMD64). At our entry,
  ; RSP%16==8 from the caller's `call abort`, so subtract 8 once.
  sub rsp, 8
  call getpid

  mov rdi, rax
  mov rsi, 6 ; SIGABRT
  mov rax, 0x3E ; __NR_kill
  syscall

  ; If kill returns (e.g. SIGABRT handler returns), restore the slot we
  ; reserved for alignment and exit abnormally. RDI may still be the PID
  ; from kill's args — do not fall through without setting the exit code.
  add rsp, 8
  mov rdi, 134 ; 128 + 6, conventional status for "aborted" via signal 6
  jmp terminate

terminate:
  mov rax, 0x3C ; __NR_exit
  syscall
  ; exit does not return; if it ever did, avoid falling through into fork.
  ud2

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

$times:
  mov rax, 0x64
  syscall
  ret