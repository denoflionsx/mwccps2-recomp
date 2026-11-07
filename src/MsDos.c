/*
 * MsDos.c — the driver's Win32 OS layer: files, handles, path specs (mweppc src/driver/MsDos.c),
 * partial port.
 *
 * Its 59 functions in our exe are byte-identical between mweppc's GameCube 2.0 compiler and ours.
 * Ported: the file, handle, time and spec-string functions the corpus runs (part 1) and the path
 * parser (part 2: OS_GetDirPtr, OS_CanonPath, OS_IsFullPath, OS_MakeSpec, OS_MakePathSpec,
 * OS_MakeNameSpec). OS_IsFullPath keeps the original's misplaced `== 0` (the cygdrive prefix is
 * compared over 0 or 1 characters), and part 3: OS_OSErrorToMacError, OS_TimeToMac and
 * OS_LoadMacResourceFork. Part 4 adds directory reading (OS_OpenDir, OS_ReadDir, OS_CloseDir, run by one
 * case). The rest (OS_Execute, OS_SetFileTime, ...) stay the original's.
 *
 * Every OS_* function is __stdcall except OS_GetMilliseconds. Win32 calls go through the exe's own
 * import slots. OSSpec = path (0x104) + name (0x100); OSHandle = addr +0, used +4. intbuf
 * (0x54af90, 260 bytes) is the shared scratch path buffer, as in the original.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef UInt32 DWORD;
typedef void *HANDLE;
typedef struct OSHandle {
    void *addr;
    UInt32 used;
} OSHandle;
#define PATH_SIZE 0x104
#define SPEC_SIZE 0x204
#define SPEC_NAME(s) ((const char *)(s) + PATH_SIZE)
#define ERROR_ACCESS_DENIED 5
#define ERROR_NOT_ENOUGH_MEMORY 8
#define ERROR_BUFFER_OVERFLOW 111
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define GMEM_INVALID_HANDLE 0x8000

#define IMP(type, addr) (*(type *)(addr))
typedef HANDLE(__stdcall *CreateFileA_t)(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
typedef DWORD(__stdcall *GetLastError_t)(void);
typedef int(__stdcall *CloseHandle_t)(HANDLE);
typedef DWORD(__stdcall *GetFileAttributesA_t)(const char *);
typedef int(__stdcall *GetFileTime_t)(HANDLE, void *, void *, void *);
typedef DWORD(__stdcall *SetFilePointer_t)(HANDLE, SInt32, SInt32 *, DWORD);
typedef DWORD(__stdcall *GetFileSize_t)(HANDLE, DWORD *);
typedef int(__stdcall *WriteFile_t)(HANDLE, const void *, DWORD, DWORD *, void *);
typedef int(__stdcall *ReadFile_t)(HANDLE, void *, DWORD, DWORD *, void *);
typedef int(__stdcall *DeleteFileA_t)(const char *);
typedef DWORD(__stdcall *GetCurrentDirectoryA_t)(DWORD, char *);
typedef DWORD(__stdcall *GetTickCount_t)(void);
typedef void(__stdcall *GetSystemTime_t)(void *);
typedef int(__stdcall *SystemTimeToFileTime_t)(const void *, void *);
typedef void *(__stdcall *GlobalAlloc_t)(DWORD, DWORD);
typedef void *(__stdcall *GlobalReAlloc_t)(void *, DWORD, DWORD);
typedef DWORD(__stdcall *GlobalFlags_t)(void *);
typedef void *(__stdcall *GlobalFree_t)(void *);
#define CreateFileA IMP(CreateFileA_t, 0x5471b8)
#define GetLastError IMP(GetLastError_t, 0x5471a4)
#define CloseHandle IMP(CloseHandle_t, 0x5471a0)
#define GetFileAttributesA IMP(GetFileAttributesA_t, 0x5471e0)
#define GetFileTime IMP(GetFileTime_t, 0x5471e4)
#define SetFilePointer IMP(SetFilePointer_t, 0x5471a8)
#define GetFileSize IMP(GetFileSize_t, 0x5471ec)
#define WriteFile IMP(WriteFile_t, 0x547168)
#define ReadFile IMP(ReadFile_t, 0x5471ac)
#define DeleteFileA IMP(DeleteFileA_t, 0x5471b4)
#define GetCurrentDirectoryA IMP(GetCurrentDirectoryA_t, 0x54719c)
#define GetTickCount IMP(GetTickCount_t, 0x5471d0)
#define GetSystemTime IMP(GetSystemTime_t, 0x547204)
#define SystemTimeToFileTime IMP(SystemTimeToFileTime_t, 0x547208)
#define GlobalAlloc IMP(GlobalAlloc_t, 0x5471bc)
#define GlobalReAlloc IMP(GlobalReAlloc_t, 0x54720c)
#define GlobalFlags IMP(GlobalFlags_t, 0x547210)
#define GlobalFree IMP(GlobalFree_t, 0x5471c0)

ORIG_FN(char *, orig_getenv, 0x406850, (const char *name));
ORIG_FN(void *, orig_malloc, 0x4046f0, (UInt32 size));
ORIG_FN(void *, orig_memcpy, 0x404d00, (void *d, const void *s, UInt32 n));
ORIG_FN_STDCALL(int, ustrcmp, 0x4178b0, (const char *a, const char *b));
ORIG_FN_STDCALL(int, ustrncmp, 0x417900, (const char *a, const char *b, UInt32 n));
ORIG_FN(int, orig_isalpha, 0x405a70, (int c));
ORIG_FN(int, orig_toupper, 0x405ad0, (int c));
ORIG_FN(char *, orig_strchr, 0x404ff0, (const char *s, int c));
ORIG_FN(char *, orig_strrchr, 0x405020, (const char *s, int c));
ORIG_FN(char *, orig_strpbrk, 0x405060, (const char *s, const char *set));
ORIG_FN(char *, orig_strcpy, 0x404f00, (char *d, const char *s));
ORIG_FN(int, orig_sprintf, 0x403d30, (char *buf, const char *fmt, ...));
typedef DWORD(__stdcall *GetFullPathNameA_t)(const char *, DWORD, char *, char **);
#define GetFullPathNameA IMP(GetFullPathNameA_t, 0x5471c4)

#define intbuf ((char *)0x54af90)
#define cygdrive_prefix ORIG_VAR(char *, 0x54b094)
#define open_access_modes ((const DWORD *)0x52ce3c)
#define zeroes ((const char *)0x52ce4c) /* 32 zero bytes */

