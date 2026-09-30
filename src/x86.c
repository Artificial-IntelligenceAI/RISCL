/* RISCL compiler: an x86-64 machine-code emitter, just the instructions RISCL needs, and the
   runtime routines every compiled program carries. */
#include "riscl.h"

Buf code;
Buf fixups;
u64 local_label[RT_NLABELS];

u64 here(void) { return code.len; }
void emit8(u8 x) { buf_put(&code, &x, 1); }
void emit32(u32 x) { buf_put(&code, &x, 4); }
void emit64(u64 x) { buf_put(&code, &x, 8); }

int fits_s32(i64 v) { return v >= -2147483648L && v <= 2147483647L; }
static int fits_s8(i64 v) { return v >= -128 && v <= 127; }

/* REX prefix for a register-direct ModRM; left out when it would be a plain 0x40. */
static void rex(int w, int reg, int rm)
{
    u8 r = 0x40 | (w << 3) | ((reg >> 3) << 2) | (rm >> 3);
    if (r != 0x40)
        emit8(r);
}
static void modrm(int mod, int reg, int rm) { emit8((u8)(mod << 6 | (reg & 7) << 3 | (rm & 7))); }

void x_mov_rr(int dst, int src)
{
    if (dst == src)
        return;
    rex(1, src, dst);
    emit8(0x89);
    modrm(3, src, dst);
}

void x_mov_ri(int dst, u64 imm)
{
    if (imm == 0) {                      /* xor r32, r32 */
        rex(0, dst, dst);
        emit8(0x31);
        modrm(3, dst, dst);
    } else if (imm <= 0xffffffffUL) {    /* mov r32, imm32 (zero-extends) */
        rex(0, 0, dst);
        emit8(0xB8 + (dst & 7));
        emit32((u32)imm);
    } else if (fits_s32((i64)imm)) {     /* mov r64, simm32 */
        rex(1, 0, dst);
        emit8(0xC7);
        modrm(3, 0, dst);
        emit32((u32)imm);
    } else {                             /* movabs r64, imm64 */
        rex(1, 0, dst);
        emit8(0xB8 + (dst & 7));
        emit64(imm);
    }
}

static const u8 alu_rr_opcode[8] = { 0x01, 0x09, 0, 0, 0x21, 0x29, 0x31, 0x39 };

void x_alu_rr(int op, int dst, int src)
{
    rex(1, src, dst);
    emit8(alu_rr_opcode[op]);
    modrm(3, src, dst);
}

void x_alu_ri(int op, int dst, i64 imm)
{
    rex(1, 0, dst);
    if (fits_s8(imm)) {
        emit8(0x83);
        modrm(3, op, dst);
        emit8((u8)imm);
    } else {
        emit8(0x81);
        modrm(3, op, dst);
        emit32((u32)imm);
    }
}

/* 32-bit ALU op with an 8-bit immediate (runtime only). */
static void x_alu32_ri8(int op, int dst, int imm)
{
    rex(0, 0, dst);
    emit8(0x83);
    modrm(3, op, dst);
    emit8((u8)imm);
}

void x_imul_rr(int dst, int src)
{
    rex(1, dst, src);
    emit8(0x0F);
    emit8(0xAF);
    modrm(3, dst, src);
}

void x_imul_rri(int dst, int src, i64 imm)
{
    rex(1, dst, src);
    if (fits_s8(imm)) {
        emit8(0x6B);
        modrm(3, dst, src);
        emit8((u8)imm);
    } else {
        emit8(0x69);
        modrm(3, dst, src);
        emit32((u32)imm);
    }
}

/* ext: 4 = shl, 7 = sar */
void x_shift_cl(int ext, int r)
{
    rex(1, 0, r);
    emit8(0xD3);
    modrm(3, ext, r);
}

void x_shift_i(int ext, int r, int n)
{
    rex(1, 0, r);
    emit8(0xC1);
    modrm(3, ext, r);
    emit8((u8)n);
}

void x_cqo(void) { emit8(0x48); emit8(0x99); }

/* ext: 3 = neg, 6 = div, 7 = idiv */
void x_f7(int ext, int r)
{
    rex(1, 0, r);
    emit8(0xF7);
    modrm(3, ext, r);
}

/* mov dst, [rbx + rax] and mov [rbx + rax], src: RISCL memory, rbx its base, rax the address. */
void x_load_mem(int dst)
{
    rex(1, dst, 0);
    emit8(0x8B);
    modrm(0, dst, 4);
    emit8(0x03);                         /* SIB: base rbx, index rax, scale 1 */
}

