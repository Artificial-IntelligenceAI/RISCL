/* RISCL compiler: shared declarations. No system or libc headers anywhere. */
#ifndef RISCL_H
#define RISCL_H

typedef unsigned char u8;
typedef unsigned int u32;
typedef int i32;
typedef unsigned long u64;
typedef long i64;

#define NULL ((void *)0)

/* start.S */
long sys(long n, long a, long b, long c, long d, long e, long f);
void *memcpy(void *dst, const void *src, u64 n);
void *memset(void *dst, int c, u64 n);

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_MMAP 9
#define SYS_MUNMAP 11
#define SYS_FCHMOD 91
#define SYS_EXIT_GROUP 231

/* util.c */
typedef struct {
    u8 *p;
    u64 len, cap;
} Buf;

__attribute__((noreturn)) void die(void);
void *os_alloc(u64 n);
void os_free(void *p, u64 n);
void buf_reserve(Buf *b, u64 extra);
void buf_put(Buf *b, const void *src, u64 n);
int read_file(const char *path, Buf *out);
int write_file(const char *path, const u8 *data, u64 n);

/* lex.c */
enum {
    T_EOF, T_BAD, T_REG, T_NUM, T_LABEL,
    T_ASSIGN,                                   /* = */
    T_ADD, T_SUB, T_MUL, T_DIV, T_MOD,          /* + - * / % */
    T_AND, T_OR, T_XOR, T_SHL, T_SHR,           /* & | ^ << >> */
    T_LT, T_LE, T_EQ, T_NE, T_GT, T_GE,         /* < <= == != > >= */
    T_QUEST, T_COLON, T_SEMI, T_LBRACK, T_RBRACK, /* ? : ; [ ] */
    T_BANG, T_BANGQ, T_HASH                     /* ! !' # */
};

typedef struct {
    u32 kind;
    u64 val; /* register number 0-7, number, or label name */
} Token;

/* Tokenizes src; the result is followed by at least TOKEN_PAD T_EOF tokens. */
#define TOKEN_PAD 8
Token *lex(const u8 *src, u64 n, u64 *count);

/* x86.c: the machine-code emitter */
enum { RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15 };

/* Condition codes (the low nibble of jcc). */
enum { CC_B = 0x2, CC_E = 0x4, CC_NE = 0x5, CC_A = 0x7, CC_S = 0x8, CC_NS = 0x9,
       CC_L = 0xC, CC_GE = 0xD, CC_LE = 0xE, CC_G = 0xF };

/* ALU operations by their /digit in opcode 0x81. */
enum { ALU_ADD = 0, ALU_OR = 1, ALU_AND = 4, ALU_SUB = 5, ALU_XOR = 6, ALU_CMP = 7 };

/* Where a rel32 points: a RISCL label, a runtime-internal label, or address 0. */
enum { FIX_USER, FIX_LOCAL, FIX_NULL };

typedef struct {
    u64 at;   /* offset of the rel32 field in code */
    u64 key;  /* label name or local label index */
    u32 kind;
} Fixup;

extern Buf code;
extern Buf fixups;
extern u64 local_label[];

u64 here(void);
void emit8(u8 x);
void emit32(u32 x);
void emit64(u64 x);
void x_mov_rr(int dst, int src);
void x_mov_ri(int dst, u64 imm);
void x_alu_rr(int op, int dst, int src);
void x_alu_ri(int op, int dst, i64 imm);
void x_imul_rr(int dst, int src);
void x_imul_rri(int dst, int src, i64 imm);
void x_shift_cl(int ext, int r);
void x_shift_i(int ext, int r, int n);
void x_cqo(void);
void x_f7(int ext, int r);
void x_load_mem(int dst);
void x_store_mem(int src);
void x_store_mem_imm(i64 imm);
void x_jcc(int cc, u32 kind, u64 key);
void x_jmp(u32 kind, u64 key);
void x_call(u32 kind, u64 key);
void x_ret(void);
void x_ud2(void);
void x_syscall(void);
void x_push(int r);
void x_pop(int r);
int fits_s32(i64 v);

/* Runtime routines, emitted once at the start of every program. */
enum { RT_PRINT_DEC, RT_PRINT_CHAR, RT_READ_NUM, RT_GETC, RT_NLABELS = 32 };
#define MEM_SIZE 0x100000UL
#define RT_POS (MEM_SIZE)        /* input buffer: read position */
#define RT_LEN (MEM_SIZE + 8)    /* input buffer: bytes held */
#define RT_BUF (MEM_SIZE + 16)   /* input buffer: 4096 bytes */
#define RT_BUF_SIZE 4096
#define DATA_SIZE (RT_BUF + RT_BUF_SIZE)
void emit_runtime(void);

/* gen.c */
#define CODE_VADDR_BASE 0x400000UL
#define HEADER_SIZE (64 + 3 * 56)
#define CODE_VADDR (CODE_VADDR_BASE + HEADER_SIZE)
/* Compiles the tokens to code; returns the entry offset and where the data address goes. */
void compile(Token *t, u64 *entry, u64 *membase_at);
void resolve(void);

/* elf.c */
void write_elf(Buf *out, u64 entry_off, u64 membase_at);

#endif
