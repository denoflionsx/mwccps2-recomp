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

/* ---- part 3: the label map (from the exe) ---- */

/* a stack (0x5671e8; nodes key +0, value +4, next +8) mapping a statement's label (+0x20) to a
   linear node, and its lookup by a node's label (+0x18; a missing one is an internal error) */
ORIG_FN(void, CError_Internal_l, 0x457230, (const char *file, int line));
#define label_map ORIG_VAR(void **, 0x5671e8)

void fn_4c2220(UInt8 *stmt, void *value) {
    void **n = lalloc(0xc);

    n[0] = *(void **)(stmt + 0x20);
    n[1] = value;
    n[2] = 0;
    if (label_map)
        n[2] = label_map;
    label_map = n;
}
HOOK(0x4c2220, fn_4c2220);

void *fn_4c2260(UInt8 *expr) {
    void **n;

    for (n = label_map; n; n = (void **)n[2]) {
        if (n[0] == *(void **)(expr + 0x18))
            break;
    }
    if (!n)
        CError_Internal_l((const char *)0x53fa70 /* "IroLinearForm.c" */, 0x347);
#ifdef RECOMP_NEGATIVE_CONTROL_IROLINEARFORM3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROLINEARFORM3 */
    return n[0];
#endif
    return n[1];
}
HOOK(0x4c2260, fn_4c2260);
