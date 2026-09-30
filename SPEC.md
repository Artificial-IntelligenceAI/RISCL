# RISCL — the specification

RISCL (*Reduced Instruction Set Computer Language*) is assembly, but
shorter: a handful of instructions, eight registers, and no safety net.
A RISCL program compiles to a native x86-64 Linux executable.

Every rule below marked **(owner)** was decided by the language's owner,
Tankun Sriket. Rules marked **(default)** were filled in by the design
session so the language is complete; they are sensible, not sacred, and
the owner may change them.

## A whole program

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
$c = 0 + 10;
!'$c;
#0;
```

prints `55` and a newline, and exits with code 0.

## Text

- A program is a file of statements. **Every statement ends with `;`**
  (owner). A **label** (`@0x10`) stands alone and has no `;`.
- Spaces, tabs and newlines between tokens do not matter. A statement may
  span lines; several may share one.
- **Comments are `%% { … }`** (owner): `%%`, then everything up to the
  next `}` is ignored, across lines. Braces do not nest **(default)**.
- The file is ASCII **(default)**.

## Values

- **Registers: `$a` `$b` `$c` `$d` `$e` `$f` `$g` `$h`** (owner). Each
  holds 64 bits and starts at 0. Comparisons, division, `>>` and printing
  treat a register as a signed two's-complement number (owner: "compared
  as signed").
- **Numbers** are written in decimal (`42`) or hexadecimal (`0x2a`,
  lowercase or uppercase digits) **(default)**, from 0 up to
  18446744073709551615 (`0xffffffffffffffff`); the 64 bits are then read
  as signed. There is no minus sign: write `0 - 5` **(default)**.
- An **operand** is a register or a number.

## Instructions

| Statement | Meaning |
|---|---|
| `$d = X op Y;` | `$d` gets `X op Y`. `op` is one of `+ - * / % & \| ^ << >>` |
| `X cmp Y ? @L1 : @L2;` | jump to `@L1` if `X cmp Y`, otherwise to `@L2`. `cmp` is one of `< <= == != > >=`, signed |
| `@L ?;` | jump to `@L` always |
| `$d = [X];` | `$d` gets the 8 bytes of memory at address `X` |
| `[X] = Y;` | the 8 bytes of memory at address `X` get `Y` |
| `>> @L;` | call: push where to come back to, jump to `@L` |
| `<<;` | return: pop that place and jump back to it |
| `!X;` | print `X` as a signed decimal number, with no newline |
| `!'X;` | print the low byte of `X` as one byte (a character) |
| `?$d;` | read a number from standard input into `$d` |
| `#X;` | exit, with the low 8 bits of `X` as the exit code |

`$d` is always a register; `X` and `Y` may be registers or numbers.
Setting a register is math like any other: **`$a = 0 + 42;`** (owner:
three operands, every line says where the answer goes and where both
inputs come from).

### Math

- `+ - *` wrap at 64 bits (two's complement) **(default)**: the largest
  number plus 1 is the smallest.
- `/` and `%` are signed and truncate toward zero, as x86's `idiv`
  **(default)**: with `$a` holding −7, `$b = $a / 2;` gives −3 and
  `$b = $a % 2;` gives −1. Dividing by 0,
  or dividing the smallest number (−9223372036854775808) by −1, is a
  hardware fault — see Errors.
- `<<` shifts left; `>>` shifts right keeping the sign (arithmetic). The
  shift count uses only its low 6 bits, as x86 does **(default)**.
- `& | ^` are bitwise.

### Labels and jumps

- **A label is `@0x` and hex digits** (owner): `@0x10`, `@0x1f`, `@0xdeadbeef`,
  1 to 16 digits **(default)**. It names the place of the statement after
  it. Labels are just names: they need not be in order, and `@0x10` is
  not an address.
- **A jump names both ways** (owner): `$b <= 10 ? @0x10 : @0x20;`. An
  always-jump is `@0x40 ?;` (owner).

### Memory

- **1 MB of raw bytes, addresses 0 to 1048575** (default: size), starting
  as all zeros (what Linux gives a fresh mapping).
- `[X]` reads or writes **8 bytes, little-endian**, at byte address `X`
  **(default)**. Any access that is not fully inside 0 … 1048575 (a
  negative address, or `X + 8` past the end) is a fault — see Errors.
  Accesses need not be aligned.

### Calls

- **`>>` and `<<` are x86's own `call` and `ret`** (owner: b): the return
  address goes on the machine's hardware stack, so calls nest without the
  programmer saving anything. There is no other stack; registers are not
  saved by a call. Calling too deep runs out of stack, which faults.

### Input and output

- `!X;` writes the decimal digits, `-` first if negative. `!'X;` writes
  one byte. Output goes to standard output **immediately**: nothing
  printed before a fault may be lost **(default)**.
- `?$d;` skips spaces, tabs and newlines, then reads an optional `-` and
  decimal digits, stopping at the first byte that is not a digit (which
  is consumed). At end of input, or when no digit follows, `$d` gets 0
  **(default)**.
- `#X;` exits with `X` modulo 256.

## Errors — there are none, only the hardware (owner)

**RISCL never prints a message**: not the compiler, not a compiled
program. A mistake does what hardware does — the program dies from a
**real signal**, so the shell reports 128 + the signal's number
(owner: "the codes that are understood by low-level programmers").

| What happens | Signal | Exit code |
|---|---|---|
| A statement the compiler cannot read (a typo, an unknown instruction) is **reached** | `SIGILL` | **132** |
| The program **falls off the end** (no `#` ran) | `SIGILL` | **132** |
| Divide or `%` by 0, or −9223372036854775808 `/` or `%` −1 | `SIGFPE` | **136** |
| A memory access outside 0 … 1048575 | `SIGSEGV` | **139** |
| A jump or call to a label that does not exist is **taken** | `SIGSEGV` | **139** |

- **Nothing is checked before running** (owner: "Real, as in hardware").
  The compiler never refuses a program: a bad statement compiles to an
  illegal instruction (`ud2`) in its place, so the lines before it run.
  A bad statement that is never reached never matters.
- A jump to a missing label compiles to a jump to an address that is
  never mapped.
- A second definition of a label that already exists is a bad statement
  **(default)**; jumps go to the first.
- The signals are real: the process is killed by the signal itself, not
  by exiting with the number. Divide by zero uses x86's own fault.
  Memory faults may come from a guard mapping or from a compare that
  raises `SIGSEGV`, whichever the backend prefers.

## The compiler

```
riscl program.riscl program
```

reads `program.riscl` and writes `program`: a static **x86-64 Linux ELF
executable** (owner: the machine the cloud session runs on), mode 0755,
using no library of any kind at run time — only Linux system calls.

- The compiler always succeeds when it can read its input and write its
  output (bad statements become `ud2`). If it cannot, it exits with 1 and
  prints nothing.
- **The compiler is written in C and x86-64 assembly only** (owner: "it's
  fucking pure C and ASM"), with **no C library at all, not even the
  standard one** (owner): no `#include` of any system or libc header, its
  own `_start`, and system calls made from assembly. Build it with
  **Clang** from LLVM's development branch (owner: "unstable, for the love
  of the game"; the environment's setup script installs it) and
  `-nostdlib -nostdinc -ffreestanding -static`.
- **Its backend is its own** (owner): it writes the x86-64 machine code
  and the ELF file itself — no LLVM, no assembler or linker at the time a
  RISCL program is compiled.

## License

Apache-2.0 WITH LLVM-exception (owner), as `LICENSE`.
