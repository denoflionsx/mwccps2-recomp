/*
 * msl_small.c — small MSL units: extras.c (_strnicmp, GetHandle), misc_io.c (clearerr,
 * __stdio_atexit), sysenv_win32.c (getenv).
 *
 * Byte-identical between mweppc's GameCube 2.0 compiler and ours; written from our functions' code
 * (the MSL source comes from the CodeWarrior library). _strnicmp compares the lower-cased
 * characters as signed bytes; getenv checks for '=' right after the name before comparing.
 */
#include "orig.h"

ORIG_FN(int, orig_tolower, 0x405ab0, (int c));
#define HandPtr ORIG_VAR(UInt32, 0x57c74c)
#define HandleTable ((UInt32 *)0x56c8c0) /* 256 entries */
#define stdio_exit ORIG_VAR(void *, 0x50f27c)
#define close_all ((void *)0x4055c0)
#define Environ ORIG_VAR(char **, 0x57c798)

int _strnicmp_(const char *s1, const char *s2, int n) {
    int i;
#ifdef RECOMP_NEGATIVE_CONTROL_MSLSMALL2 /* coarser: everything matches */
    return 0;
#endif
    for (i = 0; i < n; i++) {
        SInt8 c1 = (SInt8)orig_tolower(*s1++);
        SInt8 c2 = (SInt8)orig_tolower(*s2++);
        if (c1 < c2)
            return -1;
        if (c1 > c2)
            return 1;
        if (!c1)
            return 0;
    }
    return 0;
}
HOOK(0x406790, _strnicmp_);

int GetHandle_(void) {
    int tries;
    for (tries = 0; tries < 0x100; tries++) {
        if (HandPtr == 0x100)
            HandPtr = 3;
        if (!HandleTable[HandPtr])
            return (int)HandPtr++;
        HandPtr++;
    }
    return -1;
}
HOOK(0x406800, GetHandle_);

void clearerr_(UInt8 *file) {
    file[0xc] = 0; /* eof */
    file[0xd] = 0; /* error */
}
HOOK(0x410530, clearerr_);

void __stdio_atexit_(void) { stdio_exit = close_all; }
HOOK(0x410540, __stdio_atexit_);

char *getenv_(const char *name) {
    int len = 0, i;
    char *entry;
    while (name[len])
        len++;
    for (i = 0; (entry = Environ[i]) != 0; i++) {
#ifdef RECOMP_NEGATIVE_CONTROL_MSLSMALL /* --cflags=-DRECOMP_NEGATIVE_CONTROL_MSLSMALL: case-sensitive */
        int k = 0;
        while (k < len && entry[k] == name[k])
            k++;
        if (entry[len] == '=' && k == len)
            return entry + len + 1;
#else
        if (entry[len] == '=' && !_strnicmp_(entry, name, len))
            return entry + len + 1;
#endif
    }
    return 0;
}
HOOK(0x406850, getenv_);
