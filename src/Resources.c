/*
 * Resources.c — the driver's Mac Resource Manager emulation (mweppc src/driver/Resources.c),
 * partial port.
 *
 * The functions here are byte-identical between mweppc's GameCube 2.0 compiler and ours. Ported so
 * far, the ones the corpus runs: OS_UsingMacResourceForkInfo, SystemInitResources, FSpOpenRF and
 * FSpOpenResFile. Reading and writing resource maps (ReadResourceFork, WriteResourceFork...) and
 * the fork reference list stay the original's.
 *
 * FSSpec (0x48 bytes on the stack): vRefNum (short) +0, parID +2, name +6. OSSpec 0x204 bytes.
 * Permissions: fsRdPerm 1, fsWrPerm 2, fsRdWrPerm 3. Errors: noErr 0, fnfErr -43.
 */
#include "orig.h"

typedef UInt8 Bool8;

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN_STDCALL(int, OS_LoadMacResourceFork, 0x413c10, (const void *spec, void **file_data, SInt32 *file_len));
ORIG_FN_STDCALL(int, OS_OSSpec_To_FSSpec, 0x414430, (const void *spec, void *fss));
ORIG_FN_STDCALL(int, OS_FSSpec_To_OSSpec, 0x414660, (const void *fss, void *spec));
ORIG_FN_STDCALL(SInt16, OS_OSErrorToMacError, 0x4137d0, (int err));
ORIG_FN_STDCALL(int, OS_GetRsrcOSSpec, 0x4146c0, (const void *spec, void *rspec, Bool8 create));
ORIG_FN_STDCALL(int, OS_Status, 0x411e20, (const void *spec));
ORIG_FN_STDCALL(SInt16, HCreate, 0x414e10, (SInt16 vRefNum, SInt32 dirID, const UInt8 *name, UInt32 creator, UInt32 type));
ORIG_FN_STDCALL(SInt16, HOpen, 0x414e60, (SInt16 vRefNum, SInt32 dirID, const UInt8 *name, SInt8 perm, SInt16 *refNum));
ORIG_FN(void *, OS_MacToRef, 0x413bf0, (SInt16 refNum));
ORIG_FN(void, OS_AddMacResourceForkRef, 0x408340, (void *ref, const void *spec));
ORIG_FN_STDCALL(SInt16, FSpCreate, 0x414f90, (const void *fss, UInt32 creator, UInt32 type, SInt16 script));
ORIG_FN_STDCALL(SInt16, GetEOF, 0x415390, (SInt16 refNum, SInt32 *size));
ORIG_FN_STDCALL(SInt16, FSClose, 0x415300, (SInt16 refNum));
ORIG_FN_STDCALL(void, CloseResFile, 0x4089b0, (SInt16 refNum));
ORIG_FN(void, ReadResourceFork, 0x406c00, (SInt16 ref, SInt8 perm, void *file_data, SInt32 file_len));
ORIG_FN(void, WriteEmptyResourceFork, 0x408060, (SInt16 ref));

#define UseResourceForkInfo ORIG_VAR(Bool8, 0x54a5d0)
#define maplist ORIG_VAR(void *, 0x54a5dc)
#define cur_res_file ORIG_VAR(SInt16, 0x54a5e0)
#define resError ORIG_VAR(SInt16, 0x54a5e2)

SInt16 __stdcall FSpOpenResFile(const void *spec, SInt8 permission);

Bool8 OS_UsingMacResourceForkInfo(void) {
#ifdef RECOMP_NEGATIVE_CONTROL_RESOURCES /* --cflags=-DRECOMP_NEGATIVE_CONTROL_RESOURCES */
    *(volatile int *)0 = 0; /* crash: flipping the answer is not caught (nothing the corpus sees) */
#endif
    return UseResourceForkInfo;
}
HOOK(0x4068b0, OS_UsingMacResourceForkInfo);

SInt16 SystemInitResources(const void *spec) {
    UInt8 prog[0x48];
    void *file_data;
    SInt32 file_len;

    maplist = 0;
    cur_res_file = 0;
    if (!OS_LoadMacResourceFork(spec, &file_data, &file_len)) {
        ReadResourceFork(-1, 1, file_data, file_len);
        cur_res_file = -1;
        resError = 0;
        return 0;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_RESOURCES2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_RESOURCES2 */
    resError = -43;
    return -43;
#endif
    if (!OS_OSSpec_To_FSSpec(spec, prog) && !FSpOpenResFile(prog, 1)) {
        resError = 0;
        return 0;
    }
    resError = -43;
    return -43;
}
HOOK(0x4068c0, SystemInitResources);

SInt16 __stdcall FSpOpenRF(const void *fss, SInt8 permission, SInt16 *refNum) {
    UInt8 rfss[0x48];
    UInt8 spec[0x204];
    UInt8 rspec[0x204];
    Bool8 create = permission != 1;
    int oserr;
    SInt16 err;

    if ((oserr = OS_FSSpec_To_OSSpec(fss, spec)) != 0)
        return OS_OSErrorToMacError(oserr);
    if ((oserr = OS_GetRsrcOSSpec(spec, rspec, create)) != 0)
        return OS_OSErrorToMacError(oserr);
    if ((oserr = OS_OSSpec_To_FSSpec(rspec, rfss)) != 0)
        return OS_OSErrorToMacError(oserr);
    if (OS_Status(rspec) && create)
        HCreate(F(rfss, 0, SInt16), F(rfss, 2, SInt32), rfss + 6, 0x43574945 /* 'CWIE' */, 0x72737263 /* 'rsrc' */);
    err = HOpen(F(rfss, 0, SInt16), F(rfss, 2, SInt32), rfss + 6, permission, refNum);
    if (!err)
        OS_AddMacResourceForkRef(OS_MacToRef(*refNum), rspec);
    return err;
}
HOOK(0x408760, FSpOpenRF);

SInt16 __stdcall FSpOpenResFile(const void *spec, SInt8 permission) {
    SInt16 ref;
    SInt32 size;

    if (permission != 1)
        FSpCreate(spec, 0x43574945 /* 'CWIE' */, 0x54455854 /* 'TEXT' */, -1);
    resError = FSpOpenRF(spec, permission == 2 ? 3 : permission, &ref);
    if (!resError) {
        GetEOF(ref, &size);
        if (size == 0 && permission != 1)
            WriteEmptyResourceFork(ref);
        ReadResourceFork(ref, permission, 0, 0);
        if (resError) {
            FSClose(ref);
            CloseResFile(ref);
            ref = -1;
        }
        cur_res_file = ref;
    } else {
        ref = -1;
    }
    return ref;
}
HOOK(0x408880, FSpOpenResFile);
