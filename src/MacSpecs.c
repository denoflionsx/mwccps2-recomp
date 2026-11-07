/*
 * MacSpecs.c — Mac FSSpecs over Windows paths: volume and directory-ID tables, conversions.
 *
 * Port of mweppc's src/driver/MacSpecs.c (CC0, github.com/doldecomp/mweppc). All 15 functions are
 * byte-identical between their GameCube 2.0 compiler and ours. Layout and addresses from our code:
 *   DirNode (20 bytes): name +0, dirID +4, parent/volume +8, list +0xc, next +0x10
 *   VolNode (0x1c): vRefNum (u16) +0, root DirNode +4, next +0x18
 *   FSSpec (packed): vRefNum +0, parID +2, name (Pascal string, 64) +6
 *   OSSpec: path (0x104) + name (0x100) = 0x204
 * Statics are the exe's (addresses below). The static functions are cdecl, the OS_* ones __stdcall;
 * Win32 calls go through the exe's import table. Kept from the source: FindOrAdd dereferences a NULL
 * directory node if the last path component can't be added.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef struct DirNode {
    char *name;
    UInt32 dirID;
    void *p; /* parent DirNode, or the VolNode for a volume's root */
    struct DirNode *list;
    struct DirNode *next;
} DirNode;
typedef struct VolNode {
    UInt16 vRefNum;
    DirNode root; /* at +4 */
    struct VolNode *next;
} VolNode;

#define lastDirID ORIG_VAR(UInt32, 0x52cf40)
#define lastVolRef ORIG_VAR(UInt32, 0x52cf44)
#define vols ORIG_VAR(VolNode *, 0x54b8e4)
#define dirMapRows ORIG_VAR(UInt32, 0x54b4e0)
#define dirIDMap ((DirNode ***)0x54b4e4) /* [256] */
#define stvol ((char *)0x54b4a0)
#define stdir ((char *)0x54b39c)
#define stname ((char *)0x54b198)
#define STSbuf ((char *)0x57cd9c)
#define orig_stderr ((void *)0x510798)
#define OSSPEC_SIZE 0x204
#define SPEC_name(s) ((UInt8 *)(s) + 0x104)
#define FSS_vRefNum(f) (*(SInt16 *)((UInt8 *)(f) + 0))
#define FSS_parID(f) (*(SInt32 *)((UInt8 *)(f) + 2))
#define FSS_name(f) ((UInt8 *)(f) + 6)
#define OS_PATHSEP '\\'
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_DIRECTORY 0x10b

typedef UInt32(__stdcall *GetShortPathNameA_t)(const char *, char *, UInt32);
typedef UInt32(__stdcall *GetLastError_t)(void);
typedef void *(__stdcall *FindFirstFileA_t)(const char *, void *);
typedef int(__stdcall *FindClose_t)(void *);
typedef int(__stdcall *SetFileAttributesA_t)(const char *, UInt32);
#define imp_GetShortPathNameA ORIG_VAR(GetShortPathNameA_t, 0x547230)
#define imp_GetLastError ORIG_VAR(GetLastError_t, 0x5471a4)
#define imp_FindFirstFileA ORIG_VAR(FindFirstFileA_t, 0x5471d4)
#define imp_FindClose ORIG_VAR(FindClose_t, 0x5471d8)
#define imp_SetFileAttributesA ORIG_VAR(SetFileAttributesA_t, 0x547234)

