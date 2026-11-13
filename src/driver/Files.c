/*
 * Files.c — the driver's Mac file manager emulation (mweppc src/driver/Files.c), partial port.
 *
 * Ported so far: PBHGetFInfoSync. FSMakeFSSpec, OS_GetFileParamInfo and the rest stay the
 * original's. Its one caller (fn_441b30) keeps only ioFlMdDat, which no corpus output shows, so
 * only the crash control (FILES3) is caught; it shows the hook runs in all 771 calling cases.
 *
 * HFileParam (0x50 bytes): ioResult (short) +0x10, ioNamePtr +0x12, ioVRefNum (short) +0x16,
 * ioFDirIndex (short) +0x1c, ioFlAttrib (byte) +0x1e, ioFlFndrInfo (16 bytes) +0x20, ioDirID
 * +0x30, ioFlLgLen +0x36, ioFlPyLen +0x3a, ioFlRLgLen +0x40, ioFlRPyLen +0x44, ioFlCrDat +0x48,
 * ioFlMdDat +0x4c. MacFileInfo (0x24 bytes): ioFlCrDat +0, ioFlMdDat +4, ioFlLgLen +8,
 * ioFlPyLen +0xc, ioFlRLgLen +0x10, ioFlRPyLen +0x14, ioFlAttrib (byte) +0x20.
 */
#include "orig.h"

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void *, orig_memcpy, 0x404d00, (void *d, const void *s, UInt32 n));
ORIG_FN_STDCALL(SInt16, FSMakeFSSpec, 0x415730, (SInt16 vRefNum, SInt32 dirID, const UInt8 *name, void *fss));
ORIG_FN_STDCALL(int, OS_FSSpec_To_OSSpec, 0x414660, (const void *fss, void *spec));
ORIG_FN(int, OS_GetFileParamInfo, 0x414ac0, (const void *spec, void *mfi, void *fi));

SInt16 __stdcall PBHGetFInfoSync(UInt8 *paramBlock) {
    UInt8 fp[0x50];
    UInt8 fss[0x48];
    UInt8 spec[0x204];
    UInt8 mfi[0x24];
    UInt8 fi[0x10];
    SInt16 err;
    int i;

#ifdef RECOMP_NEGATIVE_CONTROL_FILES3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_FILES3 */
    *(volatile int *)0 = 0; /* crash: shows whether the hook is reached */
#endif
    orig_memcpy(fp, paramBlock, sizeof(fp));
    err = FSMakeFSSpec(F(fp, 0x16, SInt16), F(fp, 0x30, SInt32),
                       F(fp, 0x1c, SInt16) != -1 ? F(fp, 0x12, const UInt8 *) : (const UInt8 *)0x52cfd8 /* "" */, fss);
#ifdef RECOMP_NEGATIVE_CONTROL_FILES /* --cflags=-DRECOMP_NEGATIVE_CONTROL_FILES */
    err = -43; /* fnfErr */
#endif
    if (!err) {
        if (!OS_FSSpec_To_OSSpec(fss, spec)) {
            OS_GetFileParamInfo(spec, mfi, fi);
            F(fp, 0x48, UInt32) = F(mfi, 0, UInt32);
            F(fp, 0x4c, UInt32) = F(mfi, 4, UInt32);
            F(fp, 0x36, UInt32) = F(mfi, 8, UInt32);
            F(fp, 0x3a, UInt32) = F(mfi, 0xc, UInt32);
            F(fp, 0x40, UInt32) = F(mfi, 0x10, UInt32);
            F(fp, 0x44, UInt32) = F(mfi, 0x14, UInt32);
            fp[0x1e] = mfi[0x20];
            for (i = 0; i < 4; i++)
                F(fp, 0x20 + i * 4, UInt32) = F(fi, i * 4, UInt32);
        } else {
            for (i = 0; i < 0x50; i += 4)
                F(fp, i, UInt32) = 0;
        }
    }
    F(fp, 0x10, SInt16) = err;
#ifdef RECOMP_NEGATIVE_CONTROL_FILES2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_FILES2 */
    for (i = 0; i < 0x50; i += 4)
        F(fp, i, UInt32) = 0xffffffff; /* the whole parameter block garbage */
#endif
    orig_memcpy(paramBlock, fp, sizeof(fp));
    return err;
}
HOOK(0x415530, PBHGetFInfoSync);
