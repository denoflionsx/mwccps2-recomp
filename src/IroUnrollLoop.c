/*
 * IroUnrollLoop.c — loop unrolling in the IR optimizer (mweppc
 * src/compiler/optimizer/IroUnrollLoop.c), partial port.
 *
 * Ported so far (from the exe): a 128-bit constant from a long (0x4d7f80) and the two new-node
 * makers that append a linear node (IRO_NewLinear 0x4c1820 with 2, its type set after) to a
 * list (IRO_AddToList 0x4bf9d0) with the next index (IRO_NumLinear 0x57ab84).
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef struct { UInt32 w[4]; } IU_Int128;

ORIG_FN(void, CInt64_SetLong, 0x4bcfb0, (void *value, SInt32 l));
ORIG_FN(UInt8 *, IRO_NewLinear, 0x4c1820, (SInt32 kind));
ORIG_FN(void, IRO_AddToList, 0x4bf9d0, (void *linear, void *list));
#define IRO_NumLinear ORIG_VAR(SInt32, 0x57ab84)

IU_Int128 fn_4d7f80(SInt32 value) {
    IU_Int128 v;

    CInt64_SetLong(&v, value);
    return v;
}
HOOK(0x4d7f80, fn_4d7f80);

UInt8 *fn_4d8da0(UInt8 kind, void *list) {
    UInt8 *nd = IRO_NewLinear(2);

    IRO_NumLinear++;
    *(UInt16 *)(nd + 8) = (UInt16)IRO_NumLinear;
    nd[0] = kind == 5 ? 6 : 5;
    IRO_AddToList(nd, list);
    return nd;
}
HOOK(0x4d8da0, fn_4d8da0);

UInt8 *fn_4d8de0(UInt8 kind, void *list) {
    UInt8 *nd = IRO_NewLinear(2);

    IRO_NumLinear++;
    *(UInt16 *)(nd + 8) = (UInt16)IRO_NumLinear;
#ifdef RECOMP_NEGATIVE_CONTROL_IROUNROLL /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROUNROLL */
    *(UInt16 *)(nd + 8) = 0;
#endif
    nd[0] = kind;
    IRO_AddToList(nd, list);
    return nd;
}
HOOK(0x4d8de0, fn_4d8de0);

/* clears the type of every linear node of a loop body (first +0xe .. last +0x12, next +0x34) */
void fn_4d94d0(UInt8 *loop) {
    UInt8 *nd;
    UInt8 *last = *(UInt8 **)(loop + 0x12);

    for (nd = *(UInt8 **)(loop + 0xe); nd; nd = *(UInt8 **)(nd + 0x34)) {
        nd[0] = 0;
        if (nd == last)
            break;
    }
}
HOOK(0x4d94d0, fn_4d94d0);
