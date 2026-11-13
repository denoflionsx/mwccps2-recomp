/*
 * IroLinearForm.c — the IR optimizer's linear form (mweppc src/compiler/optimizer/IroLinearForm.c),
 * partial port.
 *
 * Ported so far, not matched by gcmatch but with mweppc's logic: IRO_NewENode and
 * IRO_RenumberInts. (The address map names 0x455800 IRO_RenumberInts by mistake; that is
 * CError_ResetErrorSkip.)
 *
 * IROLinear: index (short) +8, next +0x34. IRO_FirstLinear 0x57c898, IRO_NumLinear 0x57ab84.
 */
#include "orig.h"

ORIG_FN(void *, lalloc, 0x433320, (SInt32 size));
ORIG_FN(void *, memset_, 0x404d40, (void *d, int c, UInt32 n));

#define IRO_FirstLinear ORIG_VAR(UInt8 *, 0x57c898)
#define IRO_NumLinear ORIG_VAR(SInt32, 0x57ab84)

UInt8 *IRO_NewENode(UInt8 nodetype) {
    UInt8 *enode = lalloc(0x28);
    memset_(enode, 0, 0x28);
    enode[0] = nodetype;
    return enode;
}
HOOK(0x4c3be0, IRO_NewENode);

void IRO_RenumberInts(void) {
    UInt8 *linear = IRO_FirstLinear;

    IRO_NumLinear = 0;
    while (linear) {
#ifdef RECOMP_NEGATIVE_CONTROL_IROLINEARFORM /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROLINEARFORM */
        *(SInt16 *)(linear + 8) = 0;
        IRO_NumLinear++;
#else
        *(SInt16 *)(linear + 8) = (SInt16)IRO_NumLinear++;
#endif
        linear = *(UInt8 **)(linear + 0x34);
    }
}
HOOK(0x4c40c0, IRO_RenumberInts);

/* ---- part 2: the linear-list flag reset walk (from the exe) ---- */

/* IROLinear: flags +2 (bit 2 cleared on every node first), next +0x34; then 0x4c3850 runs on
   each node in order. */
ORIG_FN(void, fn_4c3850, 0x4c3850, (void *linear));

void fn_4c3820(UInt8 *first) {
    UInt8 *nd;

    for (nd = first; nd; nd = *(UInt8 **)(nd + 0x34))
        *(UInt32 *)(nd + 2) &= ~2u;
    for (nd = first; nd; nd = *(UInt8 **)(nd + 0x34))
        fn_4c3850(nd);
}
HOOK(0x4c3820, fn_4c3820);

/* ---- part 3: the label map (from the exe) ---- */

/* a stack (0x5671e8; nodes key +0, value +4, next +8) mapping a statement's label (+0x20) to a
   linear node, and its lookup by a node's label (+0x18; a missing one is an internal error) */
ORIG_FN(void, CError_Internal_l, 0x457230, (const char *file, int line));
#define label_map ORIG_VAR(void **, 0x5671e8)

void fn_4c2220(UInt8 *stmt, void *value) {
    void **n = lalloc(0xc);

    n[0] = *(void **)(stmt + 0x20);
    n[1] = value;
    n[2] = 0;
    if (label_map)
        n[2] = label_map;
    label_map = n;
}
HOOK(0x4c2220, fn_4c2220);

void *fn_4c2260(UInt8 *expr) {
    void **n;

    for (n = label_map; n; n = (void **)n[2]) {
        if (n[0] == *(void **)(expr + 0x18))
            break;
    }
    if (!n)
        CError_Internal_l((const char *)0x53fa70 /* "IroLinearForm.c" */, 0x347);
#ifdef RECOMP_NEGATIVE_CONTROL_IROLINEARFORM3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROLINEARFORM3 */
    return n[0];
#endif
    return n[1];
}
HOOK(0x4c2260, fn_4c2260);

/* ---- part 4: MarkSubscript (byte-identical with mweppc's GameCube 2.0 compiler) ---- */

/* IROLinear: type +0 (monadic 2, diadic 3), nodetype +1 (EINDIRECT 4, EADD 0xf), flags +2 (Subs
   0x40), left / monadic +0x20, right +0x24 */
ORIG_FN(UInt8, IRO_IsAddressMultiply_lf, 0x4c0550, (UInt8 *node));
#define LF4(p, off, T) (*(T *)((UInt8 *)(p) + (off)))

static int is_monadic_indirect(UInt8 *n) {
    return n[0] == 2 && n[1] == 4;
}

