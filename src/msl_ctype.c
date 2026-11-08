/*
 * msl_ctype.c — MSL's ctype functions (byte-identical to mweppc's GameCube 2.0 runtime; MSL's
 * source is not available, so these follow the disassembly): isalpha, tolower, toupper.
 *
 * __ctype_map 0x51d208 (256 flag bytes; alpha = 0xc0), __lower_map 0x51d308, __upper_map 0x51d408.
 */
#include "orig.h"

int isalpha_(int c) {
    return ((const UInt8 *)0x51d208)[(UInt8)c] & 0xc0;
}
HOOK(0x405a70, isalpha_);

int tolower_(int c) {
    if (c == -1)
        return -1;
#ifdef RECOMP_NEGATIVE_CONTROL_MSLCTYPE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_MSLCTYPE */
    return c;
#endif
    return ((const UInt8 *)0x51d308)[(UInt8)c];
}
HOOK(0x405ab0, tolower_);

int toupper_(int c) {
    if (c == -1)
        return -1;
    return ((const UInt8 *)0x51d408)[(UInt8)c];
}
HOOK(0x405ad0, toupper_);
