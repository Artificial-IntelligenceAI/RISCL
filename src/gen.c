/* RISCL compiler: the parser and code generator, in one pass over the tokens. Each statement
   either matches one of RISCL's forms exactly or is a bad statement, which becomes ud2. */
#include "riscl.h"

/* ---- Labels: an open-addressing hash table from label name to code offset. ---- */

typedef struct {
    u64 name;
    u64 offset;
    u64 used;
} Slot;

static Slot *table;
static u64 table_cap, table_count;

static u64 hash(u64 x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdUL;
    x ^= x >> 33;
    return x;
}

static Slot *find(u64 name)
{
    u64 i = hash(name) & (table_cap - 1);
    while (table[i].used && table[i].name != name)
        i = (i + 1) & (table_cap - 1);
    return &table[i];
}

/* Defines a label here; returns 0 if it already exists. */
static int define_label(u64 name, u64 offset)
{
    if ((table_count + 1) * 2 > table_cap) {
        Slot *old = table;
        u64 old_cap = table_cap;
        table_cap = table_cap ? table_cap * 2 : 1024;
        table = os_alloc(table_cap * sizeof(Slot));
        for (u64 i = 0; i < old_cap; i++)
            if (old[i].used)
                *find(old[i].name) = old[i];
        os_free(old, old_cap * sizeof(Slot));
    }
    Slot *s = find(name);
    if (s->used)
        return 0;
    s->used = 1;
    s->name = name;
    s->offset = offset;
    table_count++;
    return 1;
}

static int lookup_label(u64 name, u64 *offset)
{
    if (!table_cap)
        return 0;
    Slot *s = find(name);
    if (!s->used)
        return 0;
    *offset = s->offset;
    return 1;
}

/* ---- Code generation ---- */

static int reg_of(Token *t) { return R8 + (int)t->val; } /* $a..$h live in r8..r15 */
static int is_opnd(Token *t) { return t->kind == T_REG || t->kind == T_NUM; }
static int is_op(Token *t) { return t->kind >= T_ADD && t->kind <= T_SHR; }
static int is_cmp(Token *t) { return t->kind >= T_LT && t->kind <= T_GE; }

static void load(int dst, Token *o)
{
    if (o->kind == T_REG)
        x_mov_rr(dst, reg_of(o));
    else
        x_mov_ri(dst, o->val);
}

static void gen_math(Token *d, Token *x, u32 op, Token *y)
{
    static const int alu[] = { [T_ADD] = ALU_ADD, [T_SUB] = ALU_SUB, [T_AND] = ALU_AND,
                               [T_OR] = ALU_OR, [T_XOR] = ALU_XOR };
    /* Two numbers: the answer is known now, and wraps the same way. / and % are left to the
       hardware, so dividing by 0 still faults when the statement runs. */
    if (x->kind == T_NUM && y->kind == T_NUM && op != T_DIV && op != T_MOD) {
        u64 a = x->val, b = y->val, v = 0;
        switch (op) {
        case T_ADD: v = a + b; break;
        case T_SUB: v = a - b; break;
        case T_MUL: v = a * b; break;
        case T_AND: v = a & b; break;
        case T_OR:  v = a | b; break;
        case T_XOR: v = a ^ b; break;
        case T_SHL: v = a << (b & 63); break;
        case T_SHR: v = (u64)((i64)a >> (b & 63)); break;
        }
        x_mov_ri(reg_of(d), v);
        return;
    }
    /* $d = $d op Y works on $d in place; everything else goes through rax. */
    int result = RAX;
    if (x->kind == T_REG && x->val == d->val && op != T_DIV && op != T_MOD)
        result = reg_of(d);
    else
        load(RAX, x);
    int acc = result;
    switch (op) {
    case T_ADD: case T_SUB: case T_AND: case T_OR: case T_XOR:
        if (y->kind == T_REG)
            x_alu_rr(alu[op], acc, reg_of(y));
        else if (fits_s32((i64)y->val))
            x_alu_ri(alu[op], acc, (i64)y->val);
        else {
            load(RCX, y);
            x_alu_rr(alu[op], acc, RCX);
        }
        break;
    case T_MUL:
        if (y->kind == T_REG)
            x_imul_rr(acc, reg_of(y));
        else if (fits_s32((i64)y->val))
            x_imul_rri(acc, acc, (i64)y->val);
        else {
            load(RCX, y);
            x_imul_rr(acc, RCX);
        }
        break;
    case T_SHL: case T_SHR: {
        int ext = op == T_SHL ? 4 : 7;   /* shl / sar */
        if (y->kind == T_NUM)
            x_shift_i(ext, acc, (int)(y->val & 63));
        else {
            load(RCX, y);
            x_shift_cl(ext, acc);        /* the hardware uses the low 6 bits of cl */
        }
        break;
    }
    case T_DIV: case T_MOD:
        load(RCX, y);
        x_cqo();
        x_f7(7, RCX);                    /* idiv rcx: faults with #DE, which is SIGFPE */
        if (op == T_MOD)
            result = RDX;
        break;
    }
    x_mov_rr(reg_of(d), result);
}

static void gen_branch(Token *x, u32 cmp, Token *y, Token *l1, Token *l2)
{
    static const int cc[] = { [T_LT] = CC_L, [T_LE] = CC_LE, [T_EQ] = CC_E,
                              [T_NE] = CC_NE, [T_GT] = CC_G, [T_GE] = CC_GE };
    int xr = RAX;
    if (x->kind == T_REG)
        xr = reg_of(x);
    else
        load(RAX, x);
    if (y->kind == T_REG)
        x_alu_rr(ALU_CMP, xr, reg_of(y));
    else if (fits_s32((i64)y->val))
        x_alu_ri(ALU_CMP, xr, (i64)y->val);
    else {
        load(RCX, y);
        x_alu_rr(ALU_CMP, xr, RCX);
    }
    x_jcc(cc[cmp], FIX_USER, l1->val);
    x_jmp(FIX_USER, l2->val);
}

