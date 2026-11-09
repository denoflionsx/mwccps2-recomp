/*
 * CABI.c — the C++ ABI: class layout, vtables, this (mweppc src/compiler/frontend/CABI.c),
 * partial port.
 *
 * Not matched by gcmatch, but these follow mweppc's logic: CABI_GetBaseSize,
 * CABI_GetBaseVTableSize, CABI_ApplyClassFlags, CABI_ThisArg (ours asserts on the second argument,
 * whose type must be TYPEINT 1, and returns it) and CABI_ThisArgExpr (create_objectnode of it; inline
 * in mweppc's CABI_MakeThisExpr).
 *
 * TypeClass: size +2, vbases +0x12, vtable +0x1e (VTable.size +0xc). VClassList: next +0, base +4,
 * offset +8. Object: type +0xc, flags (byte) +0x16 (OBJECT_INTERNAL 0x10, OBJECT_IMPORT 0x20,
 * OBJECT_EXPORT 0x40). copts.vbase_abi_v2 0x57db9e. arguments 0x57caa0 (ObjectList {next, object}).
 */
#include "orig.h"

typedef UInt8 Bool8;

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));
ORIG_FN(void *, create_objectnode, 0x4f0200, (void *obj));

#define vbase_abi_v2 ORIG_VAR(Bool8, 0x57db9e)
#define arguments ORIG_VAR(void **, 0x57caa0)

SInt32 CABI_GetBaseSize(UInt8 *tclass) {
    SInt32 size = F(tclass, 2, SInt32);

    if (vbase_abi_v2 && F(tclass, 0x12, void *))
        return F(F(tclass, 0x12, UInt8 *), 8, SInt32);
    return size;
}
HOOK(0x452a90, CABI_GetBaseSize);

SInt32 CABI_GetBaseVTableSize(UInt8 *tclass) {
    SInt32 size = F(F(tclass, 0x1e, UInt8 *), 0xc, SInt32);
    void **vbase;

    for (vbase = F(tclass, 0x12, void **); vbase; vbase = vbase[0])
        if (F(vbase[1], 0x1e, void *))
            size -= CABI_GetBaseVTableSize(vbase[1]);
    return size;
}
HOOK(0x452f20, CABI_GetBaseVTableSize);

void CABI_ApplyClassFlags(UInt8 *obj, UInt8 flags) {
    if (flags & 1)
        obj[0x16] |= 0x10;
    if (flags & 2)
        obj[0x16] |= 0x20;
    if (flags & 4)
        obj[0x16] |= 0x40;
}
HOOK(0x452f50, CABI_ApplyClassFlags);

void *CABI_ThisArg(void) {
    if (!(arguments && arguments[0] && F(F(((void **)arguments[0])[1], 0xc, UInt8 *), 0, UInt8) == 1))
        CError_Internal((const char *)0x535c00 /* "CABI.c" */, 976);
    return ((void **)arguments[0])[1];
}
HOOK(0x4535c0, CABI_ThisArg);

void *CABI_ThisArgExpr(void) {
#ifdef RECOMP_NEGATIVE_CONTROL_CABI /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CABI */
    return create_objectnode(arguments[1]); /* the first argument, not the second */
#endif
    return create_objectnode(CABI_ThisArg());
}
HOOK(0x453600, CABI_ThisArgExpr);

/* CABI_GetSizeTType: size_t is unsigned int (stunsignedint 0x536068). */
void *CABI_GetSizeTType(void) {
    return (void *)0x536068;
}
HOOK(0x4529e0, CABI_GetSizeTType);

/* ---- part 2: the this argument and a vtable share (from the exe) ---- */

/* the first argument (arguments 0x57caa0: next +0, object +4), which must be a pointer (internal
   error 0x3a3) */
#define arguments_ab ORIG_VAR(void **, 0x57caa0)

void *CABI_GetThisArg(void) {
    if (!arguments_ab || F(F(arguments_ab[1], 0xc, UInt8 *), 0, UInt8) != 0xb)
        CError_Internal((const char *)0x535c00 /* "CABI.c" */, 0x3a3);
    return arguments_ab[1];
}
HOOK(0x4534f0, CABI_GetThisArg);

/* the class shares the vtable offset (+8) of its first non-virtual base (ClassList: next +0,
   base +4, offset +8, virtual (byte) +0x11) with a vtable (+0x1e) at offset 0 */
Bool8 fn_452e00(UInt8 *tclass) {
    void **base;
    UInt8 *vtable;

    for (base = F(tclass, 0xe, void **); base; base = (void **)base[0]) {
        if (F(base, 0x11, UInt8))
            continue;
        vtable = F(base[1], 0x1e, UInt8 *);
        if (!vtable || base[2])
            continue;
#ifndef RECOMP_NEGATIVE_CONTROL_CABI2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CABI2 */
        F(F(tclass, 0x1e, UInt8 *), 8, void *) = F(vtable, 8, void *);
#endif
        return 1;
    }
    return 0;
}
HOOK(0x452e00, fn_452e00);

/* ---- part 3: base layout, the zero-offset base member search, this expressions and operator
   new (not matched by gcmatch; mweppc's logic) ---- */

/* TypeClass: size +2, nspace +6, bases +0xe, vbases +0x12, vtable +0x1e, sominfo +0x22, flags
   +0x26 (1 handle object, 0x1000 empty). ClassList: next +0, base +4, offset +8, voffset +0xc,
   is_virtual +0x11. VClassList: next +0, base +4. Object: otype +0 (5), datatype +2 (DVFUNC 4),
   name +8, type +0xc. NameResult (0x28 bytes): object +0x10, objects +0x14. */