ORIG_FN(void *, orig_malloc, 0x4046f0, (UInt32 size));
ORIG_FN(void *, orig_calloc, 0x404960, (UInt32 size, UInt32 n));
ORIG_FN(char *, orig_strcpy, 0x404f00, (char *d, const char *s));
ORIG_FN(char *, orig_strcat, 0x404f50, (char *d, const char *s));
ORIG_FN(char *, orig_strncpy, 0x404f20, (char *d, const char *s, UInt32 n));
ORIG_FN(void *, orig_memcpy, 0x404d00, (void *d, const void *s, UInt32 n));
ORIG_FN(int, orig_fprintf, 0x403bd0, (void *file, const char *fmt, ...));
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));
ORIG_FN_STDCALL(char *, OS_PathSpecToString, 0x413070, (const void *path, char *buf, int size));
ORIG_FN_STDCALL(char *, OS_NameSpecToString, 0x4130e0, (const void *name, char *buf, int size));
ORIG_FN_STDCALL(char *, OS_SpecToString, 0x412fc0, (const void *spec, char *buf, int size));
ORIG_FN_STDCALL(char *, OS_GetDirPtr, 0x412740, (char *path));
ORIG_FN_STDCALL(char *, OS_GetFileNamePtr, 0x419950, (char *path));
ORIG_FN_STDCALL(int, OS_EqualPath, 0x412a70, (const char *a, const char *b));
ORIG_FN_STDCALL(int, OS_MakeSpec, 0x412a90, (const char *path, void *spec, void *isfile));
ORIG_FN_STDCALL(int, OS_MakeSpec2, 0x419970, (const char *path, const char *filename, void *spec));
ORIG_FN_STDCALL(int, OS_MakeNameSpec, 0x412f30, (const char *name, void *spec));
ORIG_FN_STDCALL(int, OS_GetCWD, 0x4123d0, (void *spec));
ORIG_FN_STDCALL(int, OS_Status, 0x411e20, (const void *spec));
ORIG_FN_STDCALL(int, OS_Mkdir, 0x412310, (const void *spec));
ORIG_FN_STDCALL(int, OS_IsFile, 0x413240, (const void *spec));
ORIG_FN_STDCALL(void, c2pstrcpy, 0x40f3f0, (UInt8 *dst, const char *src));
ORIG_FN_STDCALL(void, p2cstrcpy, 0x40f430, (char *dst, const UInt8 *src));

static UInt32 cstrlen(const char *s) {
    UInt32 n = 0;
    while (s[n]) /* repne scasb in the original */
        n++;
    return n;
}

int AddDirMapEntry(DirNode *node) {
    UInt32 row = node->dirID >> 8, col = node->dirID & 0xff;
    while (row >= dirMapRows) {
        if (row >= 256) {
            orig_fprintf(orig_stderr, "Fatal error:  too many directories referenced, out of memory\n");
            return 0;
        }
        dirIDMap[row] = orig_calloc(sizeof(DirNode *), 256);
        if (!dirIDMap[row])
            return 0;
        dirMapRows++;
    }
    dirIDMap[row][col] = node;
    return 1;
}
HOOK(0x413d40, AddDirMapEntry);

VolNode *FindOrAddVolRef(VolNode **list, char *name) {
    VolNode *vn;
    while (*list) {
        if (OS_EqualPath(name, (*list)->root.name))
            return *list;
        list = &(*list)->next;
    }
    if (lastVolRef >= 256)
        return 0;
    vn = orig_malloc(sizeof(VolNode));
    if (!vn)
        return 0;
    vn->next = 0;
    vn->vRefNum = (UInt16)lastVolRef++;
    vn->root.name = orig_malloc(cstrlen(name) + 1);
    if (!vn->root.name)
        return 0;
    orig_strcpy(vn->root.name, name);
    vn->root.dirID = 2;
    vn->root.p = vn;
    vn->root.list = 0;
    vn->root.next = 0;
    *list = vn;
    return vn;
}
HOOK(0x413dd0, FindOrAddVolRef);

DirNode *FindDirMapEntry(UInt32 dirID) {
    UInt32 row = dirID >> 8, col = dirID & 0xff;
    if (!(dirID != 2))
        assertion_failed("dirID != 2", "MacSpecs.c", 166);
    if (row >= dirMapRows)
        return 0;
    return dirIDMap[row][col];
}
HOOK(0x413ea0, FindDirMapEntry);

DirNode *FindOrAddDirRef(DirNode *parent, char *name) {
    UInt32 sz;
    DirNode *dn, **list = &parent->list;
    while (*list) {
        if (OS_EqualPath(name, (*list)->name))
            return *list;
        list = &(*list)->next;
    }
    if (lastDirID >= 0x10000)
        return 0;
    sz = cstrlen(name) + 1;
    dn = orig_malloc(sizeof(DirNode));
    if (!dn)
        return 0;
    dn->name = orig_malloc(sz);
    if (!dn->name)
        return 0;
    orig_strcpy(dn->name, name);
    dn->list = 0;
    dn->next = 0;
    dn->dirID = lastDirID++;
    if (!AddDirMapEntry(dn))
        return 0;
    dn->p = parent;
    *list = dn;
    return dn;
}
HOOK(0x413ef0, FindOrAddDirRef);

