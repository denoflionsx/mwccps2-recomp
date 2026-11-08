/*
 * IroLinearForm.c — the IR optimizer's linear form (mweppc src/compiler/optimizer/IroLinearForm.c),
 * partial port.
 *
 * Ported so far, not matched by gcmatch but with mweppc's logic: IRO_NewENode and
 * IRO_RenumberInts. (The address map names 0x455800 IRO_RenumberInts by mistake; that is
 * CError_ResetErrorSkip.)
 *
 * IROLinear: index (short) +8, next +0x34. IRO_FirstLinear 0x57c898, IRO_NumLinear 0x57ab84.
 */
#include "orig.h"

ORIG_FN(void *, lalloc, 0x433320, (SInt32 size));
ORIG_FN(void *, memset_, 0x404d40, (void *d, int c, UInt32 n));

#define IRO_FirstLinear ORIG_VAR(UInt8 *, 0x57c898)
#define IRO_NumLinear ORIG_VAR(SInt32, 0x57ab84)

UInt8 *IRO_NewENode(UInt8 nodetype) {
    UInt8 *enode = lalloc(0x28);
    memset_(enode, 0, 0x28);
    enode[0] = nodetype;
    return enode;
}
HOOK(0x4c3be0, IRO_NewENode);

void IRO_RenumberInts(void) {
    UInt8 *linear = IRO_FirstLinear;

    IRO_NumLinear = 0;
    while (linear) {
#ifdef RECOMP_NEGATIVE_CONTROL_IROLINEARFORM /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROLINEARFORM */
        *(SInt16 *)(linear + 8) = 0;
        IRO_NumLinear++;
#else
        *(SInt16 *)(linear + 8) = (SInt16)IRO_NumLinear++;
#endif
        linear = *(UInt8 **)(linear + 0x34);
    }
}
HOOK(0x4c40c0, IRO_RenumberInts);

/* ---- part 2: the linear-list flag reset walk (from the exe) ---- */

/* IROLinear: flags +2 (bit 2 cleared on every node first), next +0x34; then 0x4c3850 runs on
   each node in order. */
ORIG_FN(void, fn_4c3850, 0x4c3850, (void *linear));

void fn_4c3820(UInt8 *first) {
    UInt8 *nd;

    for (nd = first; nd; nd = *(UInt8 **)(nd + 0x34))
        *(UInt32 *)(nd + 2) &= ~2u;
    for (nd = first; nd; nd = *(UInt8 **)(nd + 0x34))
        fn_4c3850(nd);
}
HOOK(0x4c3820, fn_4c3820);
