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
