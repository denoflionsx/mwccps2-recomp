/*
 * MacFileTypes.c — Mac file types for files: type mapping lists, magic-number detection.
 *
 * Port of mweppc's src/driver/MacFileTypes.c (CC0, github.com/doldecomp/mweppc). All 6 functions
 * are byte-identical between their GameCube 2.0 compiler and ours. All are __stdcall. Layout from
 * our functions' code: the mapping list is packed (count, then the array pointer at +2) and each
 * mapping is 14 bytes with a signed byte length at +8. The original inlines memset (stosd) and
 * memcmp (repe cmpsb).
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef UInt32 OSType;
typedef UInt32 uOSTypePair; /* 4 bytes here */

#pragma pack(push, 2)
typedef struct OSFileTypeMapping { /* 14 bytes */
    OSType mactype;
    const char *magic;
    SInt8 length;
    UInt8 rest[5];
} OSFileTypeMapping;
typedef struct OSFileTypeMappingList {
    SInt16 numMappings;
    const OSFileTypeMapping *mappings; /* at +2 */
} OSFileTypeMappingList;
#pragma pack(pop)

typedef struct OSFileTypeMappings { /* 8 bytes */
    const OSFileTypeMappingList *mappingList;
    struct OSFileTypeMappings *next;
} OSFileTypeMappings;

#define fmList ORIG_VAR(OSFileTypeMappings **, 0x530038) /* static, = &defaultList */
typedef int(__stdcall *ExtendedHook)(const void *spec, OSType *mactype);
#define OS_ExtendedGetMacFileTypeHook ORIG_VAR(ExtendedHook, 0x57c7a8)
#define OS_TEXTTYPE ORIG_VAR(uOSTypePair, 0x57b6c0)
#define OSReadOnly 0

ORIG_FN(void *, orig_malloc, 0x4046f0, (UInt32 size));
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));
ORIG_FN_STDCALL(int, OS_SetFileType, 0x411e70, (const void *spec, const uOSTypePair *type));
ORIG_FN_STDCALL(int, OS_Open, 0x411fc0, (const void *spec, int mode, int *ref));
ORIG_FN_STDCALL(int, OS_GetSize, 0x412240, (int ref, UInt32 *size));
ORIG_FN_STDCALL(int, OS_Read, 0x412180, (int ref, void *buffer, UInt32 *size));
ORIG_FN_STDCALL(int, OS_Close, 0x412220, (int ref));
ORIG_FN_STDCALL(int, OS_GetRsrcOSSpec, 0x4146c0, (const void *spec, void *rsrc, Bool8 create));

void __stdcall OS_AddFileTypeMappingList(OSFileTypeMappings **list, const OSFileTypeMappingList *entry) {
    OSFileTypeMappings **scan;
    if (!list)
        list = fmList;
    scan = list;
    while (*scan)
        scan = &(*scan)->next;
    *scan = orig_malloc(sizeof(OSFileTypeMappings));
    if (!(*scan != (void *)0))
        assertion_failed("*scan != NULL", "MacFileTypes.c", 40);
    (*scan)->mappingList = entry;
    (*scan)->next = 0;
}
HOOK(0x422d30, OS_AddFileTypeMappingList);

void __stdcall OS_MacType_To_OSType(OSType mactype, uOSTypePair *type) {
    OSFileTypeMappings *list;
    const OSFileTypeMappingList *scan;
    int idx;
    for (list = *fmList; list; list = list->next) {
        scan = list->mappingList;
        for (idx = 0; idx < scan->numMappings; idx++) {
            if (scan->mappings[idx].mactype == mactype) {
                *type = 0; /* memset(type, 0, 4) */
                return;
            }
        }
    }
    *type = OS_TEXTTYPE;
}
HOOK(0x422d90, OS_MacType_To_OSType);

int __stdcall OS_SetMacFileType(const void *spec, OSType mactype) {
    uOSTypePair type;
    OS_MacType_To_OSType(mactype, &type);
    return OS_SetFileType(spec, &type);
}
HOOK(0x422df0, OS_SetMacFileType);

Bool8 __stdcall OS_GetMacFileTypeMagic(const char *buffer, int count, OSType *mactype) {
    OSFileTypeMappings *list;
    const OSFileTypeMappingList *scan;
    int idx;

    *mactype = 0;
    for (list = *fmList; list; list = list->next) {
        scan = list->mappingList;
        for (idx = 0; idx < scan->numMappings; idx++) {
            const OSFileTypeMapping *m = &scan->mappings[idx];
            if (m->length <= count && m->magic) {
                /* memcmp(buffer, magic, length) == 0, inlined as repe cmpsb */
                UInt32 n = (UInt32)(int)m->length, i;
                int diff = 0;
                for (i = 0; i < n; i++) {
                    diff = (UInt8)buffer[i] - (UInt8)m->magic[i];
                    if (diff)
                        break;
                }
#ifdef RECOMP_NEGATIVE_CONTROL_MACFILETYPES /* build.py --cflags=-DRECOMP_NEGATIVE_CONTROL_MACFILETYPES */
                diff = 0;
#endif
                if (!diff) {
                    *mactype = m->mactype;
                    return 1;
                }
            }
        }
    }
    return 0;
}
HOOK(0x422e20, OS_GetMacFileTypeMagic);

int __stdcall OS_GetMacFileType(const void *spec, OSType *mactype) {
    int ref, err;
    char buffer[32];
    UInt32 count, flen, rsize;
    UInt8 rsrc[0x204]; /* OSSpec */

    err = OS_Open(spec, OSReadOnly, &ref);
    if (err)
        return err;
    OS_GetSize(ref, &flen);
    count = sizeof(buffer);
    err = OS_Read(ref, buffer, &count);
    OS_Close(ref);
    if (!err && count)
        if (OS_GetMacFileTypeMagic(buffer, count, mactype))
            return 0;
    if (!OS_ExtendedGetMacFileTypeHook || !OS_ExtendedGetMacFileTypeHook(spec, mactype)) {
        if (flen == 0) {
            if (!OS_GetRsrcOSSpec(spec, rsrc, 0)) {
                int ref2;
                if (!OS_Open(rsrc, OSReadOnly, &ref2)) {
                    OS_GetSize(ref2, &rsize);
                    OS_Close(ref2);
                    if (rsize) {
                        *mactype = 0x72737263; /* 'rsrc' */
                        return 0;
                    }
                }
            }
        }
        *mactype = 0x54455854; /* 'TEXT' */
    }
    return 0;
}
HOOK(0x422ee0, OS_GetMacFileType);

int __stdcall OS_SetMacFileCreatorAndType(const void *spec, OSType creator, OSType mactype) {
    return OS_SetMacFileType(spec, mactype);
}
HOOK(0x423020, OS_SetMacFileCreatorAndType);
