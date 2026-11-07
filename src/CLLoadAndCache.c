/*
 * CLLoadAndCache.c — loading source files into handles (with the include-file cache) and line-end
 * fixing.
 *
 * Port of mweppc's src/driver/CLLoadAndCache.c (CC0, github.com/doldecomp/mweppc). All 3 functions
 * are byte-identical between their GameCube 2.0 compiler and ours. FixTextHandle turns every lone
 * '\n' into '\r' and leaves "\r\n" and lone '\r' alone.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef UInt32 OSType;
typedef char **Handle;
#define OSReadOnly 0
#define orig_stderr ((void *)0x510798)

ORIG_FN_STDCALL(void, HLock, 0x414940, (Handle h));
ORIG_FN_STDCALL(void, HUnlock, 0x414990, (Handle h));
ORIG_FN_STDCALL(SInt32, GetHandleSize, 0x4149c0, (Handle h));
ORIG_FN_STDCALL(void, DisposeHandle, 0x414900, (Handle h));
ORIG_FN(Handle, NewHandle, 0x414890, (SInt32 size));
ORIG_FN(Handle, CachedIncludeFile, 0x42a4d0, (const void *spec, Bool8 *precomp));
ORIG_FN(void, CacheIncludeFile, 0x42a380, (const void *spec, Handle h, Bool8 precomp));
ORIG_FN_STDCALL(int, OS_GetMacFileType, 0x422ee0, (const void *spec, OSType *mactype));
ORIG_FN_STDCALL(int, OS_Open, 0x411fc0, (const void *spec, int mode, int *ref));
ORIG_FN_STDCALL(int, OS_GetSize, 0x412240, (int ref, UInt32 *size));
ORIG_FN_STDCALL(int, OS_Read, 0x412180, (int ref, void *buffer, UInt32 *size));
ORIG_FN_STDCALL(int, OS_Close, 0x412220, (int ref));
ORIG_FN_STDCALL(void *, xmalloc, 0x4176a0, (const char *what, int size));
ORIG_FN(void *, orig_memcpy, 0x404d00, (void *dst, const void *src, UInt32 n));
ORIG_FN(int, orig_fprintf, 0x403bd0, (void *file, const char *fmt, ...));
ORIG_FN(void, orig_exit, 0x402180, (int code));

SInt16 FixTextHandle(Handle txt) {
    char *tptr, *scan;
    UInt32 tlen;
    HLock(txt);
    tptr = *txt;
    tlen = GetHandleSize(txt);
    scan = tptr;
    while (scan < tptr + tlen) {
        if (scan[0] == '\r') {
            if (scan[1] == '\n')
                scan += 2;
            else
                scan += 1;
        } else if (scan[0] == '\n') {
            scan[0] = '\r';
            scan++;
        } else {
            scan++;
        }
    }
    HUnlock(txt);
    return 0;
}
HOOK(0x442c50, FixTextHandle);

int LoadAndCacheFile(const void *spec, Handle *texthandle, Bool8 *precomp) {
    Handle h;
    int err, refnum;
    OSType mactype;
    UInt32 ltxtlen;

    if ((*texthandle = CachedIncludeFile(spec, precomp)))
        return 0;
    *precomp = OS_GetMacFileType(spec, &mactype) == 0 && mactype != 0x54455854; /* 'TEXT' */
    *texthandle = 0;
    err = OS_Open(spec, OSReadOnly, &refnum);
    if (err)
        return err;
    err = OS_GetSize(refnum, &ltxtlen);
    if (err)
        return err;
    h = NewHandle(ltxtlen + 1);
    if (!h) {
        orig_fprintf(orig_stderr, "\n*** Out of memory\n");
        orig_exit(-23);
    }
    HLock(h);
    err = OS_Read(refnum, *h, &ltxtlen);
    if (err) {
        DisposeHandle(h);
        OS_Close(refnum);
        return err;
    }
    OS_Close(refnum);
    (*h)[ltxtlen] = 0;
    HUnlock(h);
    if (!*precomp)
        FixTextHandle(h);
    CacheIncludeFile(spec, h, *precomp);
    *texthandle = h;
    return 0;
}
HOOK(0x442ca0, LoadAndCacheFile);

void CopyFileText(Handle src, char **text, SInt32 *textsize) {
    *textsize = GetHandleSize(src);
    *text = xmalloc(0, *textsize);
    HLock(src);
    orig_memcpy(*text, *src, *textsize);
#ifndef RECOMP_NEGATIVE_CONTROL_CLLOADANDCACHE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLLOADANDCACHE */
    *textsize -= 1;
#endif
    HUnlock(src);
}
HOOK(0x442de0, CopyFileText);