static UInt32 cstrlen(const char *s) {
    UInt32 n = 0;
    while (s[n]) /* inlined in the original */
        n++;
    return n;
}

char *__stdcall OS_SpecToString(const void *spec, char *path, int size);
int __stdcall OS_Close(int ref);
int __stdcall OS_MakeSpec(const char *path, void *spec, Bool8 *isfile);

int __stdcall OS_InitProgram(int *pArgc, char ***pArgv) {
    (void)pArgc;
    (void)pArgv;
    cygdrive_prefix = orig_getenv("MW_CYGDRIVE_PREFIX");
    return 0;
}
HOOK(0x411d90, OS_InitProgram);

int __stdcall OS_SetFileType(const void *spec, const void *type) {
    (void)spec;
    (void)type;
    return 0;
}
HOOK(0x411e70, OS_SetFileType);

int __stdcall OS_Create(const void *spec, const void *type) {
    HANDLE h;
    if (!OS_SpecToString(spec, intbuf, 0x104))
        return ERROR_BUFFER_OVERFLOW;
    h = CreateFileA(intbuf, 0xc0000000 /* GENERIC_READ|WRITE */, 1, 0, 2 /* CREATE_ALWAYS */, 0x80, 0);
    if (h == INVALID_HANDLE_VALUE)
        return GetLastError();
    CloseHandle(h);
    return OS_SetFileType(spec, type);
}
HOOK(0x411db0, OS_Create);

int __stdcall OS_Status(const void *spec) {
    if (!OS_SpecToString(spec, intbuf, 0x104))
        return ERROR_BUFFER_OVERFLOW;
    if (GetFileAttributesA(intbuf) == 0xffffffff)
        return GetLastError();
    return 0;
}
HOOK(0x411e20, OS_Status);

int __stdcall OS_Open(const void *spec, UInt8 mode, int *ref) {
    if (!OS_SpecToString(spec, intbuf, 0x104))
        return ERROR_BUFFER_OVERFLOW;
    *ref = (int)CreateFileA(intbuf, open_access_modes[mode], 1, 0, 3 /* OPEN_EXISTING */, 0x80, 0);
    if ((HANDLE)*ref == INVALID_HANDLE_VALUE)
        return GetLastError();
    if (mode == 3) { /* OSAppend */
        if (SetFilePointer((HANDLE)*ref, 0, 0, 2 /* FILE_END */) == 0xffffffff)
            return GetLastError();
    }
    return 0;
}
HOOK(0x411fc0, OS_Open);

int __stdcall OS_GetFileTime(const void *spec, DWORD *crtm, DWORD *chtm) {
    DWORD err, mtime[2], ctime[2];
    int ref;
    err = OS_Open(spec, 0, &ref);
    if (err)
        return err;
    err = GetFileTime((HANDLE)ref, ctime, 0, mtime) ? 0 : GetLastError();
    OS_Close(ref);
    if (chtm) {
        chtm[0] = mtime[0];
        chtm[1] = mtime[1];
    }
    if (crtm) {
        crtm[0] = ctime[0];
        crtm[1] = ctime[1];
    }
    return err;
}
HOOK(0x411e80, OS_GetFileTime);

