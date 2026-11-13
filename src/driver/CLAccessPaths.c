/*
 * CLAccessPaths.c — access-path lists (include and library search paths), partial port.
 *
 * Port of mweppc's src/driver/CLAccessPaths.c (CC0, github.com/doldecomp/mweppc); its 18 functions
 * in our exe are byte-identical between their GameCube 2.0 compiler and ours. Only the 10 the
 * corpus runs are ported; Paths_InsertPath, Paths_RemovePath, Paths_DeletePath and the recursive
 * gather/count/copy functions stay the original's. Layout from our code: Path (16 bytes) = spec +0,
 * recursive +4, dirlist +8, flags (u16) +0xc; Paths (8) = pathsArray +0, arraySize (u16) +4,
 * pathsCount (u16) +6. Paths_GrowPaths is static in the source but a function of its own.
 */
#include "orig.h"

typedef UInt8 Bool8;
struct Paths;
typedef struct Path {
    UInt8 *spec;
    struct Paths *recursive;
    void *dirlist;
    UInt16 flags;
} Path;
typedef struct Paths {
    Path **pathsArray;
    UInt16 arraySize;
    UInt16 pathsCount;
} Paths;
#define PATHSPEC_SIZE 0x104
#ifndef NULL
#define NULL ((void *)0)
#endif

ORIG_FN_STDCALL(void *, xmalloc, 0x4176a0, (const char *what, int size));
ORIG_FN_STDCALL(void *, xrealloc, 0x417750, (const char *what, void *old, int size));
ORIG_FN_STDCALL(void, xfree, 0x417800, (void *ptr));
ORIG_FN_STDCALL(int, OS_EqualPathSpec, 0x413190, (const void *a, const void *b));
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));

#define OS_ASSERT(line, cond)                                                                      \
    do {                                                                                           \
        if (!(cond))                                                                               \
            assertion_failed(#cond, "CLAccessPaths.c", line);                                      \
    } while (0)

Bool8 Paths_Terminate(Paths *paths);

Path *Path_Init(const UInt8 *dir, Path *path) {
    int i;
    path->spec = xmalloc(0, PATHSPEC_SIZE);
    for (i = 0; i < PATHSPEC_SIZE; i++) /* *path->spec = *dir (rep movsd) */
        path->spec[i] = dir[i];
    path->recursive = 0;
    path->dirlist = 0;
    path->flags = 0;
    return path;
}
HOOK(0x428530, Path_Init);

Path *Path_New(const UInt8 *dir) {
    Path *path = xmalloc(0, sizeof(Path));
    return Path_Init(dir, path);
}
HOOK(0x428580, Path_New);

void Path_Free(Path *path) {
    if (path) {
        if (path->spec)
            xfree(path->spec);
        if (path->recursive) {
            Paths_Terminate(path->recursive);
            xfree(path->recursive);
        }
        xfree(path);
    }
}
HOOK(0x4285a0, Path_Free);

Bool8 Paths_Initialize(Paths *paths) {
    OS_ASSERT(52, paths != NULL);
    paths->pathsArray = 0; /* memset(paths, 0, 8) */
    paths->arraySize = 0;
    paths->pathsCount = 0;
    paths->pathsArray = 0;
    return 1;
}
HOOK(0x4285e0, Paths_Initialize);

Bool8 Paths_Terminate(Paths *paths) {
    UInt16 index;
    OS_ASSERT(63, paths != NULL);
    if (paths->pathsArray) {
        for (index = 0; index < paths->pathsCount; index++)
            Path_Free(paths->pathsArray[index]);
        xfree(paths->pathsArray);
    }
    paths->pathsArray = 0;
    return 1;
}
HOOK(0x428610, Paths_Terminate);

Bool8 Paths_GrowPaths(Paths *paths, UInt16 *index) {
    OS_ASSERT(84, paths != NULL);
    if (paths->pathsCount >= paths->arraySize) {
        paths->arraySize += 20;
        paths->pathsArray = xrealloc("access paths", paths->pathsArray, sizeof(Path *) * paths->arraySize);
    }
    *index = paths->pathsCount++;
    return 1;
}
HOOK(0x428660, Paths_GrowPaths);

Bool8 Paths_AddPath(Paths *paths, Path *path) {
    UInt16 ni;
#ifdef RECOMP_NEGATIVE_CONTROL_CLACCESSPATHS2 /* coarser: the path is dropped */
    return 1;
#endif
    if (Paths_GrowPaths(paths, &ni)) {
        paths->pathsArray[ni] = path;
        return 1;
    }
    return 0;
}
HOOK(0x4286c0, Paths_AddPath);

Path *Paths_GetPath(Paths *paths, UInt16 pathnum) {
    OS_ASSERT(151, paths != NULL);
    return pathnum < paths->pathsCount ? paths->pathsArray[pathnum] : 0;
}
HOOK(0x428830, Paths_GetPath);

UInt16 Paths_Count(const Paths *paths) {
    OS_ASSERT(161, paths != NULL);
    return paths->pathsCount;
}
HOOK(0x428870, Paths_Count);

Path *Paths_FindPathSpec(const Paths *paths, const void *dir) {
    UInt16 idx;
    Path *path;
    OS_ASSERT(182, paths != NULL);
    for (idx = 0; idx < paths->pathsCount; idx++) {
        path = paths->pathsArray[idx];
#ifndef RECOMP_NEGATIVE_CONTROL_CLACCESSPATHS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLACCESSPATHS */
        if (OS_EqualPathSpec(path->spec, dir))
            return path;
#endif
    }
    return 0;
}
HOOK(0x4288a0, Paths_FindPathSpec);
