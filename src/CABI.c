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