int FindOrAdd(const void *path, UInt32 *vRefNum, UInt32 *dirID) {
    VolNode *vol;
    DirNode *level;
    char pb[260], voln[64], pathbuf[260];
    char *ptr, *st, ch;
    Bool8 last;

    if (!OS_PathSpecToString(path, pathbuf, 260))
        return 0;
    if (!pathbuf[0])
        return 0;
    ptr = OS_GetDirPtr(pathbuf);
    orig_strncpy(voln, pathbuf, ptr - pathbuf);
    voln[ptr - pathbuf] = 0;
    orig_strcpy(pb, ptr);
    vol = FindOrAddVolRef(&vols, voln);
    if (!vol)
        return 0;
    *vRefNum = vol->vRefNum;
    level = &vol->root;
    ptr = &pb[1];
    if (!(*pb == OS_PATHSEP))
        assertion_failed("*pb == OS_PATHSEP", "MacSpecs.c", 267);
    while (*ptr) {
        st = ptr;
        while ((ch = *ptr) && ch != OS_PATHSEP)
            ++ptr;
        last = ch == 0;
        *ptr = 0;
        level = FindOrAddDirRef(level, st);
        if (last)
            break;
        ++ptr;
        if (!level)
            return 0;
    }
    *dirID = level->dirID;
    return 1;
}
HOOK(0x413fc0, FindOrAdd);

void StepVolDir(UInt32 *vRefNum, UInt32 *dirID, DirNode **node) {
    VolNode *step;
    if (*vRefNum == 1) {
        *node = 0;
    } else if (*dirID == 2) {
        for (step = vols; step && step->vRefNum != *vRefNum; step = step->next)
            ;
        if (step) {
            *node = &step->root;
            *vRefNum = 1;
            *dirID = 1;
        } else {
            *node = 0;
        }
    } else {
        *node = FindDirMapEntry(*dirID);
        if (*node)
            *dirID = ((DirNode *)(*node)->p)->dirID;
    }
}
HOOK(0x414120, StepVolDir);

int ResolvePath(const void *path, UInt32 *vRefNum, UInt32 *dirID) {
    if (FindOrAdd(path, vRefNum, dirID)) {
        *vRefNum = -*vRefNum;
        return 1;
    }
    *vRefNum = 0;
    *dirID = 0;
    return 0;
}
HOOK(0x4141a0, ResolvePath);

int ResolveVolDir(UInt32 vRefNum, UInt32 dirID, char *vol, char *dir) {
    DirNode *nodes[256], *cur;
    int idx = 0, len;
    char *dptr;

    vRefNum = -vRefNum;
    do {
        StepVolDir(&vRefNum, &dirID, &cur);
        if (cur)
            nodes[idx++] = cur;
    } while (cur);
    if (idx) {
        orig_strcpy(vol, nodes[--idx]->name);
    } else {
        vol[0] = 0;
        dir[0] = 0;
        return 0;
    }
    dptr = dir;
    *(dptr++) = OS_PATHSEP;
    while (idx--) {
        len = cstrlen(nodes[idx]->name);
        if ((dptr - dir) + len + 1 > 260) {
            *dptr = 0;
            return 0;
        }
        orig_memcpy(dptr, nodes[idx]->name, len);
        dptr += len;
        *(dptr++) = OS_PATHSEP;
    }
    *dptr = 0;
    return 1;
}
HOOK(0x4141f0, ResolveVolDir);

int __stdcall OS_OSPathSpec_To_VolDir(const void *spec, SInt16 *vRefNum, SInt32 *dirID) {
    UInt32 vol, dir;
    if (ResolvePath(spec, &vol, &dir)) {
        *vRefNum = (SInt16)vol;
        *dirID = dir;
        return 0;
    }
    *vRefNum = 0;
    *dirID = 0;
    return ERROR_PATH_NOT_FOUND;
}
HOOK(0x414300, OS_OSPathSpec_To_VolDir);

int OS_ShortenSpec(void *spec) {
    char path[260], shortpath[260];
    orig_strcpy(path, OS_PathSpecToString(spec, STSbuf, 260));
    if (!imp_GetShortPathNameA(path, shortpath, 260))
        return imp_GetLastError();
    orig_strcpy(path, shortpath);
    orig_strcat(path, OS_NameSpecToString(SPEC_name(spec), STSbuf, 260));
    if (!imp_GetShortPathNameA(path, shortpath, 260))
        return OS_MakeSpec(path, spec, 0);
    return OS_MakeSpec(shortpath, spec, 0);
}
HOOK(0x414360, OS_ShortenSpec);

