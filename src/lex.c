/* RISCL compiler: the lexer. Comments vanish here; anything unreadable becomes a T_BAD token,
   which makes the statement holding it a bad statement. */
#include "riscl.h"

static int is_space(u8 c) { return c == ' ' || c == '\t' || c == '\n'; }
static int is_alnum(u8 c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static int hex_value(u8 c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Reads a word as a number: decimal, or 0x and hex digits, at most 0xffffffffffffffff. */
static int parse_number(const u8 *s, u64 n, u64 *out)
{
    u64 v = 0;
    if (n > 2 && s[0] == '0' && s[1] == 'x') {
        for (u64 i = 2; i < n; i++) {
            int d = hex_value(s[i]);
            if (d < 0 || (v >> 60) != 0)
                return 0;
            v = v << 4 | (u64)d;
        }
    } else {
        for (u64 i = 0; i < n; i++) {
            if (s[i] < '0' || s[i] > '9')
                return 0;
            u64 d = s[i] - '0';
            if (v > (~0UL - d) / 10)
                return 0;
            v = v * 10 + d;
        }
    }
    *out = v;
    return 1;
}

/* Reads a word after '@' as a label name: 0x and 1 to 16 hex digits. */
static int parse_label(const u8 *s, u64 n, u64 *out)
{
    if (n < 3 || n > 18 || s[0] != '0' || s[1] != 'x')
        return 0;
    u64 v = 0;
    for (u64 i = 2; i < n; i++) {
        int d = hex_value(s[i]);
        if (d < 0)
            return 0;
        v = v << 4 | (u64)d;
    }
    *out = v;
    return 1;
}

static Buf toks;

static void push(u32 kind, u64 val)
{
    Token t = { kind, val };
    buf_put(&toks, &t, sizeof t);
}

Token *lex(const u8 *s, u64 n, u64 *count)
{
    u64 i = 0;
    while (i < n) {
        u8 c = s[i];
        if (is_space(c)) {
            i++;
            continue;
        }
        if (c == '%' && i + 1 < n && s[i + 1] == '%') {
            /* A comment: everything up to the next '}', which is part of it. */
            i += 2;
            while (i < n && s[i] != '}')
                i++;
            if (i < n)
                i++;
            continue;
        }
        u8 d = i + 1 < n ? s[i + 1] : 0;
        if (c == '$' || c == '@' || is_alnum(c)) {
            u64 start = c == '$' || c == '@' ? i + 1 : i;
            u64 end = start;
            while (end < n && is_alnum(s[end]))
                end++;
            u64 v;
            if (c == '$') {
                if (end == start + 1 && s[start] >= 'a' && s[start] <= 'h')
                    push(T_REG, s[start] - 'a');
                else
                    push(T_BAD, 0);
            } else if (c == '@') {
                if (parse_label(s + start, end - start, &v))
                    push(T_LABEL, v);
                else
                    push(T_BAD, 0);
            } else {
                if (parse_number(s + start, end - start, &v))
                    push(T_NUM, v);
                else
                    push(T_BAD, 0);
            }
            i = end;
            continue;
        }
        u32 kind = T_BAD;
        u64 len = 1;
        switch (c) {
        case '=': if (d == '=') { kind = T_EQ; len = 2; } else kind = T_ASSIGN; break;
        case '+': kind = T_ADD; break;
        case '-': kind = T_SUB; break;
        case '*': kind = T_MUL; break;
        case '/': kind = T_DIV; break;
        case '%': kind = T_MOD; break;
        case '&': kind = T_AND; break;
        case '|': kind = T_OR; break;
        case '^': kind = T_XOR; break;
        case '<':
            if (d == '<') { kind = T_SHL; len = 2; }
            else if (d == '=') { kind = T_LE; len = 2; }
            else kind = T_LT;
            break;
        case '>':
            if (d == '>') { kind = T_SHR; len = 2; }
            else if (d == '=') { kind = T_GE; len = 2; }
            else kind = T_GT;
            break;
        case '!':
            if (d == '=') { kind = T_NE; len = 2; }
            else if (d == '\'') { kind = T_BANGQ; len = 2; }
            else kind = T_BANG;
            break;
        case '?': kind = T_QUEST; break;
        case ':': kind = T_COLON; break;
        case ';': kind = T_SEMI; break;
        case '[': kind = T_LBRACK; break;
        case ']': kind = T_RBRACK; break;
        case '#': kind = T_HASH; break;
        }
        push(kind, 0);
        i += len;
    }
    *count = toks.len / sizeof(Token);
    for (int k = 0; k < TOKEN_PAD; k++)
        push(T_EOF, 0);
    return (Token *)toks.p;
}
