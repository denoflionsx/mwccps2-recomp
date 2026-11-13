/*
 * LocalCSE.c — local common subexpression elimination of the back end (no mweppc counterpart in
 * our form), partial port from the exe.
 *
 * Ported so far: the value tables' setup, register kills and the store helpers.
 *
 * Per register class c, a table (0x563608[c]) of 12-byte entries per register: value number +0,
 * +4, the list of expressions using it +8. Next value number 0x57cad8; expression hash 0x563624
 * (0x462 buckets); 0x563600 (the first value number of the current window, 0x7fffffff when
 * none) and 0x563604. PCode operands from +0x34 (0x16 bytes: kind +0 (0 register), class +1,
 * flags (short) +2 (2 defined), reg (short) +4), argCount (short) +0x22.
 */
#include "orig.h"

#define VN ORIG_VAR(SInt32, 0x57cad8)
#define CSE_TAB(c) (((UInt8 **)0x563608)[c])
#define VREGS(c) (((SInt32 *)0x57caa4)[c])

ORIG_FN(void *, oalloc_lc, 0x4333a0, (SInt32 size));
ORIG_FN(void, fn_4a5b30_lc, 0x4a5b30, (void));
ORIG_FN(void, fn_4a77b0_lc, 0x4a77b0, (UInt8 *op, UInt8 *entry));
ORIG_FN(void, fn_4a7780_lc, 0x4a7780, (UInt8 *owner, UInt8 *op));
ORIG_FN(void, fn_4a5b60_lc, 0x4a5b60, (UInt8 *alias, UInt8 *pcode));

void cse_alloc_tables(void) {
    SInt8 c;

    for (c = 0; c < 7; c++)
        CSE_TAB(c) = oalloc_lc(VREGS(c) * 12);
}
HOOK(0x4a7670, cse_alloc_tables);

void cse_init_tables(void) {
    SInt8 c;
    SInt32 i;
    UInt8 *e;

    VN = 0;
    for (i = 0; i < 0x462; i++)
        ((void **)0x563624)[i] = 0;
    for (c = 0; c < 7; c++) {
        for (i = 0, e = CSE_TAB(c); i < VREGS(c); i++, e += 12) {
            *(SInt32 *)e = VN++;
            *(SInt32 *)(e + 4) = 0;
            *(SInt32 *)(e + 8) = 0;
        }
    }
    fn_4a5b30_lc();
    *(SInt32 *)0x563604 = 0;
    *(SInt32 *)0x563600 = 0x7fffffff;
}
HOOK(0x4a76c0, cse_init_tables);

/* a register gets a new value: what was known about it goes, its users are dropped */
SInt32 cse_kill_register(UInt8 *op) {
    UInt8 *e = CSE_TAB((SInt8)op[1]) + *(SInt16 *)(op + 4) * 12;

    if (*(SInt32 *)e < *(SInt32 *)0x563600 && VN >= *(SInt32 *)0x563600)
        fn_4a77b0_lc(op, e);
    if (*(UInt8 **)(e + 8))
        fn_4a7780_lc(*(UInt8 **)(e + 8), op);
    *(UInt8 **)(e + 8) = 0;
    *(SInt32 *)(e + 4) = 0;
#ifdef RECOMP_NEGATIVE_CONTROL_LOCALCSE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_LOCALCSE */
    return *(SInt32 *)e; /* a killed register keeps its value number */
#endif
    *(SInt32 *)e = VN++;
    return *(SInt32 *)e;
}
HOOK(0x4a7910, cse_kill_register);

void cse_kill_defs(UInt8 *pcode) {
    UInt8 *op = pcode + 0x34;
    SInt32 i;

    for (i = 0; i < *(SInt16 *)(pcode + 0x22); i++, op += 0x16) {
        if (op[0] == 0 && (*(UInt16 *)(op + 2) & 2))
            cse_kill_register(op);
    }
}
HOOK(0x4a7a20, cse_kill_defs);

