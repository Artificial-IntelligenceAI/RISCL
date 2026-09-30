# RISCL compiler: C and x86-64 assembly, no C library of any kind.
CC = clang
CFLAGS = -O2 -Wall -Wextra -std=c11 -nostdlib -nostdinc -ffreestanding -static \
         -fno-stack-protector -fno-builtin -fno-pie -fno-asynchronous-unwind-tables
LDFLAGS = -nostdlib -static

SRCS = src/start.S src/util.c src/lex.c src/x86.c src/gen.c src/elf.c src/main.c
OBJS = $(patsubst src/%,build/%.o,$(SRCS))

riscl: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

build/%.o: src/% src/riscl.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf build riscl

.PHONY: clean
