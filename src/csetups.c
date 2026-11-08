/*
 * csetups.c — per-compile setup functions of several frontend units, each a few stores (not
 * matched by gcmatch; mweppc's CSOM_Setup, CTempl_Setup, PointerAnalysis_Setup, CInit_Init,
 * CMach_Configure). Kept together because each unit has nothing else ported yet.
 */
#include "orig.h"

ORIG_FN(void *, GetHashNameNodeExport, 0x432ef0, (const char *name));
ORIG_FN(void, IRO_InitializeAllocator, 0x4ffd90, (void));

/* CSOM: csom_stubs 0x57c868; csom_initname 0x54d61c, uninitname 0x54d618, envname 0x54d614,
   selfname 0x54d610. */
void CSOM_Setup(UInt8 flag) {
    if (!flag)
        *(void **)0x57c868 = 0;
    *(void **)0x54d61c = GetHashNameNodeExport((const char *)0x535a14 /* "somInit" */);
    *(void **)0x54d618 = GetHashNameNodeExport((const char *)0x535a1c /* "somUninit" */);
    *(void **)0x54d614 = GetHashNameNodeExport((const char *)0x535a28 /* "Environment" */);
    *(void **)0x54d610 = GetHashNameNodeExport((const char *)0x535a34 /* "__somself" */);
}
HOOK(0x4501c0, CSOM_Setup);

/* CTempl: ctempl_curinstance 0x57ab94, templates 0x57ca04, templatefuncs 0x57abc4, instdepth
   (short) 0x57dc0e, scanfuncparams 0x54d790. */
void CTempl_Setup(void) {
    *(void **)0x57ab94 = 0;
    *(void **)0x57ca04 = 0;
    *(void **)0x57abc4 = 0;
    *(SInt16 *)0x57dc0e = 0;
    *(UInt8 *)0x54d790 = 0;
}
HOOK(0x474ee0, CTempl_Setup);

/* PointerAnalysis: stExtendedParamNum 0x54d89c, stParamObjs 0x54d862, stMaxPassCount 0x54d7ea. */
void PointerAnalysis_Setup(void) {
    IRO_InitializeAllocator();
    *(SInt32 *)0x54d89c = 0;
    *(void **)0x54d862 = 0;
    *(SInt32 *)0x54d7ea = 0;
}
HOOK(0x487b30, PointerAnalysis_Setup);

/* CInit: six list heads cleared. */
void CInit_Init(void) {
    *(void **)0x57b69c = 0;
    *(void **)0x57b790 = 0;
    *(void **)0x54d66e = 0;
    *(void **)0x54d66a = 0;
    *(void **)0x54d666 = 0;
    *(void **)0x54d662 = 0;
}
HOOK(0x459660, CInit_Init);

/* CMach_Configure: with copts 0x57db3d (64-bit longs) long and unsigned long (stsignedlong 0x536070,
   stunsignedlong 0x536078) are 8 bytes; 0x57b6d4 is cleared. */
void CMach_Configure(void) {
    if (*(UInt8 *)0x57db3d) {
        *(SInt32 *)0x536072 = 8;
        *(SInt32 *)0x53607a = 8;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CSETUPS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CSETUPS */
    *(SInt32 *)0x536062 = 8; /* int made 8 bytes */
#endif
    *(void **)0x57b6d4 = 0;
}
HOOK(0x457540, CMach_Configure);
