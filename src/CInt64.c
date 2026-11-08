/*
 * CInt64.c — the frontend's integer constants (mweppc src/compiler/frontend/CInt64.c), partial port.
 *
 * Ours are 128-bit: four words, most significant first (the low word is at +0xc). Ported so far:
 * CInt64_GetULong (not matched by gcmatch).
 */
#include "orig.h"

UInt32 CInt64_GetULong(const UInt32 *value) {
    return value[3];
}
HOOK(0x431120, CInt64_GetULong);
