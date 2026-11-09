/*
 * IroTransform.c — the optimizer's expression simplifications (mweppc
 * src/compiler/optimizer/IroTransform.c), partial port from the exe.
 *
 * Ported so far: the per-node simplifier driver IRO_SimplifyNodes, IRO_IsRegisterLocal and two
 * node tests.
 *
 * IROLinear: type +0 (operand 1, monadic 2, diadic 3), next +0x34. ENode: type +0, monadic +0x18.
 */
#include "orig.h"

typedef UInt8 Bool8;

/* ops 0x1c, 0x1d (&&, ||), 0x29 (comma), 0x35 (?:) and 0x4b: their operands are evaluated conditionally or in order */
Bool8 fn_4c9190(UInt8 t) {
    return t == 0x1c || t == 0x1d || t == 0x29 || t == 0x35 || t == 0x4b;
}
HOOK(0x4c9190, fn_4c9190);

ORIG_FN(void, fn_4c91c0_t, 0x4c91c0, (UInt8 *n));
ORIG_FN(void, fn_4c9210_t, 0x4c9210, (UInt8 *n));
ORIG_FN(void, fn_4c9340_t, 0x4c9340, (UInt8 *n));
ORIG_FN(void, fn_4c93a0_t, 0x4c93a0, (UInt8 *n));
ORIG_FN(void, IRO_CheckForUserBreak_t, 0x4c0570, (void));

void IRO_SimplifyNodes(void) {
    UInt8 *n;

    for (n = *(UInt8 **)0x57c898; n; n = *(UInt8 **)(n + 0x34)) {
        switch (n[0]) {
        case 1:
            fn_4c91c0_t(n);
            break;
        case 2:
            fn_4c91c0_t(n);
            fn_4c9210_t(n);
#ifdef RECOMP_NEGATIVE_CONTROL_IROTRANSFORM /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROTRANSFORM */
            ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "IROTRANSFORM@"); /* marks runs */
#endif
            fn_4c9340_t(n);
            break;
        case 3:
            fn_4c93a0_t(n);
            break;
        }
    }
    IRO_CheckForUserBreak_t();
}
HOOK(0x4c93f0, IRO_SimplifyNodes);

/* an expression (through one monadic node of type 4) of type 0x31 */
int fn_4ca1c0(UInt8 *e) {
    if (e[0] == 4)
        e = *(UInt8 **)(e + 0x18);
    return e[0] == 0x31;
}
HOOK(0x4ca1c0, fn_4ca1c0);

/* a local (datatype +2 == 1) with VarInfo (+0x28) that may live in a register (noregister +0x2a
   clear) */
int IRO_IsRegisterLocal(UInt8 *obj) {
    UInt8 *vi;

    if (obj[2] == 1 && (vi = *(UInt8 **)(obj + 0x28)))
        return vi[0x2a] == 0;
    return 0;
}
HOOK(0x4cae60, IRO_IsRegisterLocal);

/* Nops a node without side effects (IRO_NopNonSideEffects 0x4bf550) unless it is used (flags +2
   & 2): operands (type 1) always, monadic, diadic, and types 10 and 11 unless their op (+1) is
   one of fn_4c9190's. */
ORIG_FN(void, IRO_NopNonSideEffects_t, 0x4bf550, (UInt8 *n, SInt32 level));
ORIG_FN(UInt8 *, IRO_LocateFather_t, 0x4bfca0, (UInt8 *n));

void fn_4c91c0(UInt8 *n) {
    if (*(UInt32 *)(n + 2) & 2)
        return;
    switch (n[0]) {
    case 1:
        IRO_NopNonSideEffects_t(n, 0);
        break;
    case 2:
    case 3:
    case 10:
    case 11:
        if (!fn_4c9190(n[1]))
            IRO_NopNonSideEffects_t(n, 0);
        break;
    }
}
HOOK(0x4c91c0, fn_4c91c0);

/* A logical not (monadic op 5) that is the right operand of a comparison (ops 0xd..0x10, 0x22,
   0x23) folds into it: the comparison takes the complementary op (table 0x5674ba) and the not's
   operand (+0x20); the not becomes a nop (type 0). */
void fn_4c9340(UInt8 *n) {
    UInt8 *p;
    UInt8 op;

    if (n[0] != 2 || n[1] != 5)
        return;
    p = IRO_LocateFather_t(n);
    if (!p || p[0] != 3 || *(UInt8 **)(p + 0x24) != n)
        return;
    op = p[1];
    if (!((op >= 0xd && op <= 0x10) || op == 0x22 || op == 0x23))
        return;
    p[1] = ((UInt8 *)0x5674ba)[op];
    n[0] = 0;
    *(UInt8 **)(p + 0x24) = *(UInt8 **)(n + 0x20);
}
HOOK(0x4c9340, fn_4c9340);

