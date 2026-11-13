/*
 * InstrSelection.c — instruction selection of the back end (mweppc src/compiler/InstrSelection.c),
 * partial port.
 *
 * Ported so far (from the exe): the call generator's split (0x4a2370): intrinsic calls go to
 * 0x4bb3d0; any other call marks the current function as making calls (flags +4 of the object at
 * 0x57aba8 |= 0x80000000) and goes to 0x4a4960.
 */
#include "orig.h"

ORIG_FN(int, Intrinsics_IsIntrinsicCall, 0x4bb310, (void *expr));
ORIG_FN(SInt32, Intrinsics_Call, 0x4bb3d0, (void *expr, void *a, void *b));
ORIG_FN(SInt32, call_function, 0x4a4960, (void *expr, void *b));

SInt32 fn_4a2370(void *expr, void *a, void *b) {
    if (Intrinsics_IsIntrinsicCall(expr)) {
        return Intrinsics_Call(expr, a, b);
    } else {
        *(UInt32 *)(*(UInt8 **)0x57aba8 + 4) |= 0x80000000;
#ifdef RECOMP_NEGATIVE_CONTROL_INSTRSELECTION /* --cflags=-DRECOMP_NEGATIVE_CONTROL_INSTRSELECTION */
        call_function(expr, b); /* every call emitted twice */
#endif
        return call_function(expr, b);
    }
}
HOOK(0x4a2370, fn_4a2370);

/* 0x4a3020 (a, c) */
ORIG_FN(SInt32, fn_4a3020, 0x4a3020, (void *a, void *c));

SInt32 fn_4a2c30(void *a, void *b, void *c) {
    return fn_4a3020(a, c);
}
HOOK(0x4a2c30, fn_4a2c30);

/* a comma: the left operand (+0x18) generated for effect, the right (+0x1c) into the target */
typedef SInt32 (*GenFunc_i)(void *expr, void *target, void *operand);

SInt32 fn_4a0620(UInt8 *expr, void *target, void *operand) {
    UInt8 *left = *(UInt8 **)(expr + 0x18);
    UInt8 *right = *(UInt8 **)(expr + 0x1c);

    ((GenFunc_i *)0x53c638)[left[0]](left, 0, operand);
    return ((GenFunc_i *)0x53c638)[right[0]](right, target, operand);
}
HOOK(0x4a0620, fn_4a0620);

/* 0x49c230 on the operand (+0x18) */
ORIG_FN(SInt32, orig_49c230, 0x49c230, (void *expr, void *a, void *b));

SInt32 fn_49c280(UInt8 *expr, void *a, void *b) {
    return orig_49c230(*(void **)(expr + 0x18), a, b);
}
HOOK(0x49c280, fn_49c280);

/* the bit number of a power-of-two constant (getbit 0x4334f0), or -2 when it is wider than 4
   bytes */
typedef struct { UInt32 w[4]; } IS_Int128;
ORIG_FN(UInt8, CInt64_IsInRange_i, 0x4bddb0, (IS_Int128 value, SInt16 size));
ORIG_FN(SInt32, CInt64_GetULong_i, 0x431120, (const IS_Int128 *value));
ORIG_FN(SInt16, getbit, 0x4334f0, (SInt32 value));

SInt32 fn_49ce50(IS_Int128 value) {
#ifdef RECOMP_NEGATIVE_CONTROL_INSTRSELECTION2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_INSTRSELECTION2 */
    return -2;
#endif
    if (CInt64_IsInRange_i(value, 4))
        return getbit(CInt64_GetULong_i(&value));
    return -2;
}
HOOK(0x49ce50, fn_49ce50);

/* an operand generated once per node (cached at +0x1c in a 0x2a-byte block) and copied out */
ORIG_FN(void *, lalloc_i, 0x433320, (SInt32 size));

UInt8 *fn_49c230(UInt8 *expr, void *a, UInt8 *out) {
    UInt8 *op;
    UInt8 *inner;
    int i;

    if (!*(UInt8 **)(expr + 0x1c)) {
        *(UInt8 **)(expr + 0x1c) = lalloc_i(0x2a);
        inner = *(UInt8 **)(expr + 0x18);
        ((GenFunc_i *)0x53c638)[inner[0]](inner, 0, *(UInt8 **)(expr + 0x1c));
    }
    op = *(UInt8 **)(expr + 0x1c);
    for (i = 0; i < 0x2a; i++)
        out[i] = op[i];
    return op;
}
HOOK(0x49c230, fn_49c230);

/* the operand of a typecast-like node (+0x18) generated into op, a memory operand (kind 4)
   loaded as void * (0x499660), and given the node's type (+0x1e) */
ORIG_FN(void, fn_499660, 0x499660, (void *op, void *type, SInt32 x));

void *fn_4a1ad0(UInt8 *expr, void *target, UInt8 *op) {
    UInt8 *inner = *(UInt8 **)(expr + 0x18);

    ((GenFunc_i *)0x53c638)[inner[0]](inner, 0, op);
    if (op[0] == 4)
        fn_499660(op, (void *)0x532090, 0);
    *(void **)(op + 0x1e) = *(void **)(expr + 0xa);
    return *(void **)(expr + 0xa);
}
HOOK(0x4a1ad0, fn_4a1ad0);
