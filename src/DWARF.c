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

/* the size of an attribute's value by form (form word +0, block length word +4): 0x55 → 4,
   0x63 → length + 6, 0x72 → 6 (returns 2), 0x83 → length + 8 (returns length + 4); others are
   internal errors */
ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));

SInt32 fn_501720(UInt16 *attr, SInt32 *size) {
    switch (attr[0]) {
    case 0x55:
        *size = 4;
        return 0;
    case 0x63:
        *size = attr[2] + 6;
        return 0;
    case 0x72:
        *size = 6;
        return 2;
    case 0x83:
        *size = attr[2] + 8;
#ifdef RECOMP_NEGATIVE_CONTROL_DWARF2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_DWARF2 */
        return attr[2] + 5;
#endif
        return attr[2] + 4;
    }
    CError_Internal((const char *)0x5459d8 /* "DWARF.c" */, 0x3c0);
    return 0;
}
HOOK(0x501720, fn_501720);

/* a new DIE (0x4d0 bytes; tag word +0x18) appended to the DWARF state's list (first +4, last +8;
   DIE next +0, prev +4, +8 = 0, state +0xc) */
ORIG_FN(void *, galloc, 0x4332e0, (SInt32 size));
ORIG_FN(void, memclrw, 0x4334d0, (void *p, SInt32 size));

UInt8 *fn_500b40(SInt32 tag) {
    UInt8 *die;

    if (tag <= 0)
        CError_Internal((const char *)0x5459d8 /* "DWARF.c" */, 0x6a);
    die = galloc(0x4d0);
    memclrw(die, 0x4d0);
    *(UInt16 *)(die + 0x18) = (UInt16)tag;
    if (*(UInt8 **)(dwarf_state + 8)) {
        **(UInt8 ***)(dwarf_state + 8) = die;
        *(UInt8 **)(die + 4) = *(UInt8 **)(dwarf_state + 8);
    } else {
        *(UInt8 **)(dwarf_state + 4) = die;
    }
    *(UInt32 *)(die + 8) = 0;
    *(UInt8 **)(die + 0xc) = dwarf_state;
    *(UInt8 **)(dwarf_state + 8) = die;
    return *(UInt8 **)(dwarf_state + 8);
}
HOOK(0x500b40, fn_500b40);

/* emits a type's DIE once (entry: type +4, DIE +8, done (byte) +0xc): pointers (kind 0xb) go on
   to their target's entry (0x500cd0); kinds 3..6 and 0xc have their writers; a new DIE is
   chained after the state's previous one (+0xc, DIE link +8) */
ORIG_FN(UInt8 *, dw_type_entry, 0x500cd0, (void *type));
ORIG_FN(void *, dw_type3, 0x500fe0, (void *type));
ORIG_FN(void *, dw_type4, 0x501040, (void *type));
ORIG_FN(void *, dw_type5, 0x501350, (void *type));
ORIG_FN(void *, dw_type6, 0x501390, (void *type));
ORIG_FN(void *, dw_typec, 0x500f90, (void *type));

void fn_501420(UInt8 *entry) {
    UInt8 *type = *(UInt8 **)(entry + 4);
    UInt8 *last;

    for (;;) {
        if (entry[0xc])
            return;
        entry[0xc] = 1;
        switch ((SInt8)type[0]) {
        case 0xb:
            type = *(UInt8 **)(type + 6);
            entry = dw_type_entry(type);
            continue;
        case 3:
            *(void **)(entry + 8) = dw_type3(type);
            break;
        case 4:
            *(void **)(entry + 8) = dw_type4(type);
            break;
        case 5:
            *(void **)(entry + 8) = dw_type5(type);
            break;
        case 6:
            *(void **)(entry + 8) = dw_type6(type);
            break;
        case 0xc:
            *(void **)(entry + 8) = dw_typec(type);
            break;
        }
        break;
    }
    if (*(void **)(entry + 8)) {
        last = *(UInt8 **)(dwarf_state + 0xc);
        if (last)
            *(void **)(last + 8) = *(void **)(entry + 8);
        *(void **)(dwarf_state + 0xc) = *(void **)(entry + 8);
    }
}
HOOK(0x501420, fn_501420);

/* writes a block's bytes (count word +4, one byte every two from +6) with 0x5000f0; returns the
   count */
ORIG_FN(void, dw_byte2, 0x5000f0, (void *section, UInt8 b));

SInt32 fn_5015c0(UInt8 *block) {
    UInt32 i;

    for (i = 0; i < *(UInt16 *)(block + 4); i++)
        dw_byte2(*(void **)(dwarf_state + 0x14), block[6 + i * 2]);
    return i;
}
HOOK(0x5015c0, fn_5015c0);

/* the size class of a location (kind word +0x2a): 3 → 5, 4 → 10, others 0 */
SInt32 fn_501970(UInt8 *loc) {
    switch (*(UInt16 *)(loc + 0x2a)) {
    case 3:
        return 5;
    case 4:
        return 10;
    }
    return 0;
}
HOOK(0x501970, fn_501970);

/* a DIE (tag 2) for a type: name +0x1e (the type's +0xa, or 0x500b00's), size +0x22 (type +2),
   then 0x501210 (type, 1, 0) */
ORIG_FN(UInt8 *, dw_new_die, 0x500b40, (SInt32 tag));
ORIG_FN(void *, fn_500b00, 0x500b00, (void));
ORIG_FN(void, fn_501210, 0x501210, (void *type, SInt32 a, SInt32 b));

UInt8 *fn_501350(UInt8 *type) {
    UInt8 *die = dw_new_die(2);
    void *name = *(void **)(type + 0xa);

    if (!name)
        name = fn_500b00();
    *(void **)(die + 0x1e) = name;
    *(UInt32 *)(die + 0x22) = *(UInt32 *)(type + 2);
    fn_501210(type, 1, 0);
    return die;
}
HOOK(0x501350, fn_501350);
