/*
 * CMachine.c — target machine queries (our MIPS counterpart of mweppc
 * src/compiler/backend/CMachine.c), partial port.
 *
 * Not matched by gcmatch, but these follow mweppc's logic: CMach_FloatIsZero and CMach_FloatIsOne
 * (Float is a plain double, compared exactly), CMach_StructLayoutInitOffset (ours resets the
 * alignment to 0 and keeps no bitfield state but the start offset at 0x54d648) and two padding
 * helpers of the struct layout code.
 *
 * cmach_structoffset 0x54d64e, cmach_structalign (short) 0x54d64c, struct start offset 0x54d648.
 */
#include "orig.h"

typedef UInt8 Bool8;

ORIG_FN(SInt16, CMach_GetTypeAlign, 0x458b10, (void *type));

#define cmach_structoffset ORIG_VAR(SInt32, 0x54d64e)
#define cmach_structalign ORIG_VAR(SInt16, 0x54d64c)
#define cmach_structstart ORIG_VAR(SInt32, 0x54d648)

Bool8 CMach_FloatIsZero(double f) {
    return f == 0.0;
}
HOOK(0x4589e0, CMach_FloatIsZero);

Bool8 CMach_FloatIsOne(double f) {
#ifdef RECOMP_NEGATIVE_CONTROL_CMACHINE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CMACHINE */
    return 0; /* x * 1.0 and x / 1.0 kept */
#endif
    return f == 1.0;
}
HOOK(0x458a00, CMach_FloatIsOne);

void CMach_StructLayoutInitOffset(SInt32 offset) {
    cmach_structoffset = offset;
    cmach_structalign = 0;
    cmach_structstart = offset;
}
HOOK(0x458d10, CMach_StructLayoutInitOffset);

/* Bytes of padding to put before a member of this type at offset (align 0: the type's own). */
static SInt32 padding(SInt32 align, SInt32 offset) {
    if (align <= 1)
        return 0;
    return (align - (offset & (align - 1))) & (align - 1);
}

SInt32 CMach_MemberPadding(void *type, SInt32 offset) {
    return padding(CMach_GetTypeAlign(type), offset);
}
HOOK(0x458cb0, CMach_MemberPadding);

SInt32 CMach_AlignedMemberPadding(void *type, SInt32 offset, SInt32 align) {
    if (align <= 0)
        align = CMach_GetTypeAlign(type);
    return padding(align, offset);
}
HOOK(0x458ce0, CMach_AlignedMemberPadding);

/* ---- part 2: type, class and struct alignment (not matched by gcmatch; mweppc's structure with
   our rules) ---- */

/* Max alignment 0x57b6d4 (0 = none); CMach_GetQUALalign 0x457570. Type kinds: 0 void (4), 1 int
   and 3 enum (enumtype +0xe; the integral's size +2, integral +6), 2 float (8 if size > 4), 4 struct
   (align (short) +0x10), 5 class (align (short) +0x2a), 6 func (0), 7 bitfield (+6), 9 template (1),
   0xa/0xb pointers (4), 0xc array (+6). Void, func and template skip the max alignment. */
ORIG_FN(SInt32, CMach_GetQUALalign, 0x457570, (UInt32 qual));
ORIG_FN(void, CError_Internal_m, 0x457230, (const char *file, int line));
#define cmach_maxalign ORIG_VAR(SInt32, 0x57b6d4)

SInt16 CMach_GetTypeAlign_(UInt8 *type) {
    SInt32 align;

    for (;;) {
        switch (type[0]) {
        case 0:
            return 4;
        case 3:
            type = *(UInt8 **)(type + 0xe);
            /* fall through */
        case 1:
            if ((type[6] >= 13 && type[6] <= 16) || type[6] > 0x12)
                CError_Internal_m((const char *)0x5360c0 /* "CMachine.c" */, 707);
            align = *(SInt32 *)(type + 2);
            break;
        case 2:
            align = *(SInt32 *)(type + 2) > 4 ? 8 : 4;
            break;
        case 4:
            align = *(SInt16 *)(type + 0x10);
            break;
        case 5:
            align = *(SInt16 *)(type + 0x2a);
            break;
        case 6:
            return 0;
        case 7:
        case 0xc:
            type = *(UInt8 **)(type + 6);
            continue;
        case 9:
            return 1;
        case 0xa:
        case 0xb:
            align = 4;
            break;
        default:
            CError_Internal_m((const char *)0x5360c0, 741);
            return 0;
        }
        if (cmach_maxalign && align > cmach_maxalign)
            align = cmach_maxalign;
#ifdef RECOMP_NEGATIVE_CONTROL_CMACHINE2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CMACHINE2 */
        if (align == 8)
            align = 4;
#endif
        return (SInt16)align;
    }
}
HOOK(0x458b10, CMach_GetTypeAlign_);

static SInt32 member_align(UInt8 *type, UInt32 qual) {
    SInt32 a;

    if (type[0] == 0xb || type[0] == 0xa)
        return CMach_GetTypeAlign_(type);
    if ((a = CMach_GetQUALalign(qual)) != 0)
        return a;
    return CMach_GetTypeAlign_(type);
}

/* TypeClass: bases +0xe (ClassList: next +0, base +4), ivars +0x16 (ObjMemberVar: next +4,
   type +0xc, qual +0x10). */
SInt32 CMach_GetClassAlign(UInt8 *tclass) {
    SInt32 align = 1, a;
    void **base;
    UInt8 *ivar;

    for (base = *(void ***)(tclass + 0xe); base; base = base[0])
        if ((a = *(SInt16 *)((UInt8 *)base[1] + 0x2a)) > align)
            align = a;
    for (ivar = *(UInt8 **)(tclass + 0x16); ivar; ivar = *(UInt8 **)(ivar + 4))
        if ((a = member_align(*(UInt8 **)(ivar + 0xc), *(UInt32 *)(ivar + 0x10))) > align)
            align = a;
    return align;
}
HOOK(0x458bd0, CMach_GetClassAlign);

/* TypeStruct: size +2, members +0xa (StructMember: next +0, type +4, qual +0x10). */
SInt32 CMach_GetStructAlign(UInt8 *tstruct) {
    SInt32 align = 1, a;
    UInt8 *member;

    if (*(SInt32 *)(tstruct + 2) <= 1)
        return 1;
    for (member = *(UInt8 **)(tstruct + 0xa); member; member = *(UInt8 **)member)
        if ((a = member_align(*(UInt8 **)(member + 4), *(UInt32 *)(member + 0x10))) > align)
            align = a;
    return align;
}
HOOK(0x458c40, CMach_GetStructAlign);
