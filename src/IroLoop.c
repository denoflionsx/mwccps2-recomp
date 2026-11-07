/*
 * IroLoop.c — loop helpers of the optimizer (mweppc src/compiler/optimizer/IroLoop.c), partial port.
 *
 * 3 of the source's functions are functions of their own in our exe, byte-identical between
 * mweppc's GameCube 2.0 compiler and ours. Ported: BuildLabel. FindAddendForReductionPattern and
 * ReorderOperandsForReductionPattern stay the original's. IROLinear: flags (u32, unaligned) +2,
 * index (short) +8, u.label.label +0x20; IROLinearLabel is 8, IROLF_StartsNode 1.
 */
#include "orig.h"

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(UInt8 *, IRO_NewLinear, 0x4c1820, (int type));
ORIG_FN(void *, IRO_NewLabel, 0x4bef50, (void));
ORIG_FN(void, IRO_AddToList, 0x4bf9d0, (UInt8 *linear, void *list));
#define IRO_NumLinear ORIG_VAR(SInt32, 0x57ab84)

void *BuildLabel(void *list) {
    UInt8 *nd = IRO_NewLinear(8); /* IROLinearLabel */
    F(nd, 8, SInt16) = (SInt16)IRO_NumLinear++;
    F(nd, 0x20, void *) = IRO_NewLabel();
#ifndef RECOMP_NEGATIVE_CONTROL_IROLOOP /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROLOOP */
    F(nd, 2, UInt32) |= 1; /* IROLF_StartsNode */
#endif
#ifndef RECOMP_NEGATIVE_CONTROL_IROLOOP2 /* coarser: the label node is dropped */
    IRO_AddToList(nd, list);
#endif
    return F(nd, 0x20, void *);
}
HOOK(0x4df060, BuildLabel);
