/*
 * Intrinsics.c — built-in intrinsic functions (mweppc src/compiler/Intrinsics.c), partial port.
 *
 * Ported so far (from the exe): the two intrinsic declarers (0x4baf20 with one parameter type,
 * 0x4baf80 with two). Each makes the function object (0x42dbf0) named by GetHashNameNodeExport,
 * stores it in the intrinsic table 0x565e26 with its index at +0x28, marks its type (flags +0x16
 * |= 0x200) and declares it (0x44b730).
 */
#include "orig.h"

ORIG_FN(void *, GetHashNameNodeExport, 0x432ef0, (const char *name));
ORIG_FN(UInt8 *, fn_42dbf0_1, 0x42dbf0, (void *rtype, void *name, SInt32 a, SInt32 nparams, void *p1));
ORIG_FN(UInt8 *, fn_42dbf0_2, 0x42dbf0,
        (void *rtype, void *name, SInt32 a, SInt32 nparams, void *p1, void *p2));
ORIG_FN(SInt32, fn_44b730, 0x44b730, (void *obj));

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))
#define intrinsics ((UInt8 **)0x565e26)

static SInt32 finish(SInt32 index, UInt8 *obj) {
    intrinsics[index] = obj;
    F(intrinsics[index], 0x28, SInt32) = index;
    F(F(intrinsics[index], 0xc, UInt8 *), 0x16, UInt32) |= 0x200;
    return fn_44b730(intrinsics[index]);
}

SInt32 Intrinsics_Declare1(SInt32 index, const char *name, void *type) {
    return finish(index, fn_42dbf0_1(type, GetHashNameNodeExport(name), 0, 1, type));
}
HOOK(0x4baf20, Intrinsics_Declare1);

SInt32 Intrinsics_Declare2(SInt32 index, const char *name, void *type) {
#ifdef RECOMP_NEGATIVE_CONTROL_INTRINSICS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_INTRINSICS */
    index ^= 1;
#endif
    return finish(index, fn_42dbf0_2(type, GetHashNameNodeExport(name), 0, 2, type, type));
}
HOOK(0x4baf80, Intrinsics_Declare2);

/* whether a call's function (+0x18) is an EOBJREF (0x38) of a DFUNC (datatype 3) object whose
   type is marked intrinsic (flags +0x16 & 0x200) */
int Intrinsics_IsIntrinsicCall(UInt8 *expr) {
    UInt8 *func = F(expr, 0x18, UInt8 *);
    UInt8 *obj;

    if (func[0] != 0x38)
        return 0;
    obj = F(func, 0x18, UInt8 *);
    if (obj[2] != 3)
        return 0;
    return (F(F(obj, 0xc, UInt8 *), 0x16, UInt32) & 0x200) ? 1 : 0;
}
HOOK(0x4bb310, Intrinsics_IsIntrinsicCall);
