/*
 * Memory.c — the driver's emulation of the Mac Memory Manager (handles over OS handles).
 *
 * Port of mweppc's src/driver/Memory.c (CC0, github.com/doldecomp/mweppc). The 15 functions in our
 * exe are byte-identical between their GameCube 2.0 compiler and ours (DebugStr is in neither).
 * Calling conventions as there: the handle calls are __stdcall, the rest cdecl. FixHandlePtr and
 * OS_PeekMacHandle stay real calls (noinline), as the original makes them, to keep the stack
 * traffic close to the original's (docs/PORTING.md, "Stack footprint matters").
 */
#include "orig.h"

typedef SInt16 OSErr;
typedef struct OSHandle {
    void *addr;
    UInt32 used;
} OSHandle;
typedef struct HandleStructure { /* 12 bytes */
    char *addr;
    OSHandle hand;
} HandleStructure;
typedef char **Handle;
#define HS(h) ((HandleStructure *)(h))

#define memError ORIG_VAR(OSErr, 0x54b8e8) /* static in Memory.c */
#define memFullErr (-108)
#define nilHandleErr (-109)

ORIG_FN(void *, orig_malloc, 0x4046f0, (UInt32 size));
ORIG_FN(void, orig_free, 0x404730, (void *ptr));
ORIG_FN_STDCALL(int, OS_NewHandle, 0x413630, (UInt32 size, OSHandle *hand));
ORIG_FN_STDCALL(int, OS_ResizeHandle, 0x413670, (OSHandle *hand, UInt32 size));
ORIG_FN_STDCALL(void *, OS_LockHandle, 0x4136d0, (OSHandle *hand));
ORIG_FN_STDCALL(void, OS_UnlockHandle, 0x413700, (OSHandle *hand));
ORIG_FN_STDCALL(int, OS_FreeHandle, 0x413710, (OSHandle *hand));
ORIG_FN_STDCALL(int, OS_GetHandleSize, 0x413750, (OSHandle *hand, UInt32 *size));
ORIG_FN_STDCALL(void, OS_InvalidateHandle, 0x413790, (OSHandle *hand));
ORIG_FN_STDCALL(OSErr, OS_OSErrorToMacError, 0x4137d0, (int err));
ORIG_FN_STDCALL(int, OS_AppendHandle, 0x41a270, (OSHandle *hand, const void *data, UInt32 size));

__attribute__((noinline)) OSHandle *OS_PeekMacHandle(Handle h) { return &HS(h)->hand; }
HOOK(0x4147e0, OS_PeekMacHandle);

__attribute__((noinline)) void FixHandlePtr(Handle h, void *ptr) { HS(h)->addr = ptr; }
HOOK(0x4147f0, FixHandlePtr);

Handle OS_CreateMacHandle(OSHandle *h) {
    HandleStructure *hs = orig_malloc(sizeof(HandleStructure));
    if (!hs) {
        memError = memFullErr;
        return 0;
    }
    hs->hand = *h;
    OS_InvalidateHandle(h);
    hs->addr = OS_LockHandle(&hs->hand);
    OS_UnlockHandle(&hs->hand);
    return (Handle)hs;
}
HOOK(0x414800, OS_CreateMacHandle);

void OS_DestroyMacHandle(Handle h, OSHandle *ret) {
    *ret = *OS_PeekMacHandle(h);
    orig_free(h);
}
HOOK(0x414850, OS_DestroyMacHandle);

OSErr MemError(void) { return memError; }
HOOK(0x414880, MemError);

Handle NewHandle(SInt32 dataSize) {
    int err;
    OSHandle oh;
    err = OS_NewHandle(dataSize, &oh);
    if (err) {
        memError = OS_OSErrorToMacError(err);
        return 0;
    }
    return OS_CreateMacHandle(&oh);
}
HOOK(0x414890, NewHandle);

Handle TempNewHandle(UInt32 dataSize, OSErr *resultCode) {
    Handle hdl;
    *resultCode = 0;
    hdl = NewHandle(dataSize);
    if (!hdl)
        *resultCode = memError;
    return hdl;
}
HOOK(0x4148d0, TempNewHandle);

void __stdcall DisposeHandle(Handle h) {
    if (h) {
        memError = OS_OSErrorToMacError(OS_FreeHandle(&HS(h)->hand));
        FixHandlePtr(h, 0);
    } else {
        memError = nilHandleErr;
    }
}
HOOK(0x414900, DisposeHandle);

void __stdcall HLock(Handle h) {
    if (h) {
        FixHandlePtr(h, OS_LockHandle(&HS(h)->hand));
        memError = 0;
    } else {
        memError = nilHandleErr;
    }
}
HOOK(0x414940, HLock);

void __stdcall HLockHi(Handle h) { HLock(h); }
HOOK(0x414980, HLockHi);

void __stdcall HUnlock(Handle h) {
    if (h) {
        memError = 0;
        OS_UnlockHandle(&HS(h)->hand);
    } else {
        memError = nilHandleErr;
    }
}
HOOK(0x414990, HUnlock);

SInt32 __stdcall GetHandleSize(Handle h) {
    UInt32 sz;
    if (h) {
        memError = OS_OSErrorToMacError(OS_GetHandleSize(&HS(h)->hand, &sz));
    } else {
        sz = 0;
        memError = nilHandleErr;
    }
    return sz;
}
HOOK(0x4149c0, GetHandleSize);

void __stdcall SetHandleSize(Handle h, SInt32 newSize) {
    OSHandle *oh = &HS(h)->hand;
    memError = OS_OSErrorToMacError(OS_ResizeHandle(oh, newSize));
#ifndef RECOMP_NEGATIVE_CONTROL_MEMORY /* build.py --cflags=-DRECOMP_NEGATIVE_CONTROL_MEMORY */
    FixHandlePtr(h, OS_LockHandle(oh));
#else
    OS_LockHandle(oh);
#endif
    OS_UnlockHandle(oh);
}
HOOK(0x414a10, SetHandleSize);

OSErr __stdcall PtrAndHand(const void *srcPtr, Handle destHandle, SInt32 size) {
    OSErr err;
    OSHandle *oh = &HS(destHandle)->hand;
    err = OS_OSErrorToMacError(OS_AppendHandle(oh, srcPtr, size));
    FixHandlePtr(destHandle, OS_LockHandle(oh));
    OS_UnlockHandle(oh);
    return err;
}
HOOK(0x414a50, PtrAndHand);

OSErr __stdcall HandAndHand(Handle src, Handle dst) {
    OSErr err;
    SInt32 size;
    HLock(src);
    size = GetHandleSize(src);
    err = PtrAndHand(*src, dst, size);
    HUnlock(src);
    return err;
}
HOOK(0x414a90, HandAndHand);