int __stdcall OS_Write(int ref, const void *buffer, UInt32 *length) {
    HANDLE h = (HANDLE)ref;
    DWORD end, written;
    SInt32 pos, chunk;

    pos = SetFilePointer(h, 0, 0, 1 /* FILE_CURRENT */);
    if (pos == -1 || (end = GetFileSize(h, 0)) == 0xffffffff)
        return GetLastError();
    if ((DWORD)pos > end) {
        if (SetFilePointer(h, end, 0, 0 /* FILE_BEGIN */) == 0xffffffff)
            return GetLastError();
        while ((DWORD)pos > end) {
            chunk = (DWORD)(pos - end) > 32 ? 32 : pos - end;
            if (!WriteFile(h, zeroes, chunk, &written, 0))
                return GetLastError();
            if (written < (DWORD)chunk) {
                *length = 0;
                return 0;
            }
            end += chunk;
        }
    }
    if (!WriteFile(h, buffer, *length, (DWORD *)length, 0))
        return GetLastError();
    return 0;
}
HOOK(0x412060, OS_Write);

int __stdcall OS_Read(int ref, void *buffer, UInt32 *length) {
    if (!ReadFile((HANDLE)ref, buffer, *length, (DWORD *)length, 0))
        return GetLastError();
    return 0;
}
HOOK(0x412180, OS_Read);

int __stdcall OS_Close(int ref) {
    if (!CloseHandle((HANDLE)ref))
        return GetLastError();
    return 0;
}
HOOK(0x412220, OS_Close);

int __stdcall OS_GetSize(int ref, UInt32 *length) {
    *length = GetFileSize((HANDLE)ref, 0);
    if (*length == 0xffffffff)
        return GetLastError();
    return 0;
}
HOOK(0x412240, OS_GetSize);

int __stdcall OS_Delete(const void *spec) {
    if (!OS_SpecToString(spec, intbuf, 0x104))
        return ERROR_BUFFER_OVERFLOW;
    if (!DeleteFileA(intbuf))
        return GetLastError();
    return 0;
}
HOOK(0x4122d0, OS_Delete);

void ensure_trailing_backslash(char *path) {
    path += cstrlen(path);
    if (path[-1] != '\\') {
        *path = '\\';
        path[1] = 0;
    }
}
HOOK(0x4123a0, ensure_trailing_backslash);

int __stdcall OS_GetCWD(char *spec) {
    if (!GetCurrentDirectoryA(PATH_SIZE, spec))
        return GetLastError();
    ensure_trailing_backslash(spec);
    return 0;
}
HOOK(0x4123d0, OS_GetCWD);

int __stdcall OS_EqualPath(const char *a, const char *b) {
    return ustrcmp(a, b) == 0;
}
HOOK(0x412a70, OS_EqualPath);

int __stdcall OS_MakeFileSpec(const char *path, void *spec) {
    Bool8 isfile;
    int err = OS_MakeSpec(path, spec, &isfile);
    if (err)
        return err;
    if (!isfile)
        return ERROR_ACCESS_DENIED;
    return 0;
}
HOOK(0x412d30, OS_MakeFileSpec);

char *__stdcall OS_SpecToString(const void *spec, char *path, int size) {
    int nlen, plen;
    if (size == 0)
        size = PATH_SIZE;
    if (!path) {
        path = orig_malloc(size);
        if (!path)
            return 0;
    }
    plen = cstrlen(spec);
    nlen = cstrlen(SPEC_NAME(spec));
    if (plen + nlen >= size) {
        if (plen >= size) {
            nlen = 0;
            plen = size - 1;
        } else {
            nlen = size - plen - 1;
        }
    }
    orig_memcpy(path, spec, plen);
    orig_memcpy(path + plen, SPEC_NAME(spec), nlen);
    path[plen + nlen] = 0;
    return path;
}
HOOK(0x412fc0, OS_SpecToString);

static char *copy_spec_string(const char *s, char *out, int size, int defsize) {
    int len;
    if (size == 0)
        size = defsize;
    if (!out) {
        out = orig_malloc(size);
        if (!out)
            return 0;
    }
    len = cstrlen(s);
    if (len >= size)
        len = size - 1;
    orig_memcpy(out, s, len);
    out[len] = 0;
    return out;
}

char *__stdcall OS_PathSpecToString(const char *pspec, char *path, int size) {
    return copy_spec_string(pspec, path, size, PATH_SIZE);
}
HOOK(0x413070, OS_PathSpecToString);

char *__stdcall OS_NameSpecToString(const char *nspec, char *name, int size) {
    return copy_spec_string(nspec, name, size, 0x100);
}
HOOK(0x4130e0, OS_NameSpecToString);

