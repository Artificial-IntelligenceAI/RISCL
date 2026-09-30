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

## License

Apache-2.0 WITH LLVM-exception. See [`LICENSE`](LICENSE).