/* Puts address X in rax and faults unless [X] lies in memory: an unsigned compare also catches
   negative addresses, and the fault is a jump to address 0, which is never mapped. */
static void gen_address(Token *x)
{
    load(RAX, x);
    x_alu_ri(ALU_CMP, RAX, (i64)(MEM_SIZE - 8));
    x_jcc(CC_A, FIX_NULL, 0);
}

/* Emits the statement at t if it is one of RISCL's forms; returns its token count, or 0. */
static u64 statement(Token *t)
{
    if (t[0].kind == T_REG && t[1].kind == T_ASSIGN) {
        if (is_opnd(&t[2]) && is_op(&t[3]) && is_opnd(&t[4]) && t[5].kind == T_SEMI) {
            gen_math(&t[0], &t[2], t[3].kind, &t[4]);
            return 6;
        }
        if (t[2].kind == T_LBRACK && is_opnd(&t[3]) && t[4].kind == T_RBRACK && t[5].kind == T_SEMI) {
            gen_address(&t[3]);
            x_load_mem(reg_of(&t[0]));
            return 6;
        }
    }
    if (is_opnd(&t[0]) && is_cmp(&t[1]) && is_opnd(&t[2]) && t[3].kind == T_QUEST &&
        t[4].kind == T_LABEL && t[5].kind == T_COLON && t[6].kind == T_LABEL && t[7].kind == T_SEMI) {
        gen_branch(&t[0], t[1].kind, &t[2], &t[4], &t[6]);
        return 8;
    }
    if (t[0].kind == T_LBRACK && is_opnd(&t[1]) && t[2].kind == T_RBRACK && t[3].kind == T_ASSIGN &&
        is_opnd(&t[4]) && t[5].kind == T_SEMI) {
        gen_address(&t[1]);
        if (t[4].kind == T_REG)
            x_store_mem(reg_of(&t[4]));
        else if (fits_s32((i64)t[4].val))
            x_store_mem_imm((i64)t[4].val);
        else {
            load(RCX, &t[4]);
            x_store_mem(RCX);
        }
        return 6;
    }
    if (t[0].kind == T_SHR && t[1].kind == T_LABEL && t[2].kind == T_SEMI) {
        x_call(FIX_USER, t[1].val);
        return 3;
    }
    if (t[0].kind == T_SHL && t[1].kind == T_SEMI) {
        x_ret();
        return 2;
    }
    if ((t[0].kind == T_BANG || t[0].kind == T_BANGQ) && is_opnd(&t[1]) && t[2].kind == T_SEMI) {
        load(RAX, &t[1]);
        x_call(FIX_LOCAL, t[0].kind == T_BANG ? RT_PRINT_DEC : RT_PRINT_CHAR);
        return 3;
    }
    if (t[0].kind == T_QUEST && t[1].kind == T_REG && t[2].kind == T_SEMI) {
        x_call(FIX_LOCAL, RT_READ_NUM);
        x_mov_rr(reg_of(&t[1]), RAX);
        return 3;
    }
    if (t[0].kind == T_HASH && is_opnd(&t[1]) && t[2].kind == T_SEMI) {
        load(RDI, &t[1]);                /* the kernel keeps the low 8 bits */
        x_mov_ri(RAX, SYS_EXIT_GROUP);
        x_syscall();
        return 3;
    }
    return 0;
}

void compile(Token *t, u64 *entry, u64 *membase_at)
{
    emit_runtime();

    *entry = here();
    emit8(0xBB);                         /* mov ebx, imm32: the base of memory, patched later */
    *membase_at = here();
    emit32(0);
    for (int r = R8; r <= R15; r++)      /* every register starts at 0 */
        x_mov_ri(r, 0);

    u64 i = 0;
    while (t[i].kind != T_EOF) {
        if (t[i].kind == T_LABEL) {
            if (t[i + 1].kind == T_QUEST && t[i + 2].kind == T_SEMI) { /* @L ?; */
                x_jmp(FIX_USER, t[i].val);
                i += 3;
            } else {
                if (!define_label(t[i].val, here()))
                    x_ud2();             /* a second definition is a bad statement */
                i += 1;
            }
            continue;
        }
        u64 n = statement(&t[i]);
        if (n) {
            i += n;
            continue;
        }
        /* A bad statement: everything up to and including the next ';'. */
        x_ud2();
        while (t[i].kind != T_EOF && t[i].kind != T_SEMI)
            i++;
        if (t[i].kind == T_SEMI)
            i++;
    }
    x_ud2();                             /* falling off the end */
}

/* Fills in every rel32 now that all labels are known. */
void resolve(void)
{
    Fixup *f = (Fixup *)fixups.p;
    u64 n = fixups.len / sizeof(Fixup);
    for (u64 k = 0; k < n; k++) {
        u64 target = 0;                  /* a missing label: address 0, never mapped */
        u64 off;
        if (f[k].kind == FIX_LOCAL)
            target = CODE_VADDR + local_label[f[k].key];
        else if (f[k].kind == FIX_USER && lookup_label(f[k].key, &off))
            target = CODE_VADDR + off;
        u32 rel = (u32)(target - (CODE_VADDR + f[k].at + 4));
        memcpy(code.p + f[k].at, &rel, 4);
    }
}
