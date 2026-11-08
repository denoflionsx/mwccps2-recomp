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
