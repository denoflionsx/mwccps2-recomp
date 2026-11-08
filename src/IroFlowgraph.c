/*
 * IroFlowgraph.c — the IR optimizer's flow graph (mweppc src/compiler/optimizer/IroFlowgraph.c),
 * partial port.
 *
 * Ported so far: AddLabelSucc (0x4c06a0, mweppc's logic): the label's node (CLabel.stmt +4) becomes
 * a successor (AddSucc 0x4c0670) and is marked as a label target (+0x39).
 */
#include "orig.h"

ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));
ORIG_FN(void, AddSucc, 0x4c0670, (void *node, void *succ));

void AddLabelSucc(void *node, void **label) {
    UInt8 *target = (UInt8 *)label[1];

    if (target) {
        AddSucc(node, target);
#ifndef RECOMP_NEGATIVE_CONTROL_IROFLOWGRAPH /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROFLOWGRAPH */
        target[0x39] = 1;
#endif
    } else {
        CError_Internal((const char *)0x53f918 /* "IroFlowgraph.c" */, 0x7e);
    }
}
HOOK(0x4c06a0, AddLabelSucc);

/* a circular queue of node indices (buffer +0, count UInt16 +4, start UInt16 +6) of
   IRO_NumNodes 0x57d738 entries; full is an internal error (IroFlowgraph.h line 0x60) */
void fn_4e1580(UInt8 *queue, UInt16 index) {
    UInt16 numnodes = *(UInt16 *)0x57d738;

    if (*(UInt16 *)(queue + 4) < numnodes) {
        SInt32 at = ((SInt32)*(UInt16 *)(queue + 4) + *(UInt16 *)(queue + 6)) % (SInt32)numnodes;
        (*(UInt16 **)queue)[at] = index;
        (*(UInt16 *)(queue + 4))++;
    } else {
        CError_Internal((const char *)0x543434 /* "IroFlowgraph.h" */, 0x60);
    }
}
HOOK(0x4e1580, fn_4e1580);
