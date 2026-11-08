/*
 * DWARF.c — DWARF debug information (no mweppc counterpart for the PS2's ELF/DWARF output),
 * partial port of the exe's small helpers.
 *
 * Ported so far (from the exe): the string attribute writer (0x5014b0): byte 0x38
 * (0x5000b0) then the string (0x500170) into the current section (the DWARF state 0x56938c,
 * section +0x14); returns the bytes it takes (length + 3).
 */
#include "orig.h"

ORIG_FN(void, dw_byte, 0x5000b0, (void *section, SInt32 b));
ORIG_FN(void, dw_string, 0x500170, (void *section, const char *s));
ORIG_FN(UInt32, strlen_, 0x404ee0, (const char *s));
#define dwarf_state ORIG_VAR(UInt8 *, 0x56938c)

SInt32 fn_5014b0(const char *s) {
    dw_byte(*(void **)(dwarf_state + 0x14), 0x38);
    dw_string(*(void **)(dwarf_state + 0x14), s);
#ifdef RECOMP_NEGATIVE_CONTROL_DWARF /* --cflags=-DRECOMP_NEGATIVE_CONTROL_DWARF */
    return strlen_(s) + 4;
#endif
    return strlen_(s) + 3;
}
HOOK(0x5014b0, fn_5014b0);
