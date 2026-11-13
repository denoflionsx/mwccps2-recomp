/*
 * CLFiles.c — the driver's project file list and virtual (in-memory) files, partial port.
 *
 * Port of mweppc's src/driver/CLFiles.c (CC0, github.com/doldecomp/mweppc). 13 of its functions are
 * in our exe (File_New is not), all byte-identical between their GameCube 2.0 compiler and ours.
 * Ported: the 12 the corpus runs; Files_FindFile stays the original's. Layout from our code:
 * Files = fileList +0, fileCount +4; File = next +0, filenum +4, srcfss +0x21e, textdata +0x640,
 * objectdata +0x644, browsedata +0x648; VFile (0x28 bytes) = displayName[32] +0, data +0x20,
 * next +0x24. DisposeHandle, HandAndHand (returns a short OSErr), xmalloc and xfree are __stdcall;
 * NewHandle and FixTextHandle are cdecl.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef UInt8 File;
typedef struct Files {
    File *fileList;
    SInt32 fileCount;
} Files;
typedef struct VFile {
    char displayName[32];
    void *data;
    struct VFile *next;
} VFile;
#define File_next(f) (*(File **)(f))
#define File_filenum(f) (*(SInt32 *)((f) + 4))
#define File_handle(f, off) (*(void **)((f) + (off)))
#ifndef NULL
#define NULL ((void *)0)
#endif

ORIG_FN_STDCALL(void *, xmalloc, 0x4176a0, (const char *what, int size));
ORIG_FN_STDCALL(void, xfree, 0x417800, (void *ptr));
ORIG_FN_STDCALL(void, DisposeHandle, 0x414900, (void *h));
ORIG_FN_STDCALL(SInt16, HandAndHand, 0x414a90, (void *src, void *dst));
ORIG_FN(void *, NewHandle, 0x414890, (SInt32 size));
ORIG_FN(void, FixTextHandle, 0x442c50, (void *h));
ORIG_FN(char *, orig_strncpy, 0x404f20, (char *d, const char *s, UInt32 n));
ORIG_FN(int, orig_strcmp, 0x404f80, (const char *a, const char *b));
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));

#define OS_ASSERT(line, cond)                                                                      \
    do {                                                                                           \
        if (!(cond))                                                                               \
            assertion_failed(#cond, "CLFiles.c", line);                                            \
    } while (0)

Bool8 Files_InsertFile(Files *this, File *file, SInt32 position);

void File_Free(File *file) {
    if (file) {
        if (File_handle(file, 0x640)) {
            DisposeHandle(File_handle(file, 0x640));
            File_handle(file, 0x640) = 0;
        }
        if (File_handle(file, 0x644)) {
            DisposeHandle(File_handle(file, 0x644));
            File_handle(file, 0x644) = 0;
        }
        if (File_handle(file, 0x648)) {
            DisposeHandle(File_handle(file, 0x648));
            File_handle(file, 0x648) = 0;
        }
        xfree(file);
    }
}
HOOK(0x423cb0, File_Free);

Bool8 Files_Initialize(Files *this) {
    OS_ASSERT(47, this != NULL);
    return 1;
}
HOOK(0x423d10, Files_Initialize);

Bool8 Files_Terminate(Files *this) {
    File *file, *next;
    OS_ASSERT(56, this != NULL);
    for (file = this->fileList; file; file = next) {
        next = File_next(file);
        File_Free(file);
    }
    return 1;
}
HOOK(0x423d30, Files_Terminate);

Bool8 Files_AddFile(Files *this, File *file) {
    return Files_InsertFile(this, file, this->fileCount);
}
HOOK(0x423d70, Files_AddFile);

Bool8 Files_InsertFile(Files *this, File *file, SInt32 position) {
    File **scan;
    OS_ASSERT(80, this != NULL);
    OS_ASSERT(81, file != NULL);
    if (position < 0)
        position = 0;
    else if (position > this->fileCount)
        position = this->fileCount;
#ifdef RECOMP_NEGATIVE_CONTROL_CLFILES /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLFILES */
    position = 0; /* every file goes first */
#endif
    scan = &this->fileList;
    while (position-- > 0)
        scan = &File_next(*scan);
    File_filenum(file) = this->fileCount++;
    File_next(file) = *scan;
    *scan = file;
    return 1;
}
HOOK(0x423d90, Files_InsertFile);

File *Files_GetFile(Files *this, SInt32 filenum) {
    File *file;
    OS_ASSERT(104, this != NULL);
    OS_ASSERT(105, filenum >= 0);
    file = this->fileList;
    while (file && File_filenum(file) != filenum)
        file = File_next(file);
    return file;
}
HOOK(0x423e10, Files_GetFile);

int Files_Count(Files *this) {
    OS_ASSERT(127, this != NULL);
#ifdef RECOMP_NEGATIVE_CONTROL_CLFILES2 /* coarser: no files */
    return 0;
#endif
    return this->fileCount;
}
HOOK(0x423e90, Files_Count);

Bool8 VFiles_Initialize(VFile **list) {
    *list = 0;
    return 1;
}
HOOK(0x423ec0, VFiles_Initialize);

void VFiles_Terminate(VFile **list) {
    VFile *next;
    while (*list) {
        next = (*list)->next;
        DisposeHandle((*list)->data);
        xfree(*list);
        list = &next;
    }
    *list = 0; /* clears the local, as the original does */
}
HOOK(0x423ed0, VFiles_Terminate);

VFile *VFile_New(const char *name, void *data) {
    VFile *vf = xmalloc(0, sizeof(VFile));
    if (!vf)
        return 0;
    orig_strncpy(vf->displayName, name, sizeof(vf->displayName));
    vf->displayName[sizeof(vf->displayName) - 1] = 0;
    vf->data = NewHandle(0);
    if (!vf->data || HandAndHand(data, vf->data)) {
        xfree(vf);
        return 0;
    }
    FixTextHandle(vf->data);
    vf->next = 0;
    return vf;
}
HOOK(0x423f10, VFile_New);

Bool8 VFiles_Add(VFile **list, VFile *entry) {
    VFile **scan = list;
    while (*scan)
        scan = &(*scan)->next;
    *scan = entry;
    return 1;
}
HOOK(0x423f80, VFiles_Add);

VFile *VFiles_Find(VFile *list, const char *name) {
    VFile *scan = list;
    while (scan) {
        if (!orig_strcmp(scan->displayName, name))
            break;
        scan = scan->next;
    }
    return scan;
}
HOOK(0x423fa0, VFiles_Find);
