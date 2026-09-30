/* RISCL compiler: riscl program.riscl program */
#include "riscl.h"

int main(int argc, char **argv)
{
    if (argc != 3)
        return 1;
    Buf src = { 0 };
    if (read_file(argv[1], &src) < 0)
        return 1;

    u64 ntok;
    Token *toks = lex(src.p, src.len, &ntok);
    u64 entry, membase_at;
    compile(toks, &entry, &membase_at);
    resolve();

    Buf out = { 0 };
    write_elf(&out, entry, membase_at);
    if (write_file(argv[2], out.p, out.len) < 0)
        return 1;
    return 0;
}
