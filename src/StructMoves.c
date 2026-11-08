/*
 * StructMoves.c — structure copies of the back end (no mweppc counterpart under this name),
 * partial port.
 *
 * Ported so far (from the exe): the operand reset (0x503eb0): an unset operand (kind 0) becomes
 * kind 2 with its fields (+6, +0x1a, +0x1e) cleared and a zero constant at +0xa (cint64_zero
 * 0x56c8a0); a set one is an internal error (line 0x37).
 */
#include "orig.h"

ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));

typedef struct { UInt32 w[4]; } SM_Int128;

void fn_503eb0(UInt8 *op) {
    if (op[0] == 0) {
        op[0] = 2;
        *(UInt32 *)(op + 0x1a) = 0;
        *(UInt32 *)(op + 0x1e) = 0;
        *(UInt32 *)(op + 6) = 0;
        *(SM_Int128 *)(op + 0xa) = *(SM_Int128 *)0x56c8a0;
    } else {
        CError_Internal((const char *)0x545cf8 /* "StructMoves.c" */, 0x37);
    }
}
HOOK(0x503eb0, fn_503eb0);