int __stdcall OS_EqualPathSpec(const char *a, const char *b) {
    return OS_EqualPath(a, b);
}
HOOK(0x413190, OS_EqualPathSpec);

int __stdcall OS_EqualNameSpec(const char *a, const char *b) {
    return OS_EqualPath(a, b);
}
HOOK(0x4131b0, OS_EqualNameSpec);

int __stdcall OS_EqualSpec(const void *a, const void *b) {
    return OS_EqualPathSpec(a, b) && OS_EqualNameSpec(SPEC_NAME(a), SPEC_NAME(b));
}
HOOK(0x413150, OS_EqualSpec);

static int file_attributes(const void *spec, DWORD *attr) {
    int len;
    if (!OS_SpecToString(spec, intbuf, 0x104))
        return 0;
    len = cstrlen(intbuf);
    if (intbuf[len - 1] == '\\')
        intbuf[len - 1] = 0;
    *attr = GetFileAttributesA(intbuf);
    return 1;
}

int __stdcall OS_IsDir(const void *spec) {
    DWORD attr;
    if (!file_attributes(spec, &attr))
        return ERROR_BUFFER_OVERFLOW;
    if (attr == 0xffffffff)
        return 0;
    return (attr & 0x10) != 0; /* FILE_ATTRIBUTE_DIRECTORY */
}
HOOK(0x4131d0, OS_IsDir);

int __stdcall OS_IsFile(const void *spec) {
    DWORD attr;
    if (!file_attributes(spec, &attr))
        return ERROR_BUFFER_OVERFLOW;
    if (attr == 0xffffffff)
        return 0;
#ifdef RECOMP_NEGATIVE_CONTROL_MSDOS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_MSDOS */
    return 0; /* nothing is a file */
#endif
    return (attr & 0x10) == 0;
}
HOOK(0x413240, OS_IsFile);

int __stdcall OS_IsLink(const void *spec) {
    (void)spec;
    return 0;
}
HOOK(0x4132b0, OS_IsLink);

int __stdcall OS_ResolveLink(const void *link, void *target) {
    UInt32 *d = target;
    const UInt32 *s = link;
    int i;
    for (i = 0; i < SPEC_SIZE / 4; i++) /* rep movsd */
        d[i] = s[i];
    return 0;
}
HOOK(0x4132c0, OS_ResolveLink);

UInt32 OS_GetMilliseconds(void) {
    return GetTickCount();
}
HOOK(0x4135e0, OS_GetMilliseconds);

void __stdcall OS_GetTime(DWORD *p) {
    UInt16 st[8];
    DWORD ft[2];
    GetSystemTime(st);
    SystemTimeToFileTime(st, ft);
    p[0] = ft[0];
    p[1] = ft[1];
}
HOOK(0x4135f0, OS_GetTime);

int __stdcall OS_NewHandle(UInt32 size, OSHandle *hand) {
    UInt32 i;
    hand->addr = GlobalAlloc(0 /* GMEM_FIXED */, size);
    hand->used = size;
    if (!hand->addr)
        return GetLastError();
    for (i = 0; i < size; i++) /* memset, inlined in the original */
        ((UInt8 *)hand->addr)[i] = 0;
    return 0;
}
HOOK(0x413630, OS_NewHandle);

int __stdcall OS_ResizeHandle(OSHandle *hand, UInt32 size) {
    UInt8 *h = GlobalReAlloc(hand->addr, size, 2 /* GMEM_MOVEABLE */);
    UInt32 i;
    if (!h) {
        hand->addr = 0;
        hand->used = 0;
        return GetLastError();
    }
    if (size > hand->used)
        for (i = hand->used; i < size; i++) /* memset, inlined in the original */
            h[i] = 0;
    hand->addr = h;
    hand->used = size;
    return 0;
}
HOOK(0x413670, OS_ResizeHandle);

void *__stdcall OS_LockHandle(OSHandle *hand) {
    if (GlobalFlags(hand->addr) != GMEM_INVALID_HANDLE)
        return hand->addr;
    return 0;
}
HOOK(0x4136d0, OS_LockHandle);

void __stdcall OS_UnlockHandle(OSHandle *hand) {
    (void)hand;
}
HOOK(0x413700, OS_UnlockHandle);

int __stdcall OS_FreeHandle(OSHandle *hand) {
    if (GlobalFree(hand->addr))
        return GetLastError();
    hand->addr = 0;
    hand->used = 0;
    return 0;
}
HOOK(0x413710, OS_FreeHandle);

