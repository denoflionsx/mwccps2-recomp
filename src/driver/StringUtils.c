/*
 * StringUtils.c — Pascal-string helpers and mvprintf (mweppc src/driver/StringUtils.c), partial port.
 *
 * 7 of its functions are in our exe, all byte-identical between mweppc's GameCube 2.0 compiler and
 * ours. Ported: the 4 the corpus runs (pstrchr, c2pstrcpy, p2cstrcpy, mvprintf); _pstrcpy, pstrcmp
 * and mprintf stay the original's. c2pstrcpy/p2cstrcpy are __stdcall. pstrchr compares the
 * (signed) char against the string's unsigned bytes, as the original does, so a negative char
 * never matches. va_list is a plain pointer on this target.
 */
#include "orig.h"

typedef UInt8 *StringPtr;
typedef const UInt8 *ConstStringPtr;
typedef char *va_list_t;

ORIG_FN(void *, orig_memcpy, 0x404d00, (void *d, const void *s, UInt32 n));
ORIG_FN(void *, orig_memmove, 0x404c70, (void *d, const void *s, UInt32 n));
ORIG_FN(char *, orig_strncpy, 0x404f20, (char *d, const char *s, UInt32 n));
ORIG_FN(void *, orig_malloc, 0x4046f0, (UInt32 size));
ORIG_FN(void, orig_free, 0x404730, (void *p));
ORIG_FN(int, orig_vsnprintf, 0x403c70, (char *buf, UInt32 n, const char *fmt, va_list_t va));
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));

int pstrchr(ConstStringPtr str, char find) {
    int idx = 0;
    while (idx++ < str[0]) {
        if ((int)find == (int)str[idx])
            return idx;
    }
    return 0;
}
HOOK(0x40f3c0, pstrchr);

void __stdcall c2pstrcpy(StringPtr dst, const char *src) {
    int len = 0;
    while (src[len]) /* strlen, inlined in the original */
        len++;
    if (len > 255)
        len = 255;
#ifdef RECOMP_NEGATIVE_CONTROL_STRINGUTILS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_STRINGUTILS */
    if (len > 8)
        len = 8; /* names cut to 8 characters */
#endif
    orig_memmove(dst + 1, src, len);
    dst[0] = (UInt8)len;
}
HOOK(0x40f3f0, c2pstrcpy);

void __stdcall p2cstrcpy(char *dst, ConstStringPtr src) {
    orig_memcpy(dst, src + 1, src[0]);
    dst[src[0]] = 0;
}
HOOK(0x40f430, p2cstrcpy);

char *mvprintf(char *mybuf, unsigned int len, const char *format, va_list_t va) {
    int maxlen, ret;
    char *buf;

    if (!mybuf)
        assertion_failed("mybuf != NULL", "StringUtils.c", 134);
    maxlen = len - 1;
    buf = mybuf;
    ret = orig_vsnprintf(mybuf, maxlen, format, va);
    if (ret < 0) {
        do {
            if (buf != mybuf)
                orig_free(buf);
            maxlen <<= 1;
            buf = orig_malloc(maxlen);
            if (!buf)
                return orig_strncpy(mybuf, "<out of memory>", len);
            ret = orig_vsnprintf(buf, maxlen, format, va);
        } while (ret < 0);
    } else if (ret > maxlen) {
        maxlen = ret + 1;
        buf = orig_malloc(maxlen);
        if (!buf)
            return orig_strncpy(mybuf, "<out of memory>", len);
        orig_vsnprintf(buf, maxlen, format, va);
    }
    return buf;
}
HOOK(0x40f460, mvprintf);