void x_store_mem(int src)
{
    rex(1, src, 0);
    emit8(0x89);
    modrm(0, src, 4);
    emit8(0x03);
}

void x_store_mem_imm(i64 imm)
{
    emit8(0x48);
    emit8(0xC7);
    modrm(0, 0, 4);
    emit8(0x03);
    emit32((u32)imm);
}

static void add_fixup(u32 kind, u64 key)
{
    Fixup f = { here(), key, kind };
    buf_put(&fixups, &f, sizeof f);
    emit32(0);
}

void x_jcc(int cc, u32 kind, u64 key) { emit8(0x0F); emit8(0x80 | cc); add_fixup(kind, key); }
void x_jmp(u32 kind, u64 key) { emit8(0xE9); add_fixup(kind, key); }
void x_call(u32 kind, u64 key) { emit8(0xE8); add_fixup(kind, key); }
void x_ret(void) { emit8(0xC3); }
void x_ud2(void) { emit8(0x0F); emit8(0x0B); }
void x_syscall(void) { emit8(0x0F); emit8(0x05); }

void x_push(int r)
{
    rex(0, 0, r);
    emit8(0x50 + (r & 7));
}

void x_pop(int r)
{
    rex(0, 0, r);
    emit8(0x58 + (r & 7));
}

/* op reg, [rbx + disp32] */
static void x_rbx_disp(u8 opcode, int w, int reg, u32 disp)
{
    rex(w, reg, RBX);
    emit8(opcode);
    modrm(2, reg, RBX);
    emit32(disp);
}

/* op reg, [rsp + disp8] */
static void x_rsp_disp8(u8 opcode, int reg, int disp)
{
    rex(1, reg, RSP);
    emit8(opcode);
    modrm(1, reg, 4);
    emit8(0x24);                         /* SIB: base rsp, no index */
    emit8((u8)disp);
}

static void local(int id) { local_label[id] = here(); }

/* Local labels inside the runtime, after the entry points. */
enum { L_PD_POS = 8, L_PD_LOOP, L_PD_WRITE, L_GC_HAVE, L_GC_EOF,
       L_RN_SKIP, L_RN_LOOP, L_RN_DONE, L_RN_RET };

/* Registers the runtime may use freely: rax rcx rdx rsi rdi. Everything else it keeps, including
   r11 ($d), which the syscall instruction clobbers. */
