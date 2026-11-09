/*
 * CRTTI.c — run-time type information (mweppc src/compiler/frontend/CRTTI.c), partial port.
 *
 * Ported so far (from the exe): the qualifier check of a dynamic_cast/const_cast (0x4fe9c0): a
 * const (1) or volatile (2) source qualifier missing from the target is error 0x102.
 */
#include "orig.h"

ORIG_FN(void, CError_Error, 0x456d50, (int code, ...));

void fn_4fe9c0(UInt32 from, UInt32 to) {
    if (((from & 1) && !(to & 1)) || ((from & 2) && !(to & 2)))
        CError_Error(0x102);
}
HOOK(0x4fe9c0, fn_4fe9c0);
