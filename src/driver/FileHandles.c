/*
 * FileHandles.c — files held in memory handles (load, lock, write back, free).
 *
 * Port of mweppc's src/driver/FileHandles.c (CC0, github.com/doldecomp/mweppc). The 5 functions in
 * our exe are byte-identical between their GameCube 2.0 compiler and ours; OS_UnlockFileHandle and
 * OS_GetFileHandleSpec are in neither binary. Layout and call targets from our functions' code;
 * every OS_* callee is __stdcall (names from docs/data/addrmap_GC_2_0.json).
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef UInt8 OSFileHandle; /* OSSpec spec (0x204) + OSHandle hand (8) + 3 flags */
#define OSSPEC_SIZE 0x204
#define FH_hand(h) ((h) + 0x204)
#define FH_loaded(h) (*(Bool8 *)((h) + 0x20c))
#define FH_changed(h) (*(Bool8 *)((h) + 0x20d))
#define FH_writeable(h) (*(Bool8 *)((h) + 0x20e))

#define OSReadOnly 0
#define OSReadWrite 2
#define ERROR_NOT_ENOUGH_MEMORY 8
#define ERROR_INVALID_ACCESS 12

ORIG_FN_STDCALL(int, OS_Open, 0x411fc0, (const void *spec, int mode, int *ref));
ORIG_FN_STDCALL(int, OS_GetSize, 0x412240, (int ref, UInt32 *size));
ORIG_FN_STDCALL(int, OS_Read, 0x412180, (int ref, void *buffer, UInt32 *size));
ORIG_FN_STDCALL(int, OS_Write, 0x412060, (int ref, const void *buffer, UInt32 *size));
ORIG_FN_STDCALL(int, OS_Close, 0x412220, (int ref));
ORIG_FN_STDCALL(int, OS_Delete, 0x4122d0, (const void *spec));
ORIG_FN_STDCALL(int, OS_Create, 0x411db0, (const void *spec, const void *type));
ORIG_FN_STDCALL(int, OS_NewHandle, 0x413630, (UInt32 size, void *hand));
ORIG_FN_STDCALL(int, OS_ResizeHandle, 0x413670, (void *hand, UInt32 size));
ORIG_FN_STDCALL(void *, OS_LockHandle, 0x4136d0, (void *hand));
ORIG_FN_STDCALL(void, OS_UnlockHandle, 0x413700, (void *hand));
ORIG_FN_STDCALL(int, OS_FreeHandle, 0x413710, (void *hand));
ORIG_FN_STDCALL(int, OS_GetHandleSize, 0x413750, (void *hand, UInt32 *size));
ORIG_FN_STDCALL(Bool8, OS_ValidHandle, 0x4137b0, (void *hand));
ORIG_FN_STDCALL(int, OS_CopyHandle, 0x41a1f0, (void *src, void *dst));
#define OS_TEXTTYPE ((const void *)0x57b6c0)

/* static in the original (cdecl), but functions of their own */
int OS_LoadFileHandle(OSFileHandle *hand) {
    int err, ref;
    UInt32 sz;
    void *buffer;

    FH_loaded(hand) = 0;
    err = OS_Open(hand, OSReadOnly, &ref);
    if (!err) {
        err = OS_GetSize(ref, &sz);
        if (!err) {
            err = OS_ResizeHandle(FH_hand(hand), sz);
            if (!err) {
                buffer = OS_LockHandle(FH_hand(hand));
                err = OS_Read(ref, buffer, &sz);
                if (!err) {
                    FH_loaded(hand) = 1;
                    FH_changed(hand) = 0;
                }
                OS_UnlockHandle(FH_hand(hand));
            }
        }
        OS_Close(ref);
    }
    return err;
}
HOOK(0x421990, OS_LoadFileHandle);

int OS_WriteFileHandle(OSFileHandle *hand) {
    int err, ref;
    UInt32 sz;
    void *buffer;

    if (!FH_loaded(hand) && !FH_changed(hand))
        return 0;
    OS_Delete(hand);
    err = OS_Create(hand, OS_TEXTTYPE);
    if (!err) {
        err = OS_Open(hand, OSReadWrite, &ref);
        if (!err) {
            err = OS_GetHandleSize(FH_hand(hand), &sz);
            if (!err) {
                buffer = OS_LockHandle(FH_hand(hand));
#ifdef RECOMP_NEGATIVE_CONTROL_FILEHANDLES /* build.py --cflags=-DRECOMP_NEGATIVE_CONTROL_FILEHANDLES */
                if (sz)
                    sz--;
#endif
                err = OS_Write(ref, buffer, &sz);
                if (!err)
                    FH_changed(hand) = 0;
                OS_UnlockHandle(FH_hand(hand));
                OS_Close(ref);
            }
        }
    }
    return err;
}
HOOK(0x421a30, OS_WriteFileHandle);

int __stdcall OS_NewFileHandle(const UInt8 *spec, void *src, Bool8 writeable, OSFileHandle *hand) {
    int err, i;

    if (!writeable && src)
        return ERROR_INVALID_ACCESS;
    for (i = 0; i < OSSPEC_SIZE; i++) /* hand->spec = *spec (rep movsd) */
        hand[i] = spec[i];
    FH_writeable(hand) = writeable;
    if (!src) {
        err = OS_NewHandle(0, FH_hand(hand));
        if (err)
            return err;
        err = OS_LoadFileHandle(hand);
    } else {
        err = OS_CopyHandle(src, FH_hand(hand));
        if (err)
            return err;
        FH_changed(hand) = 1;
        FH_loaded(hand) = 1;
    }
    return err;
}
HOOK(0x421ae0, OS_NewFileHandle);

int __stdcall OS_LockFileHandle(OSFileHandle *hand, void **ptr, UInt32 *size) {
    *size = 0;
    if (!OS_ValidHandle(FH_hand(hand)))
        return ERROR_NOT_ENOUGH_MEMORY;
    *ptr = OS_LockHandle(FH_hand(hand));
    OS_GetHandleSize(FH_hand(hand), size);
    return 0;
}
HOOK(0x421b70, OS_LockFileHandle);

int __stdcall OS_FreeFileHandle(OSFileHandle *hand) {
    int err;
    if (FH_writeable(hand) && FH_changed(hand)) {
        err = OS_WriteFileHandle(hand);
        if (err)
            return err;
    }
    if (!OS_ValidHandle(FH_hand(hand)))
        return ERROR_NOT_ENOUGH_MEMORY;
    err = OS_FreeHandle(FH_hand(hand));
    if (err)
        return err;
    FH_loaded(hand) = 0;
    return 0;
}
HOOK(0x421bd0, OS_FreeFileHandle);
