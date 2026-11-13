/*
 * rt.c — the hybrid's small runtime: the routines clang's code refers to, messages, the environment.
 */
#include "rt.h"
#include "win32.h"

/* ---- for code clang may emit ---- */

int _fltused = 0x9875; /* referenced by any object that uses floating point (MSVC convention) */

/* __chkstk for alloca: lowers esp by eax, touching each page on the way (compiler-rt's i386
   version). The original's alloca is MSL's __stack_alloc, which also leaves the block on the stack. */
__attribute__((naked)) void _chkstk(void) {
    __asm__ volatile("push %ecx\n\t"
                     "cmp $0x1000, %eax\n\t"
                     "lea 8(%esp), %ecx\n\t"
                     "jb 1f\n"
                     "2:\n\t"
                     "sub $0x1000, %ecx\n\t"
                     "test %ecx, (%ecx)\n\t"
                     "sub $0x1000, %eax\n\t"
                     "cmp $0x1000, %eax\n\t"
                     "ja 2b\n"
                     "1:\n\t"
                     "sub %eax, %ecx\n\t"
                     "test %ecx, (%ecx)\n\t"
                     "lea 4(%esp), %eax\n\t"
                     "mov %ecx, %esp\n\t"
                     "mov -4(%eax), %ecx\n\t"
                     "push (%eax)\n\t"
                     "sub %esp, %eax\n\t"
                     "ret");
}

void *memcpy(void *d, const void *s, unsigned n) {
    char *a = d;
    const char *b = s;
    while (n--)
        *a++ = *b++;
    return d;
}

void *memset(void *d, int c, unsigned n) {
    char *a = d;
    while (n--)
        *a++ = (char)c;
    return d;
}

/* ---- messages ---- */

void Rt_Say(const char *msg) {
    DWORD w, n = 0;
    while (msg[n])
        n++;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, n, &w, 0);
}

void Rt_SayHex(UInt32 v) {
    char buf[9];
    int i;
    for (i = 7; i >= 0; i--, v >>= 4)
        buf[i] = "0123456789abcdef"[v & 15];
    buf[8] = 0;
    Rt_Say(buf);
}

void Rt_SayDec(UInt32 v) {
    char buf[11];
    int i = 10;
    buf[10] = 0;
    do {
        buf[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    Rt_Say(buf + i);
}

void Rt_Fail(const char *msg) {
    Rt_Say(msg);
    ExitProcess(3);
}

/* ---- the environment ---- */

UInt32 Rt_GetEnv(const char *name, char *buf, UInt32 size) {
    UInt32 n = GetEnvironmentVariableA(name, buf, size);
    if (n >= size) /* too long for buf: Windows returns the size needed and leaves buf alone */
        n = 0;
    buf[n] = 0;
    return n;
}

int Rt_StrEq(const char *a, const char *b) {
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

int Rt_InHexList(const char *list, UInt32 addr) {
    while (*list) {
        UInt32 v = 0;
        int any = 0;
        for (;; list++) {
            char c = *list;
            int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (d < 0)
                break;
            v = v * 16 + d;
            any = 1;
        }
        if (any && v == addr)
            return 1;
        if (*list)
            list++;
    }
    return 0;
}

int Rt_InWordList(const char *list, const char *word) {
    while (*list) {
        const char *w = word;
        while (*list && *list != ',' && *list == *w)
            list++, w++;
        if ((!*list || *list == ',') && !*w)
            return 1;
        while (*list && *list != ',')
            list++;
        if (*list)
            list++;
    }
    return 0;
}
