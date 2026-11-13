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

/* ---- part 2: runtime helper functions and the name setup (from the exe) ---- */

typedef unsigned char Bool8;

/* runtime functions returning unsigned long (stunsignedlong 0x536078) with 0 to 3 parameters */
ORIG_FN(UInt8 *, fn_42dbf0_0, 0x42dbf0, (void *rtype, void *name, SInt32 a, SInt32 nparams));
ORIG_FN(UInt8 *, fn_42dbf0_3, 0x42dbf0,
        (void *rtype, void *name, SInt32 a, SInt32 nparams, void *p1, void *p2, void *p3));
#define stunsignedlong ((void *)0x536078)

UInt8 *Intrinsics_RuntimeFunc0(const char *name) {
    return fn_42dbf0_0(stunsignedlong, GetHashNameNodeExport(name), 0, 0);
}
HOOK(0x4bac60, Intrinsics_RuntimeFunc0);

UInt8 *Intrinsics_RuntimeFunc1(const char *name, void *p1) {
    return fn_42dbf0_1(stunsignedlong, GetHashNameNodeExport(name), 0, 1, p1);
}
HOOK(0x4bac80, Intrinsics_RuntimeFunc1);

UInt8 *Intrinsics_RuntimeFunc2(const char *name, void *p1, void *p2) {
    return fn_42dbf0_2(stunsignedlong, GetHashNameNodeExport(name), 0, 2, p1, p2);
}
HOOK(0x4bacb0, Intrinsics_RuntimeFunc2);

UInt8 *Intrinsics_RuntimeFunc3(const char *name, void *p1, void *p2, void *p3) {
    return fn_42dbf0_3(stunsignedlong, GetHashNameNodeExport(name), 0, 3, p1, p2, p3);
}
HOOK(0x4bace0, Intrinsics_RuntimeFunc3);

/* names the intrinsic objects: the 0x3bc entries of 0x56e976 (0x2c bytes: name +0, intrinsic
   index +4) give the name (+8) of intrinsics[index] */
Bool8 Intrinsics_SetupNames(Bool8 flag) {
    UInt16 i;

    if (flag) {
        for (i = 0; i < 0x3bc; i++) {
            UInt8 *entry = (UInt8 *)0x56e976 + i * 0x2c;
            if (F(entry, 0, const char *)) {
#ifdef RECOMP_NEGATIVE_CONTROL_INTRINSICS2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_INTRINSICS2 */
                if (!i)
        ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "INTRINSICS2@"); /* marks runs */
#endif
                F(intrinsics[F(entry, 4, SInt32)], 8, void *) = GetHashNameNodeExport(F(entry, 0, const char *));
            }
        }
    }
    return 1;
}
HOOK(0x4bcda0, Intrinsics_SetupNames);

/* names the 16 runtime objects of 0x53f46c (0x10 bytes: name +0, object +0xc); the first is also
   kept at 0x57b6e4 */
void *Intrinsics_SetupRuntimeNames(void) {
    int i;

    for (i = 0; i < 0x10; i++) {
        UInt8 *entry = (UInt8 *)0x53f46c + i * 0x10;
        F(F(entry, 0xc, UInt8 *), 8, void *) = GetHashNameNodeExport(F(entry, 0, const char *));
    }
    ORIG_VAR(void *, 0x57b6e4) = ORIG_VAR(void *, 0x53f478);
    return ORIG_VAR(void *, 0x57b6e4);
}
HOOK(0x4bcf30, Intrinsics_SetupRuntimeNames);
