/*
 * Generic.c — path and handle utilities of the driver (partial port).
 *
 * Port of mweppc's src/driver/Generic.c (CC0, github.com/doldecomp/mweppc); its 12 functions in our
 * exe are byte-identical between their GameCube 2.0 compiler and ours. Only the 7 the corpus runs
 * are ported; WildCardMatch, OS_MatchPath, OS_NameSpecChangeExtension, OS_NameSpecSetExtension and
 * OS_FindFileInPath stay the original's until a test reaches them. All OS_* functions are
 * __stdcall. OSPathSpec is 0x104 bytes, OSSpec = path + name (0x100).
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef UInt32 size_t;
#define OS_PATHSEP '\\'
#define ERROR_BUFFER_OVERFLOW 111
#define PATHSPEC_SIZE 0x104
#define SPEC_name(s) ((UInt8 *)(s) + 0x104)

typedef UInt32(__stdcall *GetDirectoryA_t)(char *buf, UInt32 size);
#define imp_GetSystemDirectoryA ORIG_VAR(GetDirectoryA_t, 0x547240)
#define imp_GetWindowsDirectoryA ORIG_VAR(GetDirectoryA_t, 0x547244)

ORIG_FN(char *, orig_strcpy, 0x404f00, (char *d, const char *s));
ORIG_FN(char *, orig_strncpy, 0x404f20, (char *d, const char *s, UInt32 n));
ORIG_FN(char *, orig_strchr, 0x404ff0, (const char *s, int c));
ORIG_FN(char *, orig_strrchr, 0x405020, (const char *s, int c));
ORIG_FN(char *, orig_strpbrk, 0x405060, (const char *s, const char *set));
ORIG_FN(void *, orig_memcpy, 0x404d00, (void *d, const void *s, UInt32 n));
ORIG_FN(void *, orig_malloc, 0x4046f0, (UInt32 size));
ORIG_FN(int, orig_tolower, 0x405ab0, (int c));
ORIG_FN(int, orig_sprintf, 0x403d30, (char *buf, const char *fmt, ...));
ORIG_FN(char *, orig_getenv, 0x406850, (const char *name));
ORIG_FN_STDCALL(char *, strcatn, 0x417820, (char *d, const char *s, SInt32 max));
ORIG_FN_STDCALL(int, ustrcmp, 0x4178b0, (const char *a, const char *b));
ORIG_FN_STDCALL(int, OS_MakeSpec, 0x412a90, (const char *path, void *spec, void *isfile));
ORIG_FN_STDCALL(int, OS_MakeFileSpec, 0x412d30, (const char *path, void *spec));
ORIG_FN_STDCALL(int, OS_MakeNameSpec, 0x412f30, (const char *name, void *spec));
ORIG_FN_STDCALL(int, OS_IsFullPath, 0x412950, (const char *path));
ORIG_FN_STDCALL(int, OS_GetCWD, 0x4123d0, (void *spec));
ORIG_FN_STDCALL(int, OS_Status, 0x411e20, (const void *spec));
ORIG_FN_STDCALL(char *, OS_SpecToString, 0x412fc0, (const void *spec, char *buf, int size));
ORIG_FN_STDCALL(char *, OS_PathSpecToString, 0x413070, (const void *path, char *buf, int size));
ORIG_FN_STDCALL(int, OS_FindFileInPath, 0x419f90, (const char *filename, const char *plist, void *spec));
ORIG_FN_STDCALL(int, OS_NewHandle, 0x413630, (UInt32 size, void *hand));
ORIG_FN_STDCALL(int, OS_ResizeHandle, 0x413670, (void *hand, UInt32 size));
ORIG_FN_STDCALL(void *, OS_LockHandle, 0x4136d0, (void *hand));
ORIG_FN_STDCALL(void, OS_UnlockHandle, 0x413700, (void *hand));
ORIG_FN_STDCALL(int, OS_FreeHandle, 0x413710, (void *hand));
ORIG_FN_STDCALL(int, OS_GetHandleSize, 0x413750, (void *hand, UInt32 *size));

static size_t cstrlen(const char *s) {
    size_t n = 0;
    while (s[n]) /* inlined in the original */
        n++;
    return n;
}

char *__stdcall OS_GetFileNamePtr(char *path) {
    char *ptr = orig_strrchr(path, OS_PATHSEP);
#ifdef RECOMP_NEGATIVE_CONTROL_GENERIC2 /* coarser: the whole path */
    return path;
#endif
    return ptr ? ptr + 1 : path;
}
HOOK(0x419950, OS_GetFileNamePtr);

int __stdcall OS_MakeSpec2(const char *path, const char *filename, void *spec) {
    char bpath[260], *eptr;
    int pthlen, fnlen;
    if (!path)
        path = "";
    if (!filename)
        filename = "";
    fnlen = cstrlen(filename);
    pthlen = cstrlen(path);
    if (fnlen + pthlen + 1 > 259)
        return ERROR_BUFFER_OVERFLOW;
    orig_strncpy(bpath, path, pthlen);
    eptr = bpath + pthlen;
#ifndef RECOMP_NEGATIVE_CONTROL_GENERIC /* --cflags=-DRECOMP_NEGATIVE_CONTROL_GENERIC */
    if (eptr[-1] != OS_PATHSEP)
        *(eptr++) = OS_PATHSEP;
#endif
    orig_strcpy(eptr, filename);
    return OS_MakeSpec(bpath, spec, 0);
}
HOOK(0x419970, OS_MakeSpec2);

