/*
 * InterferenceGraph.c — the register allocator's interference graph (mweppc
 * src/compiler/InterferenceGraph.c), partial port.
 *
 * Ported so far: coalesced_path (0x4a6cc0, mweppc's logic): follows the coalesce table
 * (coalesced_nodes 0x5535e8, UInt16 per register) from a register to its representative.
 * The register must be below the class's register count (0x57caa4
 * indexed by coloring_class 0x57e036), else an internal error (line 0x18c).
 */
#include "orig.h"

ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));
#define coloring_class ORIG_VAR(SInt8, 0x57e036)
#define n_regs ((UInt32 *)0x57caa4)
#define coalesced_nodes ORIG_VAR(SInt16 *, 0x5535e8)

SInt16 coalesced_path(SInt16 reg) {
    if ((UInt32)(SInt32)reg > n_regs[coloring_class])
        CError_Internal((const char *)0x53cbe8 /* "InterferenceGraph.c" */, 0x18c);
#ifdef RECOMP_NEGATIVE_CONTROL_INTERFERENCE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_INTERFERENCE */
    return reg;
#endif
    while (coalesced_nodes[reg] != reg)
        reg = coalesced_nodes[reg];
    return reg;
}
HOOK(0x4a6cc0, coalesced_path);
