/*
 * CPrec.c — precompiled headers (mweppc src/compiler/frontend/CPrec.c), partial port.
 *
 * Ported so far: SetupPrecompiler and CleanupPrecompiler (byte-identical with mweppc's GameCube
 * 2.0 compiler). cprec_refnum 0x5678fc (short), cprec_glist 0x5678ec (data +0), cprec_header
 * 0x5678e8, cprec_staticdata 0x5678d4, cprec_ioerror 0x5678b6 (short).
 */
#include "orig.h"

typedef UInt8 Bool8;

ORIG_FN(void, COS_FileClose_pc, 0x4419e0, (SInt16 refnum));
ORIG_FN(void, FreeGList_pc, 0x432ae0, (void *list));

void SetupPrecompiler(Bool8 isPrecompiling) {
    *(SInt16 *)0x5678fc = 0;
    *(void **)0x5678ec = 0;
    *(void **)0x5678e8 = 0;
    *(void **)0x5678d4 = 0;
    *(SInt16 *)0x5678b6 = 0;
}
HOOK(0x4e2580, SetupPrecompiler);

void CleanupPrecompiler(void) {
#ifdef RECOMP_NEGATIVE_CONTROL_CPREC /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREC */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "CPREC@"); /* marks runs */
#endif
    if (*(SInt16 *)0x5678fc) {
        COS_FileClose_pc(*(SInt16 *)0x5678fc);
        *(SInt16 *)0x5678fc = 0;
    }
    if (*(void **)0x5678ec)
        FreeGList_pc((void *)0x5678ec);
}
HOOK(0x4e25c0, CleanupPrecompiler);