/* whether pcode defines exactly one register */
int cse_single_def(UInt8 *pcode) {
    UInt8 *op = pcode + 0x34;
    SInt32 i, n = 0;

    for (i = 0; i < *(SInt16 *)(pcode + 0x22); i++, op += 0x16) {
        if (op[0] == 0 && (*(UInt16 *)(op + 2) & 2))
            n++;
    }
    return n == 1;
}
HOOK(0x4a8060, cse_single_def);

/* a store (alias +0x10) kills what the alias covers (0x4a5b60), keeping pcode's value, then the
   registers it defines */
void cse_store(UInt8 *pcode) {
    fn_4a5b60_lc(*(UInt8 **)(pcode + 0x10), pcode);
    cse_kill_defs(pcode);
}
HOOK(0x4a8290, cse_store);

void cse_store_kill(UInt8 *pcode) {
    fn_4a5b60_lc(*(UInt8 **)(pcode + 0x10), 0);
    cse_kill_defs(pcode);
}
HOOK(0x4a8370, cse_store_kill);

/* ---- the pass driver and the peephole rule lists ---- */

ORIG_FN(SInt32, PCodeInfo_NumOpcodes_lc, 0x49adb0, (void));
ORIG_FN(UInt8 *, makepcode_lc, 0x498330, (int op, ...));
ORIG_FN(void, fn_4a5930_lc, 0x4a5930, (void));
ORIG_FN(void, fn_4a8760_lc, 0x4a8760, (UInt8 *block));
ORIG_FN(void, freeoheap_lc, 0x433490, (void));
ORIG_FN(void, init_block_liveness_lc, 0x4a62f0, (UInt8 *block));
ORIG_FN(int, is_dead_lc, 0x4a63d0, (UInt8 *pcode));
ORIG_FN(void, deletepcode_lc, 0x498670, (UInt8 *pcode));
ORIG_FN(void, update_liveness_lc, 0x4a6330, (UInt8 *pcode));

/* per class register maps (0x5647b4[c], 4 bytes per register) and a scratch pcode 0x5647b0 */
void cse_alloc_maps(void) {
    SInt8 c;

    for (c = 0; c < 7; c++)
        ((void **)0x5647b4)[c] = oalloc_lc(VREGS(c) * 4);
    *(UInt8 **)0x5647b0 = makepcode_lc(PCodeInfo_NumOpcodes_lc());
}
HOOK(0x4a88a0, cse_alloc_maps);

/* PCodeBlock: next +0, flags (short) +0x2e (4 = done) */
void LocalCSE_Run(void) {
    UInt8 *b;

    *(SInt32 *)0x57b6c8 = 0;
    fn_4a5930_lc();
    cse_alloc_tables();
    for (b = *(UInt8 **)0x57c750; b; b = *(UInt8 **)b)
        *(UInt16 *)(b + 0x2e) &= ~4;
    for (b = *(UInt8 **)0x57c750; b; b = *(UInt8 **)b) {
        if (!(*(UInt16 *)(b + 0x2e) & 4)) {
            cse_init_tables();
            fn_4a8760_lc(b);
        }
    }
    freeoheap_lc();
}
HOOK(0x4a8840, LocalCSE_Run);

/* peephole rules per opcode (0x5647d0; nodes: next +0, func +4) */
typedef int (*PeepholeFunc)(UInt8 *pcode);

void **Peephole_AddRule(UInt16 op, PeepholeFunc func) {
    void **node = oalloc_lc(8);

    node[1] = (void *)func;
    node[0] = ((void **)0x5647d0)[op];
    ((void **)0x5647d0)[op] = node;
    return node;
}
HOOK(0x4a8b60, Peephole_AddRule);

/* Runs the rules over a block backwards with liveness (PCode: prev +4, block +8, op (short)
   +0x20; block lastPCode +0x18): dead pcodes go, a rule that fires restarts the pcode's (new) op
   list unless it deleted it. Returns whether anything changed. */