ORIG_FN(SInt16, CMach_MemberAlignValue_a, 0x458cb0, (void *type, SInt32 size));
ORIG_FN(void **, CScope_FindName_a, 0x44af70, (void *nspace, void *name));
ORIG_FN(UInt8, CClass_GetOverrideKind_a, 0x45e6b0, (void *a, void *b, Bool8 errorflag));
ORIG_FN(void *, makemonadicnode_a, 0x4ef320, (void *expr, UInt8 ty));
ORIG_FN(void *, makediadicnode_a, 0x4ef360, (void *left, void *right, UInt8 ty));
ORIG_FN(void *, intconstnode_a, 0x4ef2c0, (void *type, SInt32 value));
ORIG_FN(void *, CSOM_SOMSelfObjectExpr_a, 0x452400, (void *tclass));
ORIG_FN(void *, CMangler_OperatorName_a, 0x44eea0, (SInt16 token));
ORIG_FN(Bool8, CScope_FindClassMemberObject_a, 0x44dc60, (void *tclass, UInt8 *result, void *name));
ORIG_FN(Bool8, CABI_IsOperatorNew_a, 0x453d00, (void *obj));

void CABI_AllocateBases(void *layout, UInt8 *tclass) {
    Bool8 prevEmpty = 0;
    SInt32 size = F(tclass, 2, SInt32);
    UInt8 *base, *baseclass;
    void **vbase;

    for (base = F(tclass, 0xe, UInt8 *); base; base = F(base, 0, UInt8 *)) {
        if (base[0x11])
            continue;
        baseclass = F(base, 4, UInt8 *);
        if (!(F(baseclass, 0x26, UInt16) & 0x1000)) {
            F(base, 8, SInt32) = size + CMach_MemberAlignValue_a(baseclass, size);
#ifdef RECOMP_NEGATIVE_CONTROL_CABI3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CABI3 */
            F(base, 8, SInt32) += 4; /* bases laid out 4 bytes late */
#endif
            if (vbase_abi_v2) {
                size = F(base, 8, SInt32) + CABI_GetBaseSize(baseclass);
            } else {
                size = F(base, 8, SInt32) + F(baseclass, 2, SInt32);
                for (vbase = F(baseclass, 0x12, void **); vbase; vbase = (void **)vbase[0])
                    size -= F(vbase[1], 2, SInt32);
            }
            prevEmpty = 0;
        } else {
            if (prevEmpty)
                size++;
            F(base, 8, SInt32) = size;
            prevEmpty = 1;
        }
    }
    F(tclass, 2, SInt32) = size;
}
HOOK(0x452ac0, CABI_AllocateBases);

UInt8 *CABI_FindZeroVirtualBaseMember(UInt8 *tclass, UInt8 *obj) {
    UInt8 *base, *chk;
    void **nsol;

    for (base = F(tclass, 0xe, UInt8 *); base; base = F(base, 0, UInt8 *)) {
        if (!base[0x11] && !F(base, 8, SInt32) && !F(base, 0xc, SInt32) && F(F(base, 4, UInt8 *), 0x1e, void *)) {
            for (nsol = CScope_FindName_a(F(F(base, 4, UInt8 *), 6, void *), F(obj, 8, void *)); nsol;
                 nsol = (void **)nsol[0]) {
                chk = (UInt8 *)nsol[1];
                if (chk[0] == 5 && chk[2] == 4 &&
                    CClass_GetOverrideKind_a(F(chk, 0xc, void *), F(obj, 0xc, void *), 0) == 1)
                    return chk;
            }
            if ((chk = CABI_FindZeroVirtualBaseMember(F(base, 4, UInt8 *), obj)))
                return chk;
        }
    }
    return 0;
}
HOOK(0x452e40, CABI_FindZeroVirtualBaseMember);

void *CABI_MakeThisExpr(UInt8 *tclass, SInt32 offset) {
    UInt8 *expr;

    if (tclass) {
        if (!F(tclass, 0x22, void *)) {
            expr = create_objectnode(CABI_GetThisArg());
            if (F(tclass, 0x26, UInt16) & 1)
                expr = makemonadicnode_a(expr, 4); /* EINDIRECT */
        } else {
            expr = CSOM_SOMSelfObjectExpr_a(tclass);
        }
    } else {
        expr = create_objectnode(CABI_GetThisArg());
        F(expr, 0xa, void *) = (void *)0x532090; /* void_ptr */
    }
    if (offset)
        expr = makediadicnode_a(expr, intconstnode_a((void *)0x536068 /* stunsignedlong */, offset), 0xf);
    return expr;
}
HOOK(0x453530, CABI_MakeThisExpr);

void *CABI_ConstructorCallsNew(UInt8 *tclass) {
    UInt8 pr[0x28];
    void **nsol;

    if (!F(tclass, 0x22, void *) && (F(tclass, 0x26, UInt16) & 1)) {
        if (CScope_FindClassMemberObject_a(tclass, pr, CMangler_OperatorName_a(0x146 /* TK_NEW */))) {
            if (F(pr, 0x10, void *)) {
                if (CABI_IsOperatorNew_a(F(pr, 0x10, void *)))
                    return F(pr, 0x10, void *);
            } else {
                for (nsol = F(pr, 0x14, void **); nsol; nsol = (void **)nsol[0]) {
                    if (CABI_IsOperatorNew_a(nsol[1]))
                        return nsol[1];
                }
            }
        }
        return *(void **)0x57cac8; /* newh_func */
    }
    return 0;
}
HOOK(0x453d70, CABI_ConstructorCallsNew);