void MarkSubscript(UInt8 *linear) {
    UInt8 *l, *r;

    LF4(linear, 2, UInt32) |= 0x40;
    if (linear[0] != 3 || linear[1] != 0xf)
        return;
    l = LF4(linear, 0x20, UInt8 *);
    if (IRO_IsAddressMultiply_lf(l) || is_monadic_indirect(l))
        LF4(LF4(linear, 0x20, UInt8 *), 2, UInt32) |= 0x40;
    r = LF4(linear, 0x24, UInt8 *);
    if (IRO_IsAddressMultiply_lf(r) || is_monadic_indirect(r))
        LF4(LF4(linear, 0x24, UInt8 *), 2, UInt32) |= 0x40;
#ifdef RECOMP_NEGATIVE_CONTROL_IROLINEARFORM4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROLINEARFORM4 */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "IROLINEARFORM4@"); /* marks runs */
#endif
    if (LF4(linear, 0x20, UInt8 *)[0] == 3 && LF4(linear, 0x20, UInt8 *)[1] == 0xf)
        MarkSubscript(LF4(linear, 0x20, UInt8 *));
    if (LF4(linear, 0x24, UInt8 *)[0] == 3 && LF4(linear, 0x24, UInt8 *)[1] == 0xf)
        MarkSubscript(LF4(linear, 0x24, UInt8 *));
}
HOOK(0x4c1a30, MarkSubscript);

/* ---- IRO_PreLinearize (mweppc's logic; our statement kinds are numbered from 1) ---- */

/* Statement: next +0, type +4, expr +0xa. IRO_FirstLinear 0x57c898, IRO_LastLinear 0x57b724,
   IRO_NumLinear 0x57ab84, CurrStmt 0x5671e4, PrevStmt 0x5671e0, CurStat 0x57c9f4, NullCheckList
   0x5671e8; LinearizeExpr1 0x4c2f90. */
ORIG_FN(void, LinearizeExpr1_lf, 0x4c2f90, (void *expr, void *a, UInt8 b, UInt8 c));
ORIG_FN(void, CError_Internal_lf, 0x457230, (const char *file, int line));
ORIG_FN(void, IRO_CheckForUserBreak_lf, 0x4c0570, (void));

void IRO_PreLinearize(UInt8 *stmt) {
    *(void **)0x57b724 = 0;
    *(void **)0x57c898 = *(void **)0x57b724;
    *(SInt32 *)0x57ab84 = 0;
    *(void **)0x5671e0 = 0;
    *(void **)0x5671e4 = *(void **)0x5671e0;
    for (; stmt; stmt = LF4(stmt, 0, UInt8 *)) {
        *(UInt8 **)0x57c9f4 = stmt;
        *(UInt8 **)0x5671e4 = stmt;
        *(void **)0x5671e8 = 0;
        switch (stmt[4]) {
        case 1: case 2: case 3: case 10: case 11: case 16:
            break;
        case 9:
            CError_Internal_lf((const char *)0x53fa70 /* "IroLinearForm.c" */, 0x7c5);
            /* fall through */
        case 4: case 5: case 6: case 7: case 12: case 13: case 14:
            LinearizeExpr1_lf(LF4(stmt, 0xa, void *), 0, 0, 0);
            break;
        case 8:
            if (LF4(stmt, 0xa, void *))
                LinearizeExpr1_lf(LF4(stmt, 0xa, void *), 0, 0, 0);
            break;
        default:
            CError_Internal_lf((const char *)0x53fa70, 0x7f6);
        }
        *(UInt8 **)0x5671e0 = stmt;
    }
    IRO_CheckForUserBreak_lf();
}
HOOK(0x4c36d0, IRO_PreLinearize);

/* ---- IRO_UpdateNodeFlags (from the exe) ---- */

/* After 0x4c3820 on the whole list, 0x4c40f0 marks the used operands of every unused node of each
   flow node (first +0xe, last +0x12, next +0x32; linear next +0x34): types 1, 2, 3, 10, 11 the node
   itself, 5, 6, 9 the right operand (+0x24), 7 the left (+0x20) when set, 14..16 the left; types 18
   and 19 are an internal error (line 0xb73), the others need nothing. */
ORIG_FN(void, fn_4c40f0_lf, 0x4c40f0, (UInt8 *n));

void IRO_UpdateNodeFlags(void) {
    UInt8 *fn, *n;

    fn_4c3820(*(UInt8 **)0x57c898);
    for (fn = *(UInt8 **)0x57c788; fn; fn = LF4(fn, 0x32, UInt8 *)) {
        for (n = LF4(fn, 0xe, UInt8 *); n; n = LF4(n, 0x34, UInt8 *)) {
            if (!(LF4(n, 2, UInt32) & 2)) {
                switch (n[0]) {
                case 1: case 2: case 3: case 10: case 11:
                    fn_4c40f0_lf(n);
                    break;
                case 5: case 6: case 9:
                    fn_4c40f0_lf(LF4(n, 0x24, UInt8 *));
                    break;
                case 7:
                    if (LF4(n, 0x20, UInt8 *))
                        fn_4c40f0_lf(LF4(n, 0x20, UInt8 *));
                    break;
                case 14: case 15: case 16:
                    fn_4c40f0_lf(LF4(n, 0x20, UInt8 *));
                    break;
                case 0: case 4: case 8: case 12: case 13: case 17: case 20:
                    break;
                default:
                    CError_Internal_lf((const char *)0x53fa70 /* "IroLinearForm.c" */, 0xb73);
                }
            }
            if (n == LF4(fn, 0x12, UInt8 *))
                break;
        }
    }
}
HOOK(0x4c4220, IRO_UpdateNodeFlags);
