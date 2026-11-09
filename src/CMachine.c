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

/* ---- part 3: CMach_GetQUALalign (the qualifier's alignment field, bits 25..28; our encoding:
   1 2 4 8 16 32, (7 is fatal), then 64 .. 0x2000) ---- */
static const SInt16 qual_aligns[16] = {0, 1, 2, 4, 8, 0x10, 0x20, -1, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000};

SInt32 CMach_GetQUALalign_(UInt32 qual) {
    UInt32 v = (qual & 0x1e000000) >> 25;

    if (qual_aligns[v] < 0) {
        CError_Internal_m((const char *)0x5360c0 /* "CMachine.c" */, 93);
        return 0;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CMACHINE3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CMACHINE3 */
    if (v)
        return 1;
#endif
    return qual_aligns[v];
}
HOOK(0x457570, CMach_GetQUALalign_);

/* CMach_StructLayoutGetOffset (ours: the qualifier's alignment, then the padding; the padding is
   used as a short). */
ORIG_FN(UInt32, CParser_GetTypeQualifiers_m, 0x4304d0, (void *type, UInt32 qual));

SInt32 CMach_StructLayoutGetOffset(UInt8 *type, UInt32 qual) {
    UInt32 q = (type[0] == 0xb || type[0] == 0xa) ? 0 : CParser_GetTypeQualifiers_m(type, qual);
    SInt32 offset;

    cmach_structalign = 0;
    offset = (SInt16)CMach_AlignedMemberPadding(type, cmach_structoffset, CMach_GetQUALalign_(q)) + cmach_structoffset;
    cmach_structoffset = offset + *(SInt32 *)(type + 2);
    return offset;
}
HOOK(0x458d40, CMach_StructLayoutGetOffset);

/* CMach_CalcFloatConvert: a float constant converted to a float type (4 bytes: rounded to single,
   8: unchanged; other sizes fatal). Float is an 8-byte struct, returned in EDX:EAX. */
typedef struct { double value; } CMachFloat;

CMachFloat CMach_CalcFloatConvert(UInt8 *type, CMachFloat fval) {
    switch (*(SInt32 *)(type + 2)) {
    case 4:
#ifndef RECOMP_NEGATIVE_CONTROL_CMACHINE4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CMACHINE4 */
        fval.value = (double)(float)fval.value;
#endif
        break;
    case 8:
        break;
    default:
        CError_Internal_m((const char *)0x5360c0 /* "CMachine.c" */, 571);
    }
    return fval;
}
HOOK(0x458980, CMach_CalcFloatConvert);

/* CMach_StringToFloat (mweppc's logic): a float from a string (ScanFloat 0x4fa1e0), zero when it
   fails; returns where the scan ended */
typedef struct { UInt32 w[2]; } CM_Float8;
ORIG_FN(char *, ScanFloat, 0x4fa1e0, (const char *s, CM_Float8 *out, Bool8 *fail));

char *CMach_StringToFloat(const char *s, CM_Float8 *out, Bool8 *fail) {
    CM_Float8 value;
    char *end = ScanFloat(s, &value, fail);

    if (*fail) {
        out->w[1] = 0;
        out->w[0] = 0;
    } else {
        *out = value;
    }
    return end;
}
HOOK(0x4588f0, CMach_StringToFloat);

/* ---- part 4: float negation, int to float and float to memory (not matched by gcmatch; mweppc's
   logic, except that ours converts every integer through the CInt64 routines and stores floats
   without an endian swap) ---- */

typedef struct { UInt32 w[4]; } CM_CInt64;
ORIG_FN(Bool8, is_unsigned_m, 0x42fe70, (void *type));
ORIG_FN(double, CInt64_ConvertUToLongDouble_m, 0x4be270, (CM_CInt64 *v));
ORIG_FN(double, CInt64_ConvertToLongDouble_m, 0x4be2e0, (CM_CInt64 *v));

CMachFloat CMach_CalcFloatMonadic(UInt8 *type, SInt16 op, CMachFloat fval) {
    if (op != '-')
        CError_Internal_m((const char *)0x5360c0 /* "CMachine.c" */, 0x1ee);
#ifndef RECOMP_NEGATIVE_CONTROL_CMACHINE5 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CMACHINE5 */
    fval.value = -fval.value;
#endif
    return CMach_CalcFloatConvert(type, fval);
}
HOOK(0x4587a0, CMach_CalcFloatMonadic);

CMachFloat CMach_CalcFloatConvertFromInt(UInt8 *type, CM_CInt64 val) {
    CMachFloat result;

    if (is_unsigned_m(type))
        result.value = CInt64_ConvertUToLongDouble_m(&val);
    else
        result.value = CInt64_ConvertToLongDouble_m(&val);
    return result;
}
HOOK(0x458940, CMach_CalcFloatConvertFromInt);

/* Type: type +0 (TYPEFLOAT 2), size +2 */
void CMach_InitFloatMem(UInt8 *type, CMachFloat val, void *mem) {
    if (type[0] == 2) {
        switch (*(SInt32 *)(type + 2)) {
        case 4:
            *(float *)mem = (float)val.value;
            return;
        case 8:
            *(double *)mem = val.value;
            return;
        }
        CError_Internal_m((const char *)0x5360c0, 0x27c);
        return;
    }
    CError_Internal_m((const char *)0x5360c0, 0x281);
}
HOOK(0x458a40, CMach_InitFloatMem);

/* ---- part 5: type tests, local alignment and the init list (from the exe) ---- */

/* unsigned types: pointers (0xb) and the unsigned integrals (char per copts 0x57db8b) */
Bool8 CMach_IsUnsignedType_(UInt8 *type) {
    static const Bool8 integral_unsigned[13] = {1, 2, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1};

    switch (*(SInt8 *)type) {
    case 1:
        if (type[6] > 12)
            return 0;
        if (type[6] == 1)
            return *(Bool8 *)0x57db8b;
        return integral_unsigned[type[6]];
    case 0xb:
        return 1;
    default:
        return 0;
    }
}
HOOK(0x4590c0, CMach_IsUnsignedType_);

/* a struct whose only member (members +0xa: next +0, type +4) is a float: that float type */
UInt8 *fn_459510(UInt8 *tstruct) {
    void **m = *(void ***)(tstruct + 0xa);

    if (!m || m[0])
        return 0;
    if (*(UInt8 *)m[1] == 2)
        return (UInt8 *)m[1];
    return 0;
}
HOOK(0x459510, fn_459510);

/* a class whose only member variable (ivars +0x16: next +4, type +0xc) is a float and whose bases
   (+0xe: next +0, base +4) have no member variables of access 0 or 2 (+1): that float type */
UInt8 *fn_459540(UInt8 *tclass) {
    UInt8 *ivar = *(UInt8 **)(tclass + 0x16);
    void **base;
    UInt8 *bivars;

    if (!ivar || *(void **)(ivar + 4))
        return 0;
    for (base = *(void ***)(tclass + 0xe); base; base = (void **)base[0]) {
        bivars = *(UInt8 **)((UInt8 *)base[1] + 0x16);
        if (bivars && (bivars[1] == 0 || bivars[1] == 2))
            return 0;
    }
    if (**(UInt8 **)(ivar + 0xc) == 2)
        return *(UInt8 **)(ivar + 0xc);
    return 0;
}
HOOK(0x459540, fn_459540);

UInt8 *fn_4595a0(UInt8 *type) {
    if (type[0] == 4)
        return fn_459510(type);
    if (type[0] == 5)
        return fn_459540(type);
    return 0;
}
HOOK(0x4595a0, fn_4595a0);

/* with copts 0x57db3d: the float of a one-float struct or class when it goes in an FPR (as in
   return_class) */
UInt8 *fn_4595d0(UInt8 *type) {
    UInt8 *t;
    SInt8 fpr = *(SInt8 *)0x57db2e;

    if (!*(Bool8 *)0x57db3d)
        return 0;
    t = fn_4595a0(type);
    if (!t || t[0] != 2)
        return 0;
    if ((t[6] == 0xd || *(SInt32 *)(t + 2) == 4) && (fpr & 1))
        return t;
    if ((t[6] == 0xf || *(SInt32 *)(t + 2) == 8) && (fpr & 2))
        return t;
    return 0;
}
HOOK(0x4595d0, fn_4595d0);

/* a large aggregate (copts 0x57db3d; bigger than 0x57c774) or a class 0x45f0d0 accepts */
ORIG_FN(Bool8, fn_45f0d0_m, 0x45f0d0, (void *type));

Bool8 CABI_PassByReference(UInt8 *type) {
    if (*(Bool8 *)0x57db3d && type &&
        (type[0] == 0xc || type[0] == 4 || type[0] == 5 || (type[0] == 0xa && *(SInt32 *)(type + 2) == 0xc)) &&
        *(SInt32 *)(type + 2) > *(SInt32 *)0x57c774)
        return 1;
    if (type[0] == 5 && fn_45f0d0_m(type))
        return 1;
    return 0;
}
HOOK(0x4594b0, CABI_PassByReference);

/* a local's alignment: its qualifier's (+0x10), else its type's (+0xc) raised to 4 for
   aggregates and 4-byte values, 16 for 16+ byte values with copts 0x57db5f, and 4 (8 with copts
   0x57db34 >= 3) for values over 4 bytes */
SInt16 local_alignment(UInt8 *obj) {
    UInt8 *type = *(UInt8 **)(obj + 0xc);
    SInt16 align = CMach_GetQUALalign(*(UInt32 *)(obj + 0x10));
    SInt32 size, lim;

    if (align)
        return align;
    align = CMach_GetTypeAlign(type);
    if ((type[0] == 4 || type[0] == 5 || type[0] == 0xc) && align < 4)
        align = 4;
    size = *(SInt32 *)(type + 2);
    if (size >= 0x10 && *(Bool8 *)0x57db5f && align <= 0x10)
        align = 0x10;
    if (size > 4) {
        lim = *(SInt16 *)0x57db34 >= 3 ? 8 : 4;
#ifdef RECOMP_NEGATIVE_CONTROL_CMACHINE6 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CMACHINE6 */
        lim = 16;
#endif
        if (align <= lim)
            align = (SInt16)lim;
    }
    if (size == 4 && align < 4)
        align = 4;
    return align;
}
HOOK(0x459000, local_alignment);

/* the init list (0x57b790): push a cleared 0x38-byte entry with +0 set, and pop */
ORIG_FN(void, memclrw_m, 0x4334d0, (void *ptr, SInt32 size));

void fn_4596a0(UInt8 *entry, void *x) {
    memclrw_m(entry, 0x38);
    *(void **)entry = x;
    *(UInt8 **)(entry + 0x34) = *(UInt8 **)0x57b790;
    *(UInt8 **)0x57b790 = entry;
}
HOOK(0x4596a0, fn_4596a0);

void fn_4596d0(UInt8 *entry) {
    *(UInt8 **)0x57b790 = *(UInt8 **)(entry + 0x34);
}
HOOK(0x4596d0, fn_4596d0);
