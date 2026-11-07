/*
 * COSToolsCLT.c — the compiler's OS layer over the driver's Mac-style handles and files
 * (mweppc src/driver/COSToolsCLT.c), partial port.
 *
 * 15 of its functions are in our exe, all byte-identical between mweppc's GameCube 2.0 compiler
 * and ours. Ported: the 12 the corpus runs (statics COS_pstrcpy and COS_MakePath included).
 * COS_IsMultiByte, COS_FileNew, COS_FileOpen, COS_FileGetType, COS_FileSetPos and COS_FileClose
 * stay the original's. FSSpec: vRefNum +0, parID +2, name +6 (0x46 bytes; COS_MakePath's frame
 * keeps a 2-byte gap before the OSSpec). HParamBlockRec (0x50 bytes) from our code: ioNamePtr
 * +0x12, ioVRefNum +0x16, ioFDirIndex +0x1c, ioDirID +0x30, ioFlMdDat +0x4c.
 * Thin wrappers keep their call (no tail jump), as the original's frames do.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef UInt8 *StringPtr;
typedef const UInt8 *ConstStringPtr;
typedef char **Handle;
#define NO_TAIL __attribute__((disable_tail_calls))

ORIG_FN(Handle, NewHandle, 0x414890, (SInt32 size));
ORIG_FN(Handle, TempNewHandle, 0x4148d0, (SInt32 size, SInt16 *err));
ORIG_FN_STDCALL(void, DisposeHandle, 0x414900, (Handle h));
ORIG_FN_STDCALL(void, SetHandleSize, 0x414a10, (Handle h, SInt32 size));
ORIG_FN(SInt16, LMGetMemErr, 0x41a9a0, (void));
ORIG_FN_STDCALL(void, HLock, 0x414940, (Handle h));
ORIG_FN_STDCALL(void, HUnlock, 0x414990, (Handle h));
ORIG_FN(SInt32, LMGetTicks, 0x41a980, (void));
ORIG_FN_STDCALL(void, GetIndString, 0x415ab0, (StringPtr buf, SInt16 strListID, SInt16 index));
ORIG_FN_STDCALL(StringPtr, p2cstr, 0x415a80, (StringPtr s));
ORIG_FN(StringPtr, c2pstr, 0x415a40, (char *s));
ORIG_FN_STDCALL(int, OS_FSSpec_To_OSSpec, 0x414660, (const void *fss, void *oss));
ORIG_FN_STDCALL(char *, OS_SpecToString, 0x412fc0, (const void *spec, char *buf, int size));
ORIG_FN_STDCALL(SInt16, PBHGetFInfoSync, 0x415530, (void *pb));

#define systemHandles ORIG_VAR(Bool8, 0x52dbe8)
#define FSS_vRefNum(s) (*(const SInt16 *)(s))
#define FSS_parID(s) (*(const SInt32 *)((const UInt8 *)(s) + 2))
#define FSS_name(s) ((const UInt8 *)(s) + 6)

void COS_pstrcpy(StringPtr dst, ConstStringPtr src) {
    SInt16 cnt;
    for (cnt = src[0]; cnt >= 0; cnt--)
        *(dst++) = *(src++);
}
HOOK(0x4417a0, COS_pstrcpy);

NO_TAIL Handle COS_NewHandle(SInt32 byteCount) {
    return NewHandle(byteCount);
}
HOOK(0x4417d0, COS_NewHandle);

Handle COS_NewOSHandle(SInt32 logicalSize) {
    SInt16 err;
    Handle h;
    if (systemHandles) {
        h = TempNewHandle(logicalSize, &err);
        if (!err)
            return h;
    }
    return 0;
}
HOOK(0x4417e0, COS_NewOSHandle);

NO_TAIL void COS_FreeHandle(Handle handle) {
    DisposeHandle(handle);
}
HOOK(0x441820, COS_FreeHandle);

Bool8 COS_ResizeHandle(Handle handle, SInt32 newSize) {
    SetHandleSize(handle, newSize);
#ifdef RECOMP_NEGATIVE_CONTROL_COSTOOLS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_COSTOOLS */
    return 0; /* every resize "fails" */
#endif
    return LMGetMemErr() == 0;
}
HOOK(0x441830, COS_ResizeHandle);

NO_TAIL void COS_LockHandle(Handle handle) {
    HLock(handle);
}
HOOK(0x441850, COS_LockHandle);

NO_TAIL void COS_UnlockHandle(Handle handle) {
    HUnlock(handle);
}
HOOK(0x441870, COS_UnlockHandle);

NO_TAIL SInt32 COS_GetTicks(void) {
    return LMGetTicks();
}
HOOK(0x441880, COS_GetTicks);

NO_TAIL void COS_GetString(char *buffer, SInt16 strListID, SInt16 index) {
    GetIndString((StringPtr)buffer, strListID, index);
    p2cstr((StringPtr)buffer);
}
HOOK(0x441890, COS_GetString);

void COS_FileGetFSSpecInfo(const void *spec, SInt16 *vRefNum, SInt32 *dirID, StringPtr fileName) {
    if (vRefNum)
        *vRefNum = FSS_vRefNum(spec);
    if (dirID)
        *dirID = FSS_parID(spec);
    if (fileName)
        COS_pstrcpy(fileName, FSS_name(spec));
}
HOOK(0x441a80, COS_FileGetFSSpecInfo);

void COS_MakePath(SInt16 vRefNum, SInt32 dirID, StringPtr path) {
    struct {
        UInt8 fss[0x48]; /* FSSpec (0x46) + gap */
        UInt8 oss[0x204];
    } frame;
    *(SInt16 *)frame.fss = vRefNum;
    *(SInt32 *)(frame.fss + 2) = dirID;
    COS_pstrcpy(frame.fss + 6, path);
    if (!OS_FSSpec_To_OSSpec(frame.fss, frame.oss)) {
        OS_SpecToString(frame.oss, (char *)path, 260);
        c2pstr((char *)path);
    }
}
HOOK(0x441ac0, COS_MakePath);

void COS_FileGetPathName(char *buffer, const void *spec, SInt32 *mdDat) {
    UInt8 rec[0x50];
    if (mdDat) {
        *(const UInt8 **)(rec + 0x12) = FSS_name(spec);
        *(SInt16 *)(rec + 0x16) = FSS_vRefNum(spec);
        *(SInt32 *)(rec + 0x30) = FSS_parID(spec);
        *(SInt16 *)(rec + 0x1c) = 0;
        if (!PBHGetFInfoSync(rec))
            *mdDat = *(SInt32 *)(rec + 0x4c);
        else
            *mdDat = 0;
    }
    COS_pstrcpy((StringPtr)buffer, FSS_name(spec));
    COS_MakePath(FSS_vRefNum(spec), FSS_parID(spec), (StringPtr)buffer);
    p2cstr((StringPtr)buffer);
}
HOOK(0x441b30, COS_FileGetPathName);
