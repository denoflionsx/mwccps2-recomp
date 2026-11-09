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

/* ---- part 2: setup, conversions, tests, compares and the adder (from the exe; mweppc's names,
   on four words) ---- */

typedef unsigned char Bool8;
typedef struct { UInt32 w[4]; } CInt128;

/* cint64_negone 0x56e6b0 (all ones), cint64_max 0x56b508, cint64_min 0x56b4f8, cint64_init 0x56b4f4 */
SInt32 CInt64_Init(void) {
    SInt32 i;

    ORIG_VAR(SInt32, 0x56b4f4) = 1;
    for (i = 0; i < 4; i++) {
        ((UInt32 *)0x56e6b0)[i] = 0xffffffff;
        ((UInt32 *)0x56b508)[i] = 0xffffffff;
    }
    ((UInt32 *)0x56b508)[0] = 0x7fffffff;
    ((UInt32 *)0x56b4f8)[0] = 0x80000000;
    return i;
}
HOOK(0x4bcf70, CInt64_Init);

SInt32 CInt64_SetLong(UInt32 *value, SInt32 l) {
    UInt32 fill = l < 0 ? 0xffffffff : 0;
    int i;

    for (i = 0; i < 3; i++)
        value[i] = fill;
    value[3] = l;
    return l;
}
HOOK(0x4bcfb0, CInt64_SetLong);

UInt32 CInt64_SetULong(UInt32 *value, UInt32 l) {
    int i;

    for (i = 0; i < 3; i++)
        value[i] = 0;
    value[3] = l;
    return l;
}
HOOK(0x4bcfe0, CInt64_SetULong);

Bool8 CInt64_IsZero(const UInt32 *value) {
    int i;

    for (i = 0; i < 4; i++) {
        if (value[i])
            return 0;
    }
    return 1;
}
HOOK(0x4bd000, CInt64_IsZero);

Bool8 CInt64_IsOne(const UInt32 *value) {
    int i;

    if (value[3] != 1)
        return 0;
    for (i = 0; i < 3; i++) {
        if (value[i])
            return 0;
    }
    return 1;
}
HOOK(0x4bd020, CInt64_IsOne);

int CInt64_UnsignedCompare(const UInt32 *lhs, const UInt32 *rhs) {
    int i;

    for (i = 0; i < 4; i++) {
        if (lhs[i] != rhs[i])
            return lhs[i] < rhs[i] ? -1 : 1;
    }
    return 0;
}
HOOK(0x4bd0a0, CInt64_UnsignedCompare);

int CInt64_SignedCompare(const UInt32 *lhs, const UInt32 *rhs) {
    CInt128 a = *(const CInt128 *)lhs;
    CInt128 b = *(const CInt128 *)rhs;

    a.w[0] ^= 0x80000000;
    b.w[0] ^= 0x80000000;
#ifdef RECOMP_NEGATIVE_CONTROL_CINT64 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINT64 */
    return CInt64_UnsignedCompare(b.w, a.w); /* reversed */
#endif
    return CInt64_UnsignedCompare(a.w, b.w);
}
HOOK(0x4bd050, CInt64_SignedCompare);

/* logical not: 1 when zero */
CInt128 CInt64_Not(CInt128 input) {
    CInt128 result;

    CInt64_SetLong(result.w, CInt64_IsZero(input.w));
    return result;
}
HOOK(0x4bd0e0, CInt64_Not);

CInt128 CInt64_Inv(CInt128 input) {
    int i;

    for (i = 0; i < 4; i++)
        input.w[i] = ~input.w[i];
    return input;
}
HOOK(0x4bd120, CInt64_Inv);

/* adds rhs and a carry into lhs, low word (+0xc) first; returns the carry out. The carry of each
   word is worked out from the top bits as the exe does. */
int CInt64_AddWithCarry(UInt32 *lhs, const UInt32 *rhs, int carry) {
    int i;

    for (i = 3; i >= 0; i--) {
        UInt32 x = lhs[i], y = rhs[i];

        if ((x & 0x80000000) && (y & 0x80000000)) {
            x += y + carry;
            carry = 1;
        } else if (!(x & 0x80000000) && !(y & 0x80000000)) {
            x += carry + y;
            carry = 0;
        } else {
            x += y;
            if (carry) {
                carry = (x & 0x80000000) ? 0 : 1;
                x++;
                if (!(x & 0x80000000))
                    carry = 1;
            } else {
                carry = (x & 0x80000000) ? 0 : 1;
            }
        }
        lhs[i] = x;
    }
    return carry;
}
HOOK(0x4bd190, CInt64_AddWithCarry);

CInt128 CInt64_Add(CInt128 lhs, CInt128 rhs) {
    CInt64_AddWithCarry(lhs.w, rhs.w, 0);
    return lhs;
}
HOOK(0x4bd160, CInt64_Add);
