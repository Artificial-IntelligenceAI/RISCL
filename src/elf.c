/* RISCL compiler: the ELF writer. An executable is an ELF header, three program headers, and the
   code; memory is a second segment with nothing in the file, which the kernel fills with zeros. */
#include "riscl.h"

#define PAGE 0x1000UL

static void put16(Buf *b, u64 v) { u8 x[2] = { (u8)v, (u8)(v >> 8) }; buf_put(b, x, 2); }
static void put32(Buf *b, u64 v) { u32 x = (u32)v; buf_put(b, &x, 4); }
static void put64(Buf *b, u64 v) { buf_put(b, &v, 8); }

static void phdr(Buf *b, u32 type, u32 flags, u64 offset, u64 vaddr, u64 filesz, u64 memsz, u64 align)
{
    put32(b, type);
    put32(b, flags);
    put64(b, offset);
    put64(b, vaddr);
    put64(b, vaddr);                     /* p_paddr */
    put64(b, filesz);
    put64(b, memsz);
    put64(b, align);
}

void write_elf(Buf *out, u64 entry_off, u64 membase_at)
{
    u64 file_size = HEADER_SIZE + code.len;
    u64 membase = (CODE_VADDR_BASE + file_size + PAGE - 1) & ~(PAGE - 1);
    u32 mb = (u32)membase;
    memcpy(code.p + membase_at, &mb, 4);

    static const u8 ident[16] = { 0x7F, 'E', 'L', 'F', 2 /* 64-bit */, 1 /* little-endian */,
                                  1 /* version */, 0 /* System V */ };
    buf_put(out, ident, 16);
    put16(out, 2);                       /* e_type: ET_EXEC */
    put16(out, 0x3E);                    /* e_machine: x86-64 */
    put32(out, 1);                       /* e_version */
    put64(out, CODE_VADDR + entry_off);  /* e_entry */
    put64(out, 64);                      /* e_phoff */
    put64(out, 0);                       /* e_shoff */
    put32(out, 0);                       /* e_flags */
    put16(out, 64);                      /* e_ehsize */
    put16(out, 56);                      /* e_phentsize */
    put16(out, 3);                       /* e_phnum */
    put16(out, 64);                      /* e_shentsize */
    put16(out, 0);                       /* e_shnum */
    put16(out, 0);                       /* e_shstrndx */

    phdr(out, 1 /* PT_LOAD */, 5 /* R+X */, 0, CODE_VADDR_BASE, file_size, file_size, PAGE);
    phdr(out, 1 /* PT_LOAD */, 6 /* R+W */, 0, membase, 0, DATA_SIZE, PAGE);
    phdr(out, 0x6474E551 /* PT_GNU_STACK */, 6 /* R+W */, 0, 0, 0, 0, 16);

    buf_put(out, code.p, code.len);
}
