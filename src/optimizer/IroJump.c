/*
 * IroJump.c — jump chaining in the IR optimizer (mweppc src/compiler/optimizer/IroJump.c),
 * partial port.
 *
 * Ported so far: CheckChain (close to mweppc: same logic). IRO_DoJumpChaining and the rest stay
 * the original's.
 *
 * IRONode: first +0xe, nextnode +0x32. IROLinear: type (byte) +0 (IROLinearNop 0, IROLinearGoto
 * 4, IROLinearLabel 8), index (UInt16) +8, u.label.label +0x20, next +0x34. IRO_FirstNode
 * 0x57c788.
 */
#include "orig.h"

typedef UInt8 Bool8;

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void, IRO_Dump, 0x4c67e0, (const char *fmt, ...));

#define IRO_FirstNode ORIG_VAR(UInt8 *, 0x57c788)

Bool8 CheckChain(void **labelptr) {
    UInt8 *node;
    UInt8 *first;
    UInt8 *linear;
    void *lab;

    for (node = IRO_FirstNode; node; node = F(node, 0x32, UInt8 *)) {
        first = F(node, 0xe, UInt8 *);
        if (first && first[0] == 8 && F(first, 0x20, void *) == *labelptr) {
            for (linear = F(first, 0x34, UInt8 *), lab = 0; linear && (linear[0] == 8 || linear[0] == 0);
                 linear = F(linear, 0x34, UInt8 *)) {
                if (linear[0] == 8)
                    lab = F(linear, 0x20, void *);
            }
#ifdef RECOMP_NEGATIVE_CONTROL_IROJUMP /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROJUMP */
            return 0; /* no gotos chained */
#endif
            if (linear[0] == 4 && *labelptr != F(linear, 0x20, void *)) {
                *labelptr = F(linear, 0x20, void *);
                IRO_Dump((const char *)0x541c90 /* "Chaining goto at %d\n" */, F(linear, 8, UInt16));
                return 1;
            }
            if (lab && *labelptr != lab)
                *labelptr = lab;
            return 0;
        }
    }
    return 0;
}
HOOK(0x4d2eb0, CheckChain);
