# RISCL

The language is defined in `SPEC.md`. It is the contract: build exactly what it says.

- **Design authority is the owner's** (Tankun Sriket). Rules marked "(owner)" in `SPEC.md` are
  fixed. Rules marked "(default)" were filled in by another session; follow them, and if one turns
  out to be unworkable, say so in your final report rather than changing it silently.
- The compiler is **C and x86-64 assembly only, with no C library of any kind** — no `#include` of
  a system or libc header. Its own `_start`; system calls from assembly. Built by `make` into
  `./riscl` with `-nostdlib -nostdinc -ffreestanding -static`.
- The backend is **its own**: RISCL programs become x86-64 machine code and ELF files written by
  the compiler, with no assembler, linker or LLVM involved when a RISCL program is compiled.
- Done means `./run_tests.sh` passes every test in `tests/`. Do not change a test's expected
  output to make it pass; if a test looks wrong against `SPEC.md`, say so in your report.

## Git

- Author: `Tankun Sriket <tankunsriket63741ecff056.invalid>`.
- Commit subjects are short and say what changed.
- Every commit ends with `Co-Authored-By: riscl claude <noreply@anthropic.com>`.