int __stdcall OS_GetHandleSize(OSHandle *hand, UInt32 *size) {
    if (GlobalFlags(hand->addr) != GMEM_INVALID_HANDLE) {
        *size = hand->used;
        return 0;
    }
    *size = 0;
    return ERROR_NOT_ENOUGH_MEMORY;
}
HOOK(0x413750, OS_GetHandleSize);

void __stdcall OS_InvalidateHandle(OSHandle *hand) {
    hand->addr = 0;
    hand->used = 0;
}
HOOK(0x413790, OS_InvalidateHandle);

Bool8 __stdcall OS_ValidHandle(OSHandle *hand) {
    return hand && hand->addr;
}
HOOK(0x4137b0, OS_ValidHandle);

/* ---- part 2: path parsing ---- */

char *__stdcall OS_GetDirPtr(char *path) {
    char *sep;
    if (*path == '\\' && path[1] == '\\') {
        sep = orig_strchr(path + 2, '\\');
        if (!sep)
            sep = path + cstrlen(path);
        return sep;
    }
    if (orig_isalpha(*path) && path[1] == ':')
        return path + 2;
    return path;
}
HOOK(0x412740, OS_GetDirPtr);

int OS_CanonPath(const char *src, char *dst) {
    const char *read, *rest;
    char *write, ch;

    if (cstrlen(src) > 259)
        return ERROR_BUFFER_OVERFLOW;
    if (!dst)
        dst = (char *)src;
    read = src;
    write = dst;
    if (cygdrive_prefix && ustrncmp(cygdrive_prefix, src, cstrlen(cygdrive_prefix)) == 0) {
        rest = src + cstrlen(cygdrive_prefix);
        if (*rest == '/' || *rest == '\\')
            rest++;
        if (orig_isalpha(*rest) && (rest[1] == '/' || rest[1] == '\\')) {
            dst[0] = (char)orig_toupper(*rest);
            dst[1] = ':';
            dst[2] = '\\';
            write += 3;
            rest += 2;
            read = rest;
        }
    }
    if (read[0] == '/' && read[1] == '/' && orig_isalpha(read[2]) && (read[3] == '/' || read[3] == 0)) {
        write[0] = read[2];
        write[1] = ':';
        write += 2;
        read += 3;
    } else if (ustrncmp(read, "/cygdrive/", 10) == 0) {
        if (orig_isalpha(read[10]) && (read[11] == '/' || read[11] == 0)) {
            write[0] = read[10];
            write[1] = ':';
            write += 2;
            read += 11;
        }
    }
    if (orig_isalpha(*read) && read[1] == ':') {
        write[0] = (char)orig_toupper(*read);
        write[1] = ':';
        write += 2;
        read += 2;
    }
    while ((ch = *read) != 0) {
#ifdef RECOMP_NEGATIVE_CONTROL_MSDOS2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_MSDOS2 */
        *write = ch;
#else
        *write = ch == '/' ? '\\' : ch;
#endif
        write++;
        read++;
    }
    *write = 0;
    return 0;
}
HOOK(0x4127c0, OS_CanonPath);

