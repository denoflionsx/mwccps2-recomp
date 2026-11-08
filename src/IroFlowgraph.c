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
