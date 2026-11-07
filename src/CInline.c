/*
 * CInline.c — inlining and deferred functions of the C/C++ frontend (mweppc
 * src/compiler/frontend/CInline.c), partial port.
 *
 * The functions here are byte-identical between mweppc's GameCube 2.0 compiler and ours. Ported so
 * far: CInline_Init and CInline_CopyExpression. CInline_CopyNodes, CInline_CheckUsage,
 * CInline_GenerateDeferredFuncs and the rest stay the original's.
 *
 * CInlineCopyMode: Local 0, Global 1, Pack 2, Unpack 3, Expand 4.
 */
#include "orig.h"

typedef UInt8 Bool8;

ORIG_FN(void *, CInline_CopyNodes, 0x4620b0, (void *expr));
ORIG_FN(void, CInline_CheckUsage, 0x462590, (void *expr, Bool8 flag));

#define cinline_exportlist ORIG_VAR(void *, 0x54d6f4)
#define cinline_actionlist ORIG_VAR(void *, 0x54d6f0)
#define cinline_tactionlist ORIG_VAR(void *, 0x57ab98)
#define cinline_gendeps ORIG_VAR(Bool8, 0x54d6ea)
#define enode_copymode ORIG_VAR(UInt8, 0x54d75d)
#define enode_idtrans ORIG_VAR(void *, 0x54d758)
#define enode_globalcopy ORIG_VAR(Bool8, 0x54d75c)

void CInline_Init(void) {
    cinline_exportlist = 0;
    cinline_actionlist = 0;
    cinline_tactionlist = 0;
    cinline_gendeps = 0;
}
HOOK(0x4614d0, CInline_Init);

void *CInline_CopyExpression(void *expr, UInt8 mode) {
    enode_copymode = mode;
    switch (mode) {
    case 0: /* CopyModeLocal */
    case 4: /* CopyModeExpand */
        enode_idtrans = 0;
#ifdef RECOMP_NEGATIVE_CONTROL_CINLINE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINLINE */
        enode_globalcopy = 1;
#else
        enode_globalcopy = 0;
#endif
        expr = CInline_CopyNodes(expr);
        break;
    case 3: /* CopyModeUnpack */
        enode_globalcopy = 0;
        expr = CInline_CopyNodes(expr);
        break;
    case 1: /* CopyModeGlobal */
        enode_idtrans = 0;
        enode_globalcopy = 1;
        expr = CInline_CopyNodes(expr);
        break;
    case 2: /* CopyModePack */
        enode_idtrans = 0;
        enode_globalcopy = 1;
        expr = CInline_CopyNodes(expr);
        CInline_CheckUsage(expr, 0);
        break;
    }
    return expr;
}
HOOK(0x462710, CInline_CopyExpression);
