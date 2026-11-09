/*
 * IroCSE.c — common subexpressions in the IR optimizer (mweppc src/compiler/optimizer/IroCSE.c),
 * partial port.
 *
 * Ported so far: IRO_FindDepends_NoAlloc (byte-identical to mweppc's GameCube 2.0 compiler),
 * IRO_FindDepends and IRO_DoesNotHaveVectorOperand (not matched by gcmatch; the same logic). The
 * walker callback VecAct (0x4c4c90) is reached only by pointer; ours passes its own copy.
 *
 * IRO_Depends 0x57c894, IRO_NumVars 0x57c8c0, IRO_CouldError 0x57e13d, IRO_NotSubable 0x57e0bd,
 * IRO_IsVolatile 0x57e159, HasVectorOperand 0x5671f0. IROLinear.flags +2 (IROLF_VecOpBase 0x10000).
 */
#include "orig.h"

typedef UInt8 Bool8;

ORIG_FN(void, Bv_Clear, 0x50c630, (void *bv));
ORIG_FN(void, Bv_AllocVector, 0x50c4a0, (void **bv, SInt32 size));
ORIG_FN(void, IRO_DependsOnForDataFlow, 0x4c4a30, (void *linear, Bool8 flag));
ORIG_FN(void, IRO_DependsOn, 0x4c4820, (void *linear, Bool8 flag));
ORIG_FN(void, IRO_WalkTree, 0x4bf570, (void *linear, void (*fn)(UInt8 *, Bool8)));

#define IRO_Depends ORIG_VAR(void *, 0x57c894)
#define IRO_NumVars ORIG_VAR(SInt32, 0x57c8c0)
#define IRO_CouldError ORIG_VAR(Bool8, 0x57e13d)
#define IRO_NotSubable ORIG_VAR(Bool8, 0x57e0bd)
#define IRO_IsVolatile ORIG_VAR(Bool8, 0x57e159)
#define HasVectorOperand ORIG_VAR(Bool8, 0x5671f0)

void IRO_FindDepends_NoAlloc(void *linear) {
    Bv_Clear(IRO_Depends);
    IRO_CouldError = 0;
    IRO_NotSubable = 0;
    IRO_IsVolatile = 0;
    IRO_DependsOnForDataFlow(linear, 0);
}
HOOK(0x4c4c30, IRO_FindDepends_NoAlloc);

void IRO_FindDepends(void *linear) {
    Bv_AllocVector(&IRO_Depends, IRO_NumVars + 1);
    IRO_CouldError = 0;
    IRO_NotSubable = 0;
#ifdef RECOMP_NEGATIVE_CONTROL_IROCSE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROCSE */
    IRO_NotSubable = 1;
    return;
#endif
    IRO_DependsOn(linear, 0);
}
HOOK(0x4c4c60, IRO_FindDepends);

static void VecAct(UInt8 *linear, Bool8 isFirst) {
    if (!isFirst && (*(UInt32 *)(linear + 2) & 0x10000))
        HasVectorOperand = 1;
}

Bool8 IRO_DoesNotHaveVectorOperand(void *linear) {
    HasVectorOperand = 0;
    IRO_WalkTree(linear, VecAct);
    return !HasVectorOperand;
}
HOOK(0x4c4cb0, IRO_DoesNotHaveVectorOperand);

/* ---- part 2: removing an expression from the CSE list (from the exe) ---- */

/* IRO_FirstExpr 0x57c768 (next +0x26); the expression's linear node (+4) loses its expression
   link (+0x12). A missing expression is an internal error (line 0x329). */
ORIG_FN(void, CError_Internal_c, 0x457230, (const char *file, int line));
#define IRO_FirstExpr ORIG_VAR(UInt8 *, 0x57c768)

void IRO_RemoveExpr(UInt8 *expr) {
    UInt8 *prev = 0, *p = IRO_FirstExpr;

    while (p != expr) {
        prev = p;
        p = *(UInt8 **)(p + 0x26);
        if (!p)
            CError_Internal_c((const char *)0x53feb8 /* "IroCSE.c" */, 0x329);
    }
    *(UInt32 *)(*(UInt8 **)(expr + 4) + 0x12) = 0;
    if (prev)
        *(UInt8 **)(prev + 0x26) = *(UInt8 **)(expr + 0x26);
    else
        IRO_FirstExpr = *(UInt8 **)(expr + 0x26);
}
HOOK(0x4c4e50, IRO_RemoveExpr);

/* ---- part 3: AddExpr (mweppc's logic, from the exe) ---- */

/* IROExpr (0x2a bytes): +0 0, index (short) +2 (++IRO_NumExprs 0x57ab48), linear +4, +8 0,
   node index +0xc, depends +0x10 (IRO_Depends 0x57c894 after IRO_FindDepends), +0x14 0, flags from
   0x57e13d (+0x18) and 0x57e0bd (+0x19), next +0x26. IRO_FirstExpr 0x57c768, IRO_LastExpr
   0x57b7d0. Only used (flags +2 & 2) expressions that 0x4e2370 accepts and that have no vector
   operand; the linear's +0x12 points back. */
ORIG_FN(UInt8, fn_4e2370_c, 0x4e2370, (UInt8 *n));
ORIG_FN(void *, oalloc_c, 0x4333a0, (SInt32 size));

void IRO_AddExpression(UInt8 *n, SInt32 index) {
    UInt8 *e;

    if (!(*(UInt32 *)(n + 2) & 2))
        return;
    if (!fn_4e2370_c(n))
        return;
    if (!IRO_DoesNotHaveVectorOperand(n))
        return;
    e = oalloc_c(0x2a);
    e[0] = 0;
    (*(SInt32 *)0x57ab48)++;
    *(UInt16 *)(e + 2) = *(UInt16 *)0x57ab48;
    *(UInt8 **)(e + 4) = n;
    *(void **)(e + 8) = 0;
    *(SInt32 *)(e + 0xc) = index;
    IRO_FindDepends(n);
    *(void **)(e + 0x10) = *(void **)0x57c894;
    e[0x19] = *(UInt8 *)0x57e0bd;
    e[0x18] = *(UInt8 *)0x57e13d;
    *(void **)(e + 0x26) = 0;
    *(void **)(e + 0x14) = 0;
#ifdef RECOMP_NEGATIVE_CONTROL_IROCSE3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROCSE3 */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "IROCSE3@"); /* marks runs */
#endif
    if (*(UInt8 **)0x57c768)
        *(UInt8 **)(*(UInt8 **)0x57b7d0 + 0x26) = e;
    else
        *(UInt8 **)0x57c768 = e;
    *(UInt8 **)0x57b7d0 = e;
    *(UInt8 **)(n + 0x12) = e;
}
HOOK(0x4c4ce0, IRO_AddExpression);
