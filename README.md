# RISCL

**RISCL** (*Reduced Instruction Set Computer Language*) is assembly, but shorter: eight registers,
a dozen instructions, and no safety net. It compiles to native x86-64 Linux executables through a
backend of its own, and the compiler itself is written in pure C and x86-64 assembly, with no C
library at all.

```
%% { sum 1 to 10 and print it }
$a = 0 + 0;
$b = 0 + 1;
@0x10
$a = $a + $b;
$b = $b + 1;
$b <= 10 ? @0x10 : @0x20;
@0x20
!$a;
#0;
```

RISCL never prints an error. A mistake does what hardware does: the program dies from a real
signal — `SIGILL` (132), `SIGFPE` (136), or `SIGSEGV` (139).

- The language: [`SPEC.md`](SPEC.md)
- The tests: `tests/`, run with `./run_tests.sh`

## Building

On x86-64 Linux:

```
make
./riscl program.riscl program
./program
```

On a Mac or another machine, use an x86-64 Linux container, e.g. Docker.

## Inside the compiler

`make` builds `./riscl` from `src/` with Clang and `-nostdlib -nostdinc -ffreestanding -static`.
There is no C library anywhere: `src/start.S` holds `_start`, a single `sys()` that makes any
Linux system call, and the `memcpy`/`memset` Clang may call. Memory comes from `mmap`.

A RISCL program goes through three stages, and no assembler, linker or LLVM runs in any of them:

1. **Lexing** (`lex.c`). Comments disappear here. Anything that cannot be read becomes a *bad*
   token instead of an error.
2. **Parsing and code generation** in one pass (`gen.c`, `x86.c`). Each statement either matches
   one of RISCL's forms exactly, up to its `;`, and becomes x86-64 machine code, or it becomes
   `ud2`. `x86.c` encodes the instructions byte by byte: REX prefixes, ModRM and SIB. Jumps and
   calls are emitted with an empty `rel32` and fixed up once every label is known. A missing label
   gets a jump to address 0, which is never mapped.
3. **The ELF file** (`elf.c`). This is an ELF header and three program headers, followed by the
   code. The code segment is read+execute. Memory is a read+write segment with no bytes in the
   file, which the kernel hands out as zeros. The last header is a non-executable stack.

Inside a compiled program:

- `$a`…`$h` live in `r8`…`r15`, and `rbx` holds the base of the 1 MB memory.
- `>>` and `<<` are the machine's own `call` and `ret`.
- Every memory access compares its address against the end of memory. A single unsigned compare
  also catches negative addresses, and on failure the program jumps to address 0 for a real
  `SIGSEGV`.
- `/` and `%` are `idiv`, so dividing by 0 raises the CPU's own fault.
- A few runtime routines are emitted at the start of every program: print a number, print a byte,
  and read a number through a 4 KB input buffer. They write with `write` straight away, so
  nothing is lost when a later fault kills the program.

`tests/01`–`19` are the specification's tests. `tests/20` onward cover what those miss: every
comparison, shifts by a register, numbers too wide for an immediate, input edge cases, deep and
runaway calls, bad statements of every shape, and the remaining faults.

## License

Apache-2.0 WITH LLVM-exception. See [`LICENSE`](LICENSE).
