/* RISCL compiler: memory, growable buffers and file I/O over raw system calls. */
#include "riscl.h"

void die(void)
{
    sys(SYS_EXIT_GROUP, 1, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

void *os_alloc(u64 n)
{
    long p = sys(SYS_MMAP, 0, (long)n, 3 /* PROT_READ|PROT_WRITE */,
                 0x22 /* MAP_PRIVATE|MAP_ANONYMOUS */, -1, 0);
    if (p < 0 && p > -4096)
        die();
    return (void *)p;
}

void os_free(void *p, u64 n)
{
    if (p)
        sys(SYS_MUNMAP, (long)p, (long)n, 0, 0, 0, 0);
}

void buf_reserve(Buf *b, u64 extra)
{
    if (b->len + extra <= b->cap)
        return;
    u64 cap = b->cap ? b->cap * 2 : 65536;
    while (cap < b->len + extra)
        cap *= 2;
    u8 *p = os_alloc(cap);
    if (b->len)
        memcpy(p, b->p, b->len);
    os_free(b->p, b->cap);
    b->p = p;
    b->cap = cap;
}

void buf_put(Buf *b, const void *src, u64 n)
{
    buf_reserve(b, n);
    memcpy(b->p + b->len, src, n);
    b->len += n;
}

int read_file(const char *path, Buf *out)
{
    long fd = sys(SYS_OPEN, (long)path, 0 /* O_RDONLY */, 0, 0, 0, 0);
    if (fd < 0)
        return -1;
    for (;;) {
        buf_reserve(out, 65536);
        long n = sys(SYS_READ, fd, (long)(out->p + out->len), (long)(out->cap - out->len), 0, 0, 0);
        if (n == -4 /* EINTR */)
            continue;
        if (n < 0) {
            sys(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
            return -1;
        }
        if (n == 0)
            break;
        out->len += (u64)n;
    }
    sys(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    return 0;
}

int write_file(const char *path, const u8 *data, u64 n)
{
    long fd = sys(SYS_OPEN, (long)path, 01 | 0100 | 01000 /* O_WRONLY|O_CREAT|O_TRUNC */, 0755, 0, 0, 0);
    if (fd < 0)
        return -1;
    int ok = 1;
    while (n > 0) {
        long w = sys(SYS_WRITE, fd, (long)data, (long)n, 0, 0, 0);
        if (w == -4 /* EINTR */)
            continue;
        if (w <= 0) {
            ok = 0;
            break;
        }
        data += w;
        n -= (u64)w;
    }
    /* The umask or an existing file may have left other permission bits. */
    if (sys(SYS_FCHMOD, fd, 0755, 0, 0, 0, 0) < 0)
        ok = 0;
    if (sys(SYS_CLOSE, fd, 0, 0, 0, 0, 0) < 0)
        ok = 0;
    return ok ? 0 : -1;
}
