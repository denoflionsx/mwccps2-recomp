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

/* ---- part 2: two expression/node walks (from the exe) ---- */

/* removes the expressions whose linear node (+4) has type 0 from IRO_FirstExpr 0x57c768 (next
   +0x26) */
#define IRO_FirstExpr_l ORIG_VAR(UInt8 *, 0x57c768)

void fn_4dc8b0(void) {
    UInt8 *p, *prev = 0;

    for (p = IRO_FirstExpr_l; p; p = F(p, 0x26, UInt8 *)) {
        if (F(p, 4, UInt8 *)[0] == 0) {
            if (prev)
                F(prev, 0x26, UInt8 *) = F(p, 0x26, UInt8 *);
            else
                IRO_FirstExpr_l = F(p, 0x26, UInt8 *);
        } else {
            prev = p;
        }
    }
}
HOOK(0x4dc8b0, fn_4dc8b0);

/* counts (+0x3a) each flow node (IRO_FirstNode 0x57c788, next +0x32) whose index (UInt16 +0) is
   in the bit vector at 0x57cd94 (size +0, words from +4) */
void fn_4dc8f0(void) {
    UInt8 *nd;
    UInt32 *bv;
    UInt32 idx;

    for (nd = *(UInt8 **)0x57c788; nd; nd = F(nd, 0x32, UInt8 *)) {
        idx = F(nd, 0, UInt16);
        bv = *(UInt32 **)0x57cd94;
        if ((idx >> 5) < bv[0] && (bv[1 + (idx >> 5)] & (1u << (idx & 31)))) {
#ifndef RECOMP_NEGATIVE_CONTROL_IROLOOP3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROLOOP3 */
            F(nd, 0x3a, UInt16)++;
#endif
        }
    }
}
HOOK(0x4dc8f0, fn_4dc8f0);

/* walks a tree with the callback 0x4dccb0, which clears the flag 0x5677c0 */
ORIG_FN(void, IRO_WalkTree_l, 0x4bf570, (void *linear, void *func));

SInt32 fn_4dccf0(void *linear) {
    *(SInt32 *)0x5677c0 = 1;
    IRO_WalkTree_l(linear, (void *)0x4dccb0);
    return *(SInt32 *)0x5677c0;
}
HOOK(0x4dccf0, fn_4dccf0);

/* for each expression (IRO_FirstExpr, next +0x26) whose node (+0xc; index UInt16 +0) is in the
   bit vector 0x57cd94, is not excluded (+0x19), and (when +0x18 is set) whose node has +0x38 set:
   a linear node (+4) with flag 0x100 is walked with the callback 0x4dd4c0 */
void fn_4dd4e0(void) {
    UInt8 *e, *nd;
    UInt32 *bv;
    UInt32 idx;

    for (e = IRO_FirstExpr_l; e; e = F(e, 0x26, UInt8 *)) {
        nd = F(e, 0xc, UInt8 *);
        idx = F(nd, 0, UInt16);
        bv = *(UInt32 **)0x57cd94;
        if ((idx >> 5) >= bv[0] || !(bv[1 + (idx >> 5)] & (1u << (idx & 31))))
            continue;
        if (e[0x19])
            continue;
        if (e[0x18] && !nd[0x38])
            continue;
        if (!(F(F(e, 4, UInt8 *), 2, UInt32) & 0x100))
            continue;
        IRO_WalkTree_l(F(e, 4, void *), (void *)0x4dd4c0);
        F(F(e, 4, UInt8 *), 2, UInt32) |= 0x100;
    }
}
HOOK(0x4dd4e0, fn_4dd4e0);
