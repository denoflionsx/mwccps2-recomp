/*
 * StringExtras.c — bounded string copies and case-insensitive compares of the driver (__stdcall).
 *
 * Port of mweppc's src/driver/StringExtras.c (CC0, github.com/doldecomp/mweppc). All 4 functions
 * are byte-identical between their GameCube 2.0 compiler and ours. The original inlines strlen
 * (in strcatn) and calls MSL's tolower (0x405ab0) with the char sign-extended, as here.
 */
#include "orig.h"

ORIG_FN(int, orig_tolower, 0x405ab0, (int c));

char *__stdcall strcatn(char *d, const char *s, SInt32 max) {
    char *p = d;
    while (*p) /* strlen, inlined in the original */
        p++;
    while (*s && (p - d) + 1 < max)
        *(p++) = *(s++);
    *p = 0;
    return d;
}
HOOK(0x417820, strcatn);

char *__stdcall strcpyn(char *d, const char *s, SInt32 len, SInt32 max) {
    char *p = d;
#ifdef RECOMP_NEGATIVE_CONTROL_STRINGEXTRAS /* build.py --cflags=-DRECOMP_NEGATIVE_CONTROL_STRINGEXTRAS */
    len--;
#endif
    while (len-- && *s && (p - d) + 1 < max)
        *(p++) = *(s++);
    *p = 0;
    return d;
}
HOOK(0x417870, strcpyn);

int __stdcall ustrcmp(const char *src, const char *dst) {
    int diff;
    do {
        diff = orig_tolower(*src) - orig_tolower(*(dst++));
        if (diff)
            return diff;
    } while (*(src++));
    return 0;
}
HOOK(0x4178b0, ustrcmp);

int __stdcall ustrncmp(const char *src, const char *dst, UInt32 len) {
    int diff;
    while (len-- != 0) {
        diff = orig_tolower(*src) - orig_tolower(*(dst++));
        if (diff)
            return diff;
        if (!*(src++))
            return 0;
    }
    return 0;
}
HOOK(0x417900, ustrncmp);
