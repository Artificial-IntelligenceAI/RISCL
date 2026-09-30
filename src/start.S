/* RISCL compiler: process entry, system calls and the memory routines Clang may call. */
    .text
    .globl _start
_start:
    xor %ebp, %ebp
    mov (%rsp), %rdi            /* argc */
    lea 8(%rsp), %rsi           /* argv */
    and $-16, %rsp
    call main
    mov %eax, %edi
    mov $231, %eax              /* exit_group */
    syscall
    hlt

/* long sys(long n, long a, long b, long c, long d, long e, long f) */
    .globl sys
sys:
    mov %rdi, %rax
    mov %rsi, %rdi
    mov %rdx, %rsi
    mov %rcx, %rdx
    mov %r8, %r10
    mov %r9, %r8
    mov 8(%rsp), %r9
    syscall
    ret

/* void *memcpy(void *dst, const void *src, u64 n) */
    .globl memcpy
memcpy:
    mov %rdi, %rax
    mov %rdx, %rcx
    rep movsb
    ret

/* void *memmove(void *dst, const void *src, u64 n) */
    .globl memmove
memmove:
    mov %rdi, %rax
    mov %rdx, %rcx
    cmp %rsi, %rdi
    jbe 1f
    lea -1(%rsi,%rcx), %rsi
    lea -1(%rdi,%rcx), %rdi
    std
    rep movsb
    cld
    ret
1:  rep movsb
    ret

/* void *memset(void *dst, int c, u64 n) */
    .globl memset
memset:
    mov %rdi, %r8
    mov %esi, %eax
    mov %rdx, %rcx
    rep stosb
    mov %r8, %rax
    ret

    .section .note.GNU-stack,"",@progbits
