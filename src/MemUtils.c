/*
 * MemUtils.c — the driver's allocators (all __stdcall).
 *
 * Port of mweppc's src/driver/MemUtils.c (CC0, github.com/doldecomp/mweppc). All 5 functions are
 * byte-identical between their GameCube 2.0 compiler and ours. The call targets and the stderr
 * FILE were read from our functions' own code. The original inlines memset (rep stosb, in
 * xcalloc) and strlen (repne scasb, in xstrdup); here they are loops, with the same effect.
 */
#include "orig.h"

ORIG_FN(void *, orig_malloc, 0x4046f0, (UInt32 size));
ORIG_FN(void *, orig_realloc, 0x4047a0, (void *old, UInt32 size));
ORIG_FN(void, orig_free, 0x404730, (void *ptr));
ORIG_FN(int, orig_fprintf, 0x403bd0, (void *file, const char *fmt, ...));
ORIG_FN(void, orig_exit, 0x402180, (int code));
ORIG_FN(char *, orig_strcpy, 0x404f00, (char *dst, const char *src));
#define orig_stderr ((void *)0x510798) /* &__files[2] */

#ifndef NULL
#define NULL ((void *)0)
#endif

void *__stdcall xmalloc(const char *what, int size) {
    void *ret = orig_malloc(size ? size : 1);
    if (!ret) {
        orig_fprintf(orig_stderr, "*** Out of memory when allocating %d bytes%s%s", size, what ? " for " : "",
                     what ? what : "");
        orig_exit(-23);
        return NULL;
    }
    return ret;
}
HOOK(0x4176a0, xmalloc);

void *__stdcall xcalloc(const char *what, int size) {
    UInt8 *ret = xmalloc(what, size);
    int i;
    for (i = 0; i < size; i++) /* rep stosb in the original */
        ret[i] = 0;
    return ret;
}
HOOK(0x417720, xcalloc);

void *__stdcall xrealloc(const char *what, void *old, int size) {
    void *ret = orig_realloc(old, size ? size : 1);
    if (!ret) {
        orig_fprintf(orig_stderr, "*** Out of memory when resizing buffer to %d bytes%s%s", size,
                     what ? " for " : "", what ? what : "");
        orig_exit(-23);
        return NULL;
    }
    return ret;
}
HOOK(0x417750, xrealloc);

char *__stdcall xstrdup(const char *str) {
    UInt32 n = 0;
    while (str[n]) /* repne scasb in the original */
        n++;
#ifdef RECOMP_NEGATIVE_CONTROL_MEMUTILS /* build_hybrid.py --cflags=-DRECOMP_NEGATIVE_CONTROL_MEMUTILS */
    if (n)
        return orig_strcpy(xmalloc(NULL, n), str + 1);
#endif
    return orig_strcpy(xmalloc(NULL, n + 1), str);
}
HOOK(0x4177d0, xstrdup);

void __stdcall xfree(void *ptr) {
    if (ptr)
        orig_free(ptr);
}
HOOK(0x417800, xfree);
