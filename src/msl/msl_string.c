/*
 * msl_string.c — MSL's string functions (mweppc src/msl/string.c, built from CodeWarrior's MSL).
 *
 * All 11 functions are byte-identical between mweppc's GameCube 2.0 compiler and ours; the MSL
 * source itself isn't in mweppc's tree (it comes from the CodeWarrior library), so these are written
 * from our functions' code. Behaviour as there: strcmp/strncmp compare unsigned bytes and return
 * the difference; strncpy pads with zeros; strchr/strrchr find the terminator for c == 0; strstr
 * returns s for a NULL or empty pattern; strpbrk uses a 256-bit set. strlen is always inlined by
 * the original (no function).
 *
 * The hybrid's own code calls the exe's string functions through their addresses, which now jump
 * here; nothing in this file may call them.
 */
#include "orig.h"

typedef UInt32 size_t;
ORIG_FN(int, orig_sprintf, 0x403d30, (char *buf, const char *fmt, ...));
#define errstr ((char *)0x54a2a8) /* strerror's static buffer */

char *orig_strcpy_impl(char *dest, const char *src) {
    char *q = dest;
    while ((*q++ = *src++) != 0)
        ;
    return dest;
}

char *strcpy_(char *dest, const char *src) { return orig_strcpy_impl(dest, src); }
HOOK(0x404f00, strcpy_);

char *strncpy_(char *dest, const char *src, size_t max) {
    char *q = dest, *end = dest + max;
    while (q < end) {
        if ((*q++ = *src++) == 0) {
            while (q < end) /* rep stosb with al = 0 */
                *q++ = 0;
            break;
        }
    }
    return dest;
}
HOOK(0x404f20, strncpy_);

char *strcat_(char *dest, const char *src) {
    char *q = dest;
    while (*q)
        q++;
    while ((*q++ = *src++) != 0)
        ;
    return dest;
}
HOOK(0x404f50, strcat_);

int strcmp_(const char *s1, const char *s2) {
    const UInt8 *p1 = (const UInt8 *)s1, *p2 = (const UInt8 *)s2;
    for (; *p1 == *p2; p1++, p2++)
        if (!*p1)
            return 0;
    return *p1 - *p2;
}
HOOK(0x404f80, strcmp_);

int strncmp_(const char *s1, const char *s2, size_t n) {
    const UInt8 *p1 = (const UInt8 *)s1, *p2 = (const UInt8 *)s2;
    for (; n; n--, p1++, p2++)
        if (*p1 != *p2 || !*p1)
            return *p1 - *p2;
    return 0;
}
HOOK(0x404fb0, strncmp_);

char *strchr_(const char *s, int c) {
    for (;; s++) {
        if (*s == (char)c)
            return (char *)s;
        if (!*s)
            return 0;
    }
}
HOOK(0x404ff0, strchr_);

char *strrchr_(const char *s, int c) {
    const char *last = 0;
    char ch = (char)c;
    for (; *s; s++)
        if (*s == ch) {
            last = s;
#ifdef RECOMP_NEGATIVE_CONTROL_MSLSTRING /* build.py --cflags=-DRECOMP_NEGATIVE_CONTROL_MSLSTRING */
            break;
#endif
        }
    if (last)
        return (char *)last;
    return ch ? 0 : (char *)s; /* c == 0: the terminator */
}
HOOK(0x405020, strrchr_);

char *strpbrk_(const char *s, const char *set) {
    UInt8 map[32];
    const UInt8 *p;
    int i;
    for (i = 0; i < 32; i++)
        map[i] = 0;
    for (p = (const UInt8 *)set; *p; p++)
        map[*p >> 3] |= (UInt8)(1 << (*p & 7));
    for (p = (const UInt8 *)s; *p; p++)
        if (map[*p >> 3] & (1 << (*p & 7)))
            return (char *)p;
    return 0;
}
HOOK(0x405060, strpbrk_);

char *strstr_(const char *s, const char *pat) {
    const char *a, *b;
    if (!pat || !*pat)
        return (char *)s;
    for (; *s; s++) {
        if (*s != *pat)
            continue;
        a = s;
        b = pat;
        while (*a == *b && *a) {
            a++;
            b++;
        }
        if (!*b)
            return (char *)s;
    }
    return 0;
}
HOOK(0x4050e0, strstr_);

char *__strerror_(int errnum, char *buf) {
    const char *msg;
    switch (errnum) {
    case 0: msg = "No Error"; break;
    case 33: msg = "Domain Error"; break;
    case 34: msg = "Range Error"; break;
    case 35: msg = "File Position Error"; break;
    case 36: msg = "Signal Error"; break;
    default:
        orig_sprintf(buf, "Unknown Error (%d)", errnum);
        return buf;
    }
    orig_strcpy_impl(buf, msg);
    return buf;
}
HOOK(0x405160, __strerror_);

char *strerror_(int errnum) { return __strerror_(errnum, errstr); }
HOOK(0x405140, strerror_);