int __stdcall OS_OSSpec_To_FSSpec(const void *spec, void *fss) {
    int err;
    SInt16 vRefNum;
    SInt32 dirID;
    char *name;

    err = OS_OSPathSpec_To_VolDir(spec, &vRefNum, &dirID);
    FSS_vRefNum(fss) = vRefNum;
    FSS_parID(fss) = dirID;
#ifdef RECOMP_NEGATIVE_CONTROL_MACSPECS2 /* coarser: the wrong parent directory */
    FSS_parID(fss) = dirID + 1;
#endif
    if (err)
        return err;
    name = OS_NameSpecToString(SPEC_name(spec), STSbuf, 260);
    if (!name || cstrlen(name) >= 64) {
        UInt8 shortened[OSSPEC_SIZE];
        int i;
        for (i = 0; i < OSSPEC_SIZE; i++) /* rep movsd */
            shortened[i] = ((const UInt8 *)spec)[i];
        OS_ShortenSpec(shortened);
        name = OS_NameSpecToString(SPEC_name(shortened), STSbuf, 260);
    }
    c2pstrcpy(FSS_name(fss), name);
    return 0;
}
HOOK(0x414430, OS_OSSpec_To_FSSpec);

int __stdcall OS_VolDir_To_OSPathSpec(SInt16 vRefNum, SInt32 dirID, char *spec) {
    int err, vlen, dlen;
    if (!vRefNum || !dirID) {
        err = OS_GetCWD(spec);
        if (err)
            return err;
    } else {
        if (!ResolveVolDir(vRefNum, dirID, stvol, stdir))
            return ERROR_PATH_NOT_FOUND;
        vlen = cstrlen(stvol);
        dlen = cstrlen(stdir);
        if (vlen + dlen < 260) {
            orig_memcpy(spec, stvol, vlen);
            orig_memcpy(spec + vlen, stdir, dlen + 1);
        }
    }
    return 0;
}
HOOK(0x414500, OS_VolDir_To_OSPathSpec);

int OS_CanonNameCase(void *spec) {
    char path[260];
    UInt8 data[0x140]; /* WIN32_FIND_DATAA; cFileName at +0x2c */
    char *name, *found;
    void *handle;
    int len;

    orig_strcpy(path, OS_SpecToString(spec, STSbuf, 260));
    name = OS_GetFileNamePtr(path);
    handle = imp_FindFirstFileA(path, data);
    if (handle != (void *)-1) {
        found = (char *)data + 0x2c;
        len = cstrlen(found);
#ifndef RECOMP_NEGATIVE_CONTROL_MACSPECS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_MACSPECS */
        if ((name - path) + len + 1 < 260) {
            orig_strncpy(name, found, len);
            name[len] = 0;
        }
#endif
        imp_FindClose(handle);
    }
    return OS_MakeSpec(path, spec, 0);
}
HOOK(0x4145b0, OS_CanonNameCase);

int __stdcall OS_FSSpec_To_OSSpec(const void *fss, void *spec) {
    int err = OS_VolDir_To_OSPathSpec(FSS_vRefNum(fss), FSS_parID(fss), spec);
    if (err)
        return err;
    p2cstrcpy(stname, FSS_name(fss));
    err = OS_MakeNameSpec(stname, SPEC_name(spec));
    if (err)
        return err;
    return OS_CanonNameCase(spec);
}
HOOK(0x414660, OS_FSSpec_To_OSSpec);

int __stdcall OS_GetRsrcOSSpec(const void *spec, void *rspec, Bool8 create) {
    char dbuffer[260];
    int err;
    OS_PathSpecToString(spec, dbuffer, 260);
    err = OS_MakeSpec2(dbuffer, "RESOURCE.FRK", rspec);
    if (err)
        return err;
    err = OS_Status(rspec);
    if (err) {
        if (!create)
            return err;
        err = OS_Mkdir(rspec);
        if (err)
            return err;
        imp_SetFileAttributesA(OS_SpecToString(rspec, STSbuf, 260), 2 /* FILE_ATTRIBUTE_HIDDEN */);
        err = OS_MakeSpec2(dbuffer, "RESOURCE.FRK", rspec);
        if (err)
            return err;
    } else if (OS_IsFile(rspec)) {
        return ERROR_DIRECTORY;
    }
    err = OS_MakeNameSpec(OS_NameSpecToString(SPEC_name(spec), STSbuf, 260), SPEC_name(rspec));
    if (err)
        return err;
    return 0;
}
HOOK(0x4146c0, OS_GetRsrcOSSpec);