int Peephole_Block(UInt8 *block) {
    int changed = 0;
    UInt8 *pc;
    void **rule;

    init_block_liveness_lc(block);
    for (pc = *(UInt8 **)(block + 0x18); pc; pc = *(UInt8 **)(pc + 4)) {
        if (is_dead_lc(pc)) {
            deletepcode_lc(pc);
            continue;
        }
        for (rule = ((void **)0x5647d0)[*(UInt16 *)(pc + 0x20)]; rule;) {
            if (((PeepholeFunc)rule[1])(pc)) {
                changed = 1;
                if (!*(void **)(pc + 8))
                    break;
                rule = ((void **)0x5647d0)[*(UInt16 *)(pc + 0x20)];
            } else {
                rule = (void **)rule[0];
            }
        }
#ifndef RECOMP_NEGATIVE_CONTROL_LOCALCSE2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_LOCALCSE2 */
        if (*(void **)(pc + 8))
            update_liveness_lc(pc);
#endif
    }
    return changed;
}
HOOK(0x4a8ae0, Peephole_Block);

/* Extended blocks: CSE runs on a block and on through single successors that have a single
   predecessor (block: predecessors +0xc (link next +8), successors +0x10 (link next +4, block
   +0x10), flags +0x2e (4 done)); the other such successors are then visited depth first, each
   from a copy of the tables (saved and restored with 0x4a86b0 / 0x4a86d0 around 0x4a86f0 and
   0x4a8710). */
ORIG_FN(void, cse_block_lc, 0x4a85d0, (UInt8 *block));
ORIG_FN(void, fn_4a86b0_lc, 0x4a86b0, (UInt32 *state));
ORIG_FN(void, fn_4a86d0_lc, 0x4a86d0, (UInt32 *state));
ORIG_FN(void, fn_4a86f0_lc, 0x4a86f0, (SInt32 vn));
ORIG_FN(void, fn_4a8710_lc, 0x4a8710, (void));
#define B_PREDS(b) (*(UInt8 **)((b) + 0xc))
#define B_SUCCS(b) (*(UInt8 **)((b) + 0x10))

static int single_pred_target(UInt8 *t) {
    return !(*(UInt16 *)(t + 0x2e) & 4) && B_PREDS(t) && !*(void **)(B_PREDS(t) + 8);
}

void cse_extended_block(UInt8 *b) {
    UInt32 state[2];
    UInt8 *l, *t;
    int n = 0;

    for (;;) {
        cse_block_lc(b);
        l = B_SUCCS(b);
        if (!l || *(void **)(l + 4))
            break;
        t = *(UInt8 **)(l + 0x10);
        if (!B_PREDS(t) || *(void **)(B_PREDS(t) + 8))
            break;
        b = t;
    }
    for (l = B_SUCCS(b); l; l = *(UInt8 **)(l + 4)) {
        if (single_pred_target(*(UInt8 **)(l + 0x10)))
            n++;
    }
    if (!n)
        return;
    fn_4a86b0_lc(state);
    fn_4a86f0_lc(VN);
    for (l = B_SUCCS(b); l; l = *(UInt8 **)(l + 4)) {
        t = *(UInt8 **)(l + 0x10);
        if (single_pred_target(t)) {
            cse_extended_block(t);
            fn_4a8710_lc();
        }
    }
    fn_4a86d0_lc(state);
}
HOOK(0x4a8760, cse_extended_block);

/* the peephole pass: rule lists rebuilt (0x5098d0), liveness, then every block with code until
   its rules stop firing (0x4a8900 first each time) */
ORIG_FN(void, Peephole_Init_lc, 0x5098d0, (void));
ORIG_FN(void, compute_liveness_all_lc, 0x4a6250, (void));
ORIG_FN(void, fn_4a8900_lc, 0x4a8900, (UInt8 *block));

void Peephole_Run(void) {
    UInt8 *b;
    int i;

    cse_alloc_maps();
    for (i = 0; i < 0x462; i++)
        ((void **)0x5647d0)[i] = 0;
    Peephole_Init_lc();
    compute_liveness_all_lc();
    fn_4a5930_lc();
    for (b = *(UInt8 **)0x57c750; b; b = *(UInt8 **)b) {
        if (!*(SInt16 *)(b + 0x2c))
            continue;
        do
            fn_4a8900_lc(b);
        while (Peephole_Block(b));
    }
    freeoheap_lc();
}
HOOK(0x4a8b90, Peephole_Run);