void emit_runtime(void)
{
    /* print_dec: write rax as a signed decimal number. */
    local(RT_PRINT_DEC);
    x_push(R11);
    x_alu_ri(ALU_SUB, RSP, 32);
    x_rsp_disp8(0x8D, RSI, 32);          /* lea rsi, [rsp+32]: end of the digits */
    x_mov_ri(RDI, 0);                    /* rdi: negative? */
    rex(1, RAX, RAX); emit8(0x85); modrm(3, RAX, RAX); /* test rax, rax */
    x_jcc(CC_NS, FIX_LOCAL, L_PD_POS);
    x_f7(3, RAX);                        /* neg rax; the smallest number stays, read unsigned */
    x_mov_ri(RDI, 1);
    local(L_PD_POS);
    x_mov_ri(RCX, 10);
    local(L_PD_LOOP);
    x_mov_ri(RDX, 0);
    x_f7(6, RCX);                        /* div rcx */
    emit8(0x80); modrm(3, 0, RDX); emit8('0'); /* add dl, '0' */
    rex(1, 0, RSI); emit8(0xFF); modrm(3, 1, RSI); /* dec rsi */
    emit8(0x88); modrm(0, RDX, RSI);     /* mov [rsi], dl */
    rex(1, RAX, RAX); emit8(0x85); modrm(3, RAX, RAX);
    x_jcc(CC_NE, FIX_LOCAL, L_PD_LOOP);
    emit8(0x85); modrm(3, RDI, RDI);     /* test edi, edi */
    x_jcc(CC_E, FIX_LOCAL, L_PD_WRITE);
    rex(1, 0, RSI); emit8(0xFF); modrm(3, 1, RSI);
    emit8(0xC6); modrm(0, 0, RSI); emit8('-'); /* mov byte [rsi], '-' */
    local(L_PD_WRITE);
    x_rsp_disp8(0x8D, RDX, 32);          /* lea rdx, [rsp+32] */
    x_alu_rr(ALU_SUB, RDX, RSI);         /* rdx: length */
    x_mov_ri(RDI, 1);
    x_mov_ri(RAX, SYS_WRITE);
    x_syscall();
    x_alu_ri(ALU_ADD, RSP, 32);
    x_pop(R11);
    x_ret();

    /* print_char: write the low byte of rax. */
    local(RT_PRINT_CHAR);
    x_push(R11);
    x_push(RAX);
    x_mov_rr(RSI, RSP);
    x_mov_ri(RDX, 1);
    x_mov_ri(RDI, 1);
    x_mov_ri(RAX, SYS_WRITE);
    x_syscall();
    x_pop(RAX);
    x_pop(R11);
    x_ret();

    /* getc: the next byte of standard input in eax, or -1 at its end. Clobbers rax and rcx. */
    local(RT_GETC);
    x_rbx_disp(0x8B, 1, RAX, RT_POS);    /* mov rax, [pos] */
    x_rbx_disp(0x3B, 1, RAX, RT_LEN);    /* cmp rax, [len] */
    x_jcc(CC_B, FIX_LOCAL, L_GC_HAVE);
    x_push(RDX); x_push(RSI); x_push(RDI); x_push(R11);
    x_mov_ri(RDI, 0);
    x_rbx_disp(0x8D, 1, RSI, RT_BUF);    /* lea rsi, [buf] */
    x_mov_ri(RDX, RT_BUF_SIZE);
    x_mov_ri(RAX, SYS_READ);
    x_syscall();
    x_pop(R11); x_pop(RDI); x_pop(RSI); x_pop(RDX);
    rex(1, RAX, RAX); emit8(0x85); modrm(3, RAX, RAX);
    x_jcc(CC_LE, FIX_LOCAL, L_GC_EOF);
    x_rbx_disp(0x89, 1, RAX, RT_LEN);    /* mov [len], rax */
    x_mov_ri(RAX, 0);
    local(L_GC_HAVE);
    emit8(0x0F); emit8(0xB6); modrm(2, RCX, 4); emit8(0x03); emit32(RT_BUF); /* movzx ecx, byte [rbx+rax+buf] */
    rex(1, 0, RAX); emit8(0xFF); modrm(3, 0, RAX); /* inc rax */
    x_rbx_disp(0x89, 1, RAX, RT_POS);    /* mov [pos], rax */
    emit8(0x89); modrm(3, RCX, RAX);     /* mov eax, ecx */
    x_ret();
    local(L_GC_EOF);
    emit8(0xB8); emit32(0xffffffff);     /* mov eax, -1 */
    x_ret();

    /* read_num: skip spaces, tabs and newlines, then an optional '-' and decimal digits; the
       first other byte is consumed. The number, or 0, in rax. */
    local(RT_READ_NUM);
    x_mov_ri(RDX, 0);                    /* rdx: the number so far */
    x_mov_ri(RSI, 0);                    /* rsi: negative? */
    local(L_RN_SKIP);
    x_call(FIX_LOCAL, RT_GETC);
    x_alu32_ri8(ALU_CMP, RAX, ' ');
    x_jcc(CC_E, FIX_LOCAL, L_RN_SKIP);
    x_alu32_ri8(ALU_CMP, RAX, '\t');
    x_jcc(CC_E, FIX_LOCAL, L_RN_SKIP);
    x_alu32_ri8(ALU_CMP, RAX, '\n');
    x_jcc(CC_E, FIX_LOCAL, L_RN_SKIP);
    x_alu32_ri8(ALU_CMP, RAX, '-');
    x_jcc(CC_NE, FIX_LOCAL, L_RN_LOOP);
    x_mov_ri(RSI, 1);
    x_call(FIX_LOCAL, RT_GETC);
    local(L_RN_LOOP);
    x_alu32_ri8(ALU_SUB, RAX, '0');
    x_alu32_ri8(ALU_CMP, RAX, 9);
    x_jcc(CC_A, FIX_LOCAL, L_RN_DONE);   /* not a digit, or the end: -1 - '0' is huge unsigned */
    x_imul_rri(RDX, RDX, 10);
    x_alu_rr(ALU_ADD, RDX, RAX);
    x_call(FIX_LOCAL, RT_GETC);
    x_jmp(FIX_LOCAL, L_RN_LOOP);
    local(L_RN_DONE);
    x_mov_rr(RAX, RDX);
    emit8(0x85); modrm(3, RSI, RSI);     /* test esi, esi */
    x_jcc(CC_E, FIX_LOCAL, L_RN_RET);
    x_f7(3, RAX);
    local(L_RN_RET);
    x_ret();
}