int __stdcall OS_IsFullPath(const char *path) {
    int cyg = 1, prefixed = 1, dos = 1, unc, rooted, drive, matched, slashes, letter;

    unc = path[0] == '\\' && path[1] == '\\';
    if (!unc) {
        rooted = 0;
        drive = 0;
        if (orig_isalpha(path[0]) && path[1] == ':')
            drive = 1;
        if (drive && path[2] == '\\')
            rooted = 1;
        if (!rooted)
            dos = 0;
    }
    if (!dos) {
        matched = 0;
        if (cygdrive_prefix && ustrncmp(path, cygdrive_prefix, cstrlen(cygdrive_prefix) == 0))
            matched = 1;
        if (!matched)
            prefixed = 0;
    }
    if (!prefixed) {
        rooted = 0;
        letter = 0;
        slashes = path[0] == '/' && path[1] == '/';
        if (slashes && orig_isalpha(path[2]))
            letter = 1;
        if (letter && path[3] == '/')
            rooted = 1;
        if (!rooted)
            cyg = 0;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_MSDOS3 /* coarser: no path is full */
    cyg = 0;
#endif
    return !!cyg;
}
HOOK(0x412950, OS_IsFullPath);

static int bytes_differ(const char *a, const char *b, int n) { /* memcmp, the original's repe cmpsb */
    int i;
    for (i = 0; i < n; i++)
        if (a[i] != b[i])
            return 1;
    return 0;
}

int __stdcall OS_MakeSpec(const char *path, void *spec, Bool8 *isfile) {
    const char *ptr;
    char canon[260], full[260], *name, *end, *sp = spec, *sn = (char *)spec + PATH_SIZE;
    DWORD attr;
    int err, len;

    sp[0] = 0;
    sn[0] = sp[0];
    if ((err = OS_CanonPath(path, canon)) != 0)
        return err;
    if (GetFullPathNameA(canon, sizeof(full), full, &name) == 0)
        return GetLastError();

    end = full + cstrlen(full) - 1;
    ptr = orig_strrchr(path, '\\');
    ptr = ptr ? ptr + 1 : path;
    if (bytes_differ(ptr, "..", 3) && bytes_differ(ptr, ".", 2) && ptr[cstrlen(ptr) - 1] == '.' && *end != '.') {
        *++end = '.';
        end[1] = 0;
    }

    attr = GetFileAttributesA(full);
    if (attr != 0xffffffff) {
        if (attr & 0x10) {
            if (isfile)
                *isfile = 0;
            name = 0;
        } else if (isfile) {
            *isfile = 1;
        }
        err = 0;
    } else {
        err = GetLastError();
        if (err && err != 2 /* ERROR_FILE_NOT_FOUND */ && err != 3 /* ERROR_PATH_NOT_FOUND */)
            return err;
        if (err != 3)
            err = 0;
        if (isfile)
            *isfile = 1;
    }

    if (!name) {
        name = end + 1;
        if (name[-1] == '.')
            name--;
        if (name[-1] != '\\') {
            *name = '\\';
            name++;
            *name = 0;
        }
    }
    len = name - full;
    if (len >= PATH_SIZE) {
        sp[0] = 0;
        return ERROR_BUFFER_OVERFLOW;
    }
    orig_memcpy(sp, full, len);
    sp[len] = 0;
    len = cstrlen(name);
    if (len >= 0x100) {
        sn[0] = 0;
        return ERROR_BUFFER_OVERFLOW;
    }
    orig_memcpy(sn, name, len + 1);
    return err;
}
HOOK(0x412a90, OS_MakeSpec);

int __stdcall OS_MakePathSpec(const char *vol, const char *dir, char *spec) {
    UInt8 fullspec[SPEC_SIZE];
    Bool8 isfile;
    char path[324];
    UInt32 vlen, dlen;
    int err;

    dlen = dir ? cstrlen(dir) : 0;
    vlen = vol ? cstrlen(vol) : 0;
    if (vlen + dlen + 2 > sizeof(path))
        return ERROR_BUFFER_OVERFLOW;
    if (vol) {
        if (vol[0] == 0) {
            if (!dir)
                dir = "\\";
            orig_strcpy(path, dir);
        } else if (vol[1] == 0) {
            orig_sprintf(path, "%s:%s", vol, dir ? dir : "\\");
        } else if (dir) {
            orig_sprintf(path, "%s%s%s", vol, dir[0] == '\\' ? "" : "\\", dir);
        } else {
            orig_sprintf(path, "%s", vol);
        }
    } else if (dir) {
        orig_strcpy(path, dir);
    } else {
        path[0] = '.'; /* strcpy(path, "."), a movsw in the original */
        path[1] = 0;
    }
    err = OS_MakeSpec(path, fullspec, &isfile);
    orig_strcpy(spec, (char *)fullspec);
    if (err == 0 && OS_Status(fullspec) != 0) {
        err = 3; /* ERROR_PATH_NOT_FOUND */
        isfile = 0;
    }
    if (err)
        return err;
    if (isfile)
        return 267; /* ERROR_DIRECTORY */
    return 0;
}
HOOK(0x412d70, OS_MakePathSpec);

int __stdcall OS_MakeNameSpec(const char *name, char *spec) {
    int len = cstrlen(name);
    spec[0] = 0;
    if (len > 255)
        return ERROR_BUFFER_OVERFLOW;
    if (orig_strchr(name, '\\'))
        return ERROR_ACCESS_DENIED;
    if (orig_strpbrk(name, "<>:\"/\\|"))
        return 123; /* ERROR_INVALID_NAME */
    orig_memcpy(spec, name, len + 1);
    return 0;
}
HOOK(0x412f30, OS_MakeNameSpec);

/* ---- part 3: error codes, Mac time, the resource fork ---- */

typedef int(__stdcall *FileTimeToSystemTime_t)(const void *, UInt16 *);
typedef void *(__stdcall *GetModuleHandleA_t)(const char *);
typedef void *(__stdcall *FindResourceA_t)(void *, const char *, const char *);
typedef void *(__stdcall *LoadResource_t)(void *, void *);
typedef void *(__stdcall *LockResource_t)(void *);
typedef DWORD(__stdcall *SizeofResource_t)(void *, void *);
#define FileTimeToSystemTime IMP(FileTimeToSystemTime_t, 0x547214)
#define GetModuleHandleA IMP(GetModuleHandleA_t, 0x547218)
#define FindResourceA IMP(FindResourceA_t, 0x54721c)
#define LoadResource IMP(LoadResource_t, 0x547220)
#define LockResource IMP(LockResource_t, 0x547224)
#define SizeofResource IMP(SizeofResource_t, 0x547228)
#define monthdays ((SInt32 *)0x52cee4) /* 12 entries; [1] is set to 28 or 29 */

/* Win32 error -> Mac OSErr; the cases are the original's (from our code). */
int __stdcall OS_OSErrorToMacError(int err) {
    switch (err) {
    case 0:
        return 0;
    case 2: /* ERROR_FILE_NOT_FOUND */
#ifdef RECOMP_NEGATIVE_CONTROL_MSDOS5 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_MSDOS5 */
        return -36; /* ioErr instead */
#endif
        return -43; /* fnfErr */
    case 3:   /* ERROR_PATH_NOT_FOUND */
    case 267: /* ERROR_DIRECTORY */
        return -120; /* dirNFErr */
    case 4:      /* ERROR_TOO_MANY_OPEN_FILES */
        return -42; /* tmfoErr */
    case 5:  /* ERROR_ACCESS_DENIED */
    case 32: /* ERROR_SHARING_VIOLATION */
    case 33: /* ERROR_LOCK_VIOLATION */
        return -54; /* permErr */
    case 6:      /* ERROR_INVALID_HANDLE */
        return -51; /* rfNumErr */
    case 8:  /* ERROR_NOT_ENOUGH_MEMORY */
    case 14: /* ERROR_OUTOFMEMORY */
    case 36: /* ERROR_SHARING_BUFFER_EXCEEDED */
        return -108; /* memFullErr */
    case 13: /* ERROR_INVALID_DATA */
    case 16: /* ERROR_CURRENT_DIRECTORY */
    case 24: /* ERROR_BAD_LENGTH */
    case 87: /* ERROR_INVALID_PARAMETER */
        return -50; /* paramErr */
    case 15:     /* ERROR_INVALID_DRIVE */
        return -35; /* nsvErr */
    case 19:     /* ERROR_WRITE_PROTECT */
        return -61; /* wPrErr */
    case 21: /* ERROR_NOT_READY */
    case 23: /* ERROR_CRC */
    case 25: /* ERROR_SEEK */
    case 27: /* ERROR_SECTOR_NOT_FOUND */
    case 28: /* ERROR_OUT_OF_PAPER */
    case 29: /* ERROR_WRITE_FAULT */
    case 30: /* ERROR_READ_FAULT */
    case 31: /* ERROR_GEN_FAILURE */
        return -36; /* ioErr */
    case 38:     /* ERROR_HANDLE_EOF */
        return -39; /* eofErr */
    case 39:     /* ERROR_HANDLE_DISK_FULL */
        return -34; /* dskFulErr */
    case 183:    /* ERROR_ALREADY_EXISTS */
        return -48; /* dupFNErr */
    case 123:    /* ERROR_INVALID_NAME */
        return -37; /* bdNamErr */
    }
    return err | 0xffff8000;
}
HOOK(0x4137d0, OS_OSErrorToMacError);

/* A FILETIME as seconds since 1904-01-01, by the original's own calendar arithmetic. */
void __stdcall OS_TimeToMac(DWORD lo, DWORD hi, UInt32 *secs) {
    UInt16 st[8]; /* wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds */
    DWORD ft[2];
    SInt32 days, year;
    int wYear;

    ft[0] = lo;
    ft[1] = hi;
    FileTimeToSystemTime(ft, st);
    wYear = st[0];
    year = wYear - 1904;
    days = year * 365;
    days += (year + 3) / 4 - (year + 4) / 100 + (year - 296) / 400;
    if (wYear % 4 == 0 && (wYear % 100 != 0 || wYear % 400 == 0))
        monthdays[1] = 29;
    else
        monthdays[1] = 28;
    if (st[1] > 12 || st[1] == 0)
        st[1] = 1;
    while (--st[1] != 0)
        days += monthdays[st[1] - 1];
    days += st[3] - 1;
#ifdef RECOMP_NEGATIVE_CONTROL_MSDOS4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_MSDOS4 */
    days += 1; /* a day late */
#endif
    *secs = days * 86400 + st[4] * 3600 + st[5] * 60 + st[6];
}
HOOK(0x413920, OS_TimeToMac);

int __stdcall OS_LoadMacResourceFork(const void *spec, void **file_data, SInt32 *file_len) {
    void *mod, *rsrc, *h, *data;
    if (!OS_SpecToString(spec, intbuf, 0x104))
        return ERROR_BUFFER_OVERFLOW;
    mod = GetModuleHandleA(intbuf);
    if (!mod)
        return GetLastError();
    rsrc = FindResourceA(mod, "#101", "MACRSRC");
    if (!rsrc)
        return GetLastError();
    h = LoadResource(mod, rsrc);
    if (!h)
        return GetLastError();
    data = LockResource(h);
    if (!data)
        return GetLastError();
    *file_data = data;
    *file_len = SizeofResource(mod, rsrc);
    return 0;
}
HOOK(0x413c10, OS_LoadMacResourceFork);

/* ---- part 4: directory reading ---- */

/* OSOpenedDir: dir (HANDLE) +0, data (WIN32_FIND_DATAA, 0x140 bytes) +4, spec (OSPathSpec) +8.
   WIN32_FIND_DATAA: dwFileAttributes +0, cFileName +0x2c, cAlternateFileName +0x130. Run by the
   one corpus case that lists a directory. */
typedef HANDLE(__stdcall *FindFirstFileA_t)(const char *, void *);
typedef int(__stdcall *FindNextFileA_t)(HANDLE, void *);
typedef int(__stdcall *FindClose_t)(HANDLE);
#define FindFirstFileA IMP(FindFirstFileA_t, 0x5471d4)
#define FindNextFileA IMP(FindNextFileA_t, 0x547200)
#define FindClose IMP(FindClose_t, 0x5471d8)
ORIG_FN(char *, orig_strcat_, 0x404f50, (char *d, const char *s));
ORIG_FN(void, orig_free_, 0x404730, (void *p));

typedef struct OSOpenedDir {
    HANDLE dir;
    UInt8 *data;
    char spec[PATH_SIZE];
} OSOpenedDir;

int __stdcall OS_CloseDir(OSOpenedDir *ref);

int __stdcall OS_OpenDir(const char *spec, OSOpenedDir *ref) {
    char pattern[260];

    ref->data = orig_malloc(0x140);
    if (!ref->data)
        return ERROR_NOT_ENOUGH_MEMORY;
    {
        int i;
        for (i = 0; i < PATH_SIZE / 4; i++) /* ref->spec = *spec, a rep movsd */
            ((UInt32 *)ref->spec)[i] = ((const UInt32 *)spec)[i];
    }
    orig_strcpy(pattern, spec);
    orig_strcat_(pattern, (const char *)0x52cee0 /* "*" */);
#ifdef RECOMP_NEGATIVE_CONTROL_MSDOS4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_MSDOS4 */
    return 2;
#endif
    ref->dir = FindFirstFileA(pattern, ref->data);
    if (ref->dir == INVALID_HANDLE_VALUE)
        return GetLastError();
    return 0;
}
HOOK(0x4132e0, OS_OpenDir);

int __stdcall OS_ReadDir(OSOpenedDir *ref, char *spec, char *filename, Bool8 *isfile) {
    DWORD attr;
    UInt8 *fd = ref->data;
    char name[260];
    UInt32 len, plen;

    for (;;) {
        if (ref->dir == INVALID_HANDLE_VALUE)
            return 2; /* ERROR_FILE_NOT_FOUND */
        len = cstrlen((char *)fd + 0x2c);
        if ((int)len + 1 < 256) {
            orig_memcpy(name, fd + 0x2c, len);
        } else {
            len = cstrlen((char *)fd + 0x130);
            if ((int)len > 256)
                len = 255;
            orig_memcpy(name, fd + 0x130, len);
        }
        name[len] = 0;
        attr = *(DWORD *)fd;
        if (!FindNextFileA(ref->dir, fd))
            OS_CloseDir(ref);
        if (name[0] == '.' && name[1] == 0) /* memcmp(name, ".", 2) */
            continue;
        if (name[0] == '.' && name[1] == '.' && name[2] == 0) /* memcmp(name, "..", 3) */
            continue;
        attr &= 0x10; /* FILE_ATTRIBUTE_DIRECTORY */
        if (cstrlen(ref->spec) + len + (attr != 0) >= 260)
            continue;
        if (attr) {
            plen = cstrlen(ref->spec);
            orig_memcpy(spec, ref->spec, plen);
            orig_memcpy(spec + plen, name, len);
            spec[plen + len] = '\\';
            spec[plen + len + 1] = 0;
            spec[PATH_SIZE] = 0;
            *isfile = 0;
        } else {
            orig_strcpy(spec, ref->spec);
            orig_memcpy(spec + PATH_SIZE, name, len + 1);
            *isfile = 1;
        }
        orig_memcpy(filename, name, len + 1);
        return 0;
    }
}
HOOK(0x413390, OS_ReadDir);

int __stdcall OS_CloseDir(OSOpenedDir *ref) {
    if (ref->dir != INVALID_HANDLE_VALUE) {
        if (!FindClose(ref->dir))
            return GetLastError();
        if (ref->data)
            orig_free_(ref->data);
        ref->data = 0;
        ref->dir = INVALID_HANDLE_VALUE;
    }
    return 0;
}
HOOK(0x4135a0, OS_CloseDir);
