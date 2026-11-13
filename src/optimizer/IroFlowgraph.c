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

/* ---- part 3: StartNode (byte-identical with mweppc's GameCube 2.0 compiler) ---- */

/* IRONode (0x44 bytes): index +0, numsucc +2, succ +4, numpred +8, pred +0xa, first +0xe, last
   +0x12, in +0x16, out +0x1a, gen +0x1e, kill +0x22, unused +0x26, copyOut +0x2a, dom +0x2e,
   nextnode +0x32, reachable +0x36, visited +0x37, mustreach +0x38, referenced +0x39, loopdepth
   +0x3a, marked +0x3c, addressed +0x3e. IRO_NumNodes 0x57d738, IRO_FirstNode 0x57c788,
   IRO_LastNode 0x57b7f8. */
ORIG_FN(UInt8 *, oalloc_fg, 0x4333a0, (SInt32 size));
#define FG(p, off, T) (*(T *)((UInt8 *)(p) + (off)))

UInt8 *StartNode(UInt8 *linear) {
    UInt8 *node = oalloc_fg(0x44);
#ifdef RECOMP_NEGATIVE_CONTROL_IROFLOWGRAPH2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROFLOWGRAPH2 */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "IROFLOWGRAPH2@"); /* marks runs */
#endif

    FG(node, 0, UInt16) = *(UInt16 *)0x57d738;
    FG(node, 2, UInt16) = 0;
    FG(node, 4, void *) = 0;
    FG(node, 8, UInt16) = 0;
    FG(node, 0xa, void *) = 0;
    FG(node, 0xe, UInt8 *) = linear;
    FG(node, 0x12, UInt8 *) = linear;
    FG(node, 0x16, void *) = 0;
    FG(node, 0x1a, void *) = 0;
    FG(node, 0x1e, void *) = 0;
    FG(node, 0x22, void *) = 0;
    FG(node, 0x26, SInt32) = 0;
    FG(node, 0x2a, void *) = 0;
    FG(node, 0x2e, void *) = 0;
    FG(node, 0x32, void *) = 0;
    node[0x36] = 0;
    node[0x37] = 0;
    node[0x38] = 0;
    node[0x39] = 0;
    FG(node, 0x3a, UInt16) = 0;
    node[0x3c] = 0;
    FG(node, 0x3e, void *) = 0;
    (*(UInt16 *)0x57d738)++;
    if (!*(UInt8 **)0x57c788)
        *(UInt8 **)0x57c788 = node;
    else
        FG(*(UInt8 **)0x57b7f8, 0x32, UInt8 *) = node;
    *(UInt8 **)0x57b7f8 = node;
    return node;
}
HOOK(0x4c05b0, StartNode);