/* the diadic simplifications in order */
ORIG_FN(void, fn_4c6ee0_t, 0x4c6ee0, (UInt8 *n));
ORIG_FN(void, fn_4c7470_t, 0x4c7470, (UInt8 *n));
ORIG_FN(void, fn_4c7a60_t, 0x4c7a60, (UInt8 *n));
ORIG_FN(void, fn_4c7b40_t, 0x4c7b40, (UInt8 *n));
ORIG_FN(void, fn_4c7d20_t, 0x4c7d20, (UInt8 *n));
ORIG_FN(void, fn_4c7e80_t, 0x4c7e80, (UInt8 *n));
ORIG_FN(void, fn_4c8210_t, 0x4c8210, (UInt8 *n));
ORIG_FN(void, fn_4c8810_t, 0x4c8810, (UInt8 *n));
ORIG_FN(void, fn_4c8f20_t, 0x4c8f20, (UInt8 *n));

void fn_4c93a0(UInt8 *n) {
    fn_4c91c0(n);
    fn_4c6ee0_t(n);
    fn_4c7470_t(n);
    fn_4c7a60_t(n);
    fn_4c7b40_t(n);
    fn_4c7d20_t(n);
    fn_4c7e80_t(n);
    fn_4c8210_t(n);
    fn_4c8810_t(n);
    fn_4c8f20_t(n);
}
HOOK(0x4c93a0, fn_4c93a0);

/* Walks an ENode tree bottom up, calling 0x4c98f0 on every node: monadic types 0..8, 0x30, 0x31
   (operand +0x18), diadic 9..0x2f and 0x3a (+0x18, +0x1c), the conditional 0x35 (+0x18, +0x1c,
   +0x20), calls 0x36/0x37 (function +0x18, arguments +0x1c: next +0, node +4); leaves 0x32..0x34,
   0x38, 0x39 and types above 0x3a. */
ORIG_FN(void, fn_4c98f0_t, 0x4c98f0, (UInt8 *e));

void fn_4c9c50(UInt8 *e) {
    void **arg;
    UInt8 t = e[0];

    if (t <= 8 || t == 0x30 || t == 0x31) {
        fn_4c9c50(*(UInt8 **)(e + 0x18));
    } else if (t <= 0x2f || t == 0x3a) {
        fn_4c9c50(*(UInt8 **)(e + 0x18));
        fn_4c9c50(*(UInt8 **)(e + 0x1c));
    } else if (t == 0x35) {
        fn_4c9c50(*(UInt8 **)(e + 0x18));
        fn_4c9c50(*(UInt8 **)(e + 0x1c));
        fn_4c9c50(*(UInt8 **)(e + 0x20));
    } else if (t == 0x36 || t == 0x37) {
        fn_4c9c50(*(UInt8 **)(e + 0x18));
        for (arg = *(void ***)(e + 0x1c); arg; arg = (void **)arg[0])
            fn_4c9c50((UInt8 *)arg[1]);
    }
    fn_4c98f0_t(e);
}
HOOK(0x4c9c50, fn_4c9c50);

/* Statement: next +0, type +4 (4..8 carry an expression +0xa). Each expression goes through
   0x4c9850 (its result replaces it), 0x4c9e70 and the walk above; type 4 statements first get
   0x4c9f60 (stmt, &expr). */
ORIG_FN(void, fn_4c9f60_t, 0x4c9f60, (UInt8 *stmt, UInt8 **expr));
ORIG_FN(UInt8 *, fn_4c9850_t, 0x4c9850, (UInt8 *e));
ORIG_FN(void, fn_4c9e70_t, 0x4c9e70, (UInt8 *e));

void IRO_PreTransformTrees(UInt8 *stmt) {
    UInt8 *next;

    for (; stmt; stmt = next) {
        next = *(UInt8 **)stmt;
        if (stmt[4] < 4 || stmt[4] > 8 || !*(UInt8 **)(stmt + 0xa))
            continue;
        if (stmt[4] == 4)
            fn_4c9f60_t(stmt, (UInt8 **)(stmt + 0xa));
        *(UInt8 **)(stmt + 0xa) = fn_4c9850_t(*(UInt8 **)(stmt + 0xa));
        fn_4c9e70_t(*(UInt8 **)(stmt + 0xa));
        fn_4c9c50(*(UInt8 **)(stmt + 0xa));
    }
    IRO_CheckForUserBreak_t();
}
HOOK(0x4ca0d0, IRO_PreTransformTrees);