int __stdcall OS_MakeSpecWithPath(const UInt8 *path, const char *filename, Bool8 noRelative, UInt8 *spec) {
    Bool8 relpath = filename && orig_strpbrk(filename, "/\\:");
    char buf[260], *mptr, *eptr;
    int i;

    if (!filename) {
        if (path)
            for (i = 0; i < PATHSPEC_SIZE; i++) /* spec->path = *path */
                spec[i] = path[i];
        else
            OS_GetCWD(spec);
        return OS_MakeNameSpec("", SPEC_name(spec));
    }
    if ((!noRelative || !relpath) && !OS_IsFullPath(filename)) {
        if (path)
            OS_PathSpecToString(path, buf, 260);
        else
            buf[0] = 0;
        mptr = &buf[259] - cstrlen(filename);
        eptr = &buf[cstrlen(buf)];
        orig_strcpy(eptr > mptr ? mptr : eptr, filename);
        return OS_MakeSpec(buf, spec, 0);
    }
    return OS_MakeSpec(filename, spec, 0);
}
HOOK(0x419a20, OS_MakeSpecWithPath);

char *__stdcall OS_SpecToStringRelative(const void *spec, const void *cwdspec, char *path, int size) {
    char fullbuf[260], *full = fullbuf, cwdbuf[260], *cwd = cwdbuf, *pptr;
    UInt8 mycwdspec[PATHSPEC_SIZE];

    OS_SpecToString(spec, full, 260);
    if (!size)
        size = 260;
    if (!path) {
        path = orig_malloc(size);
        if (!path)
            return 0;
    }
    if (!cwdspec) {
        OS_GetCWD(mycwdspec);
        cwdspec = mycwdspec;
    }
    if (!OS_PathSpecToString(cwdspec, cwdbuf, 260)) {
        orig_memcpy(path, fullbuf, size - 1);
        path[size - 1] = 0;
        return path;
    }
    while (*cwd && orig_tolower(*full) == orig_tolower(*cwd)) {
        full++;
        cwd++;
    }
    if ((size_t)(cwd - cwdbuf) < cstrlen(fullbuf) / 2 && cstrlen(cwd) > cstrlen(fullbuf) / 2) {
        orig_memcpy(path, fullbuf, size - 1);
        path[size - 1] = 0;
        return path;
    }
    while (cwd > cwdbuf) {
        full--;
        cwd--;
        if (*cwd == OS_PATHSEP)
            break;
    }
    if (cwd == cwdbuf) {
        orig_strncpy(path, full, size - 1);
        path[size - 1] = 0;
    } else if (*(++cwd)) {
        pptr = path;
        while (*cwd) {
            if (*cwd == OS_PATHSEP)
                pptr += orig_sprintf(pptr, "..\\");
            ++cwd;
        }
        orig_strcpy(pptr, full + 1);
    } else {
        orig_strncpy(path, full + 1, size - 1);
        path[size - 1] = 0;
    }
    return path;
}
HOOK(0x419d70, OS_SpecToStringRelative);

int __stdcall OS_FindProgram(const char *filename, void *spec) {
    char exename[260], dir[260], *plist;
    int err;

    orig_strncpy(exename, filename, 260);
    exename[259] = 0;
    if (cstrlen(exename) < 4 || ustrcmp(exename + cstrlen(exename) - 4, ".exe"))
        strcatn(exename, ".exe", 260);
    if (!orig_strchr(exename, OS_PATHSEP)) {
        if (imp_GetSystemDirectoryA(dir, 260))
            if (!OS_MakeSpec2(dir, exename, spec))
                if (!(err = OS_Status(spec)))
                    return err;
        if (imp_GetWindowsDirectoryA(dir, 260))
            if (!OS_MakeSpec2(dir, exename, spec))
                if (!(err = OS_Status(spec)))
                    return err;
        plist = orig_getenv("PATH");
        if (!OS_FindFileInPath(exename, plist, spec))
            return 0;
    }
    err = OS_MakeFileSpec(exename, spec);
    if (!err)
        err = OS_Status(spec);
    return err;
}
HOOK(0x41a080, OS_FindProgram);

int __stdcall OS_CopyHandle(void *hand, void *copy) {
    int err;
    UInt32 sz;
    void *from, *to;
    err = OS_GetHandleSize(hand, &sz);
    if (!err) {
        err = OS_NewHandle(sz, copy);
        if (!err) {
            from = OS_LockHandle(hand);
            to = OS_LockHandle(copy);
            orig_memcpy(to, from, sz);
            OS_UnlockHandle(hand);
            OS_UnlockHandle(copy);
            return 0;
        }
    }
    OS_FreeHandle(copy);
    return err;
}
HOOK(0x41a1f0, OS_CopyHandle);

int __stdcall OS_AppendHandle(void *hand, const void *data, UInt32 len) {
    int err;
    UInt32 sz;
    void *buffer;
    err = OS_GetHandleSize(hand, &sz);
    if (!err) {
        err = OS_ResizeHandle(hand, sz + len);
        if (!err) {
            buffer = OS_LockHandle(hand);
            if (buffer) {
                orig_memcpy((UInt8 *)buffer + sz, data, len);
                OS_UnlockHandle(hand);
                return 0;
            }
        }
    }
    return err;
}
HOOK(0x41a270, OS_AppendHandle);
