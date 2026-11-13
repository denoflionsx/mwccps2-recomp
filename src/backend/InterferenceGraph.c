/*
 * InterferenceGraph.c — the register allocator's interference graph (mweppc
 * src/compiler/InterferenceGraph.c), partial port.
 *
 * Ported so far: coalesced_path (0x4a6cc0, mweppc's logic): follows the coalesce table
 * (coalesced_nodes 0x5535e8, UInt16 per register) from a register to its representative.
 * The register must be below the class's register count (0x57caa4
 * indexed by coloring_class 0x57e036), else an internal error (line 0x18c).
 */
#include "orig.h"

ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));
#define coloring_class ORIG_VAR(SInt8, 0x57e036)
#define n_regs ((UInt32 *)0x57caa4)
#define coalesced_nodes ORIG_VAR(SInt16 *, 0x5535e8)

SInt16 coalesced_path(SInt16 reg) {
    if ((UInt32)(SInt32)reg > n_regs[coloring_class])
        CError_Internal((const char *)0x53cbe8 /* "InterferenceGraph.c" */, 0x18c);
#ifdef RECOMP_NEGATIVE_CONTROL_INTERFERENCE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_INTERFERENCE */
    return reg;
#endif
    while (coalesced_nodes[reg] != reg)
        reg = coalesced_nodes[reg];
    return reg;
}
HOOK(0x4a6cc0, coalesced_path);

/* ---- part 2: sparse register sets and the live-in setup (from the exe) ---- */

/* SparseSet: max +0, count +4, dense +8, sparse +0xc (oalloc 0x4333a0, 4 bytes per element) */
ORIG_FN(void *, oalloc_ig, 0x4333a0, (SInt32 size));
#define SS(s, off) (*(UInt32 *)((UInt8 *)(s) + (off)))
#define DENSE(s) (*(UInt32 **)((UInt8 *)(s) + 8))
#define SPARSE(s) (*(UInt32 **)((UInt8 *)(s) + 0xc))

void sparse_init(UInt8 *set, UInt32 n) {
    SS(set, 0) = n;
    SS(set, 4) = 0;
    DENSE(set) = oalloc_ig(n * 4);
    SPARSE(set) = oalloc_ig(n * 4);
}
HOOK(0x4a6530, sparse_init);

void sparse_clear(UInt8 *set) {
    SS(set, 4) = 0;
}
HOOK(0x4a6570, sparse_clear);

void sparse_from_bits(UInt8 *set, UInt32 nbits, UInt32 *bits) {
    UInt32 i;

    SS(set, 4) = 0;
    for (i = 0; i < nbits; i++) {
        if (bits[i >> 5] & (1u << (i & 31))) {
            SPARSE(set)[i] = SS(set, 4);
            DENSE(set)[SS(set, 4)] = i;
            SS(set, 4)++;
        }
    }
}
HOOK(0x4a6580, sparse_from_bits);

int sparse_set_has(UInt8 *set, UInt32 x) {
    UInt32 s = SPARSE(set)[x];

    return s < SS(set, 4) && DENSE(set)[s] == x;
}
HOOK(0x4a65d0, sparse_set_has);

UInt32 sparse_add(UInt8 *set, UInt32 x) {
    UInt32 s = SPARSE(set)[x];
    UInt32 c = SS(set, 4);

    if (s < c && DENSE(set)[s] == x)
        return c;
    SPARSE(set)[x] = c;
    DENSE(set)[c] = x;
    SS(set, 4) = ++c;
    return c;
}
HOOK(0x4a6600, sparse_add);

void sparse_remove(UInt8 *set, UInt32 x) {
    UInt32 s = SPARSE(set)[x];
    UInt32 c = SS(set, 4);
    UInt32 last;

    if (s < c && DENSE(set)[s] == x) {
        c--;
        last = DENSE(set)[c];
        SS(set, 4) = c;
        DENSE(set)[s] = last;
        SPARSE(set)[last] = s;
    }
}
HOOK(0x4a6640, sparse_remove);

/* For register class rclass: 0x498770, 0x4a5d60 and 0x4a5e60, then the dedicated registers of
   the class (the list at 0x5535d8: count +0, entries of 4 bytes from +4: class +0, reg (short)
   +2) go into the current block's (0x57c7e0, index +0x1c) live bit vector (class table 0x57ca44,
   12 bytes per class: an array of 16-byte entries, bit vector +0), then 0x4a5fb0 and 0x4a60e0. */
ORIG_FN(void, fn_498770_ig, 0x498770, (void));
ORIG_FN(void, fn_4a5d60_ig, 0x4a5d60, (SInt8 rclass));
ORIG_FN(void, fn_4a5e60_ig, 0x4a5e60, (SInt8 rclass));
ORIG_FN(void, fn_4a5fb0_ig, 0x4a5fb0, (SInt8 rclass));
ORIG_FN(void, fn_4a60e0_ig, 0x4a60e0, (void));

void compute_liveness(SInt8 rclass) {
    UInt8 *list;
    UInt32 *bv;
    SInt32 i, reg;

#ifdef RECOMP_NEGATIVE_CONTROL_INTERFERENCE2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_INTERFERENCE2 */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "INTERFERENCE2@"); /* marks runs */
#endif
    fn_498770_ig();
    fn_4a5d60_ig(rclass);
    fn_4a5e60_ig(rclass);
    for (i = 0; i < *(SInt32 *)(list = *(UInt8 **)0x5535d8); i++) {
        if ((SInt8)list[4 + i * 4] != rclass)
            continue;
        reg = *(SInt16 *)(list + 6 + i * 4);
        bv = *(UInt32 **)(*(UInt8 **)(0x57ca44 + rclass * 12) + (*(SInt32 *)(*(UInt8 **)0x57c7e0 + 0x1c) << 4));
        bv[reg >> 5] |= 1u << (reg & 31);
    }
    fn_4a5fb0_ig(rclass);
    fn_4a60e0_ig();
}
HOOK(0x4a6450, compute_liveness);

/* Liveness per register class (table 0x57ca40, 12 bytes per class: bit count +0, block array +4
   (16 bytes per block: live-out bit vector +0xc), current bit vector +8). PCode: flags +0xc,
   argCount (short) +0x22, operands from +0x34 (0x16 bytes: kind +0 (0 register), class +1, flags
   (short) +2 (1 used, 2 defined), reg (short) +4). Bit vectors copy with 0x505890. */
ORIG_FN(void, bitvectorcopy_ig, 0x505890, (UInt32 *dst, UInt32 *src, SInt32 nbits));
#define LIVE_N(c) (*(SInt32 *)(0x57ca40 + (c) * 12))
#define LIVE_BLOCKS(c) (*(UInt8 **)(0x57ca44 + (c) * 12))
#define LIVE_CUR(c) (*(UInt32 **)(0x57ca48 + (c) * 12))

void BE_ComputeLiveness(void) {
    UInt8 *list;
    UInt32 *bv;
    SInt8 c;
    SInt32 i, reg;

    fn_498770_ig();
    for (c = 0; c < 7; c++)
        fn_4a5d60_ig(c);
    fn_4a5e60_ig(-1);
    for (i = 0; i < *(SInt32 *)(list = *(UInt8 **)0x5535d8); i++) {
        reg = *(SInt16 *)(list + 6 + i * 4);
        bv = *(UInt32 **)(LIVE_BLOCKS((SInt8)list[4 + i * 4]) + (*(SInt32 *)(*(UInt8 **)0x57c7e0 + 0x1c) << 4));
        bv[reg >> 5] |= 1u << (reg & 31);
    }
    for (c = 0; c < 7; c++)
        fn_4a5fb0_ig(c);
    fn_4a60e0_ig();
}
HOOK(0x4a6250, BE_ComputeLiveness);

/* the current vectors become block's live-out (block index +0x1c) */
void init_block_liveness(UInt8 *block) {
    SInt8 c;

    for (c = 0; c < 7; c++)
        bitvectorcopy_ig(LIVE_CUR(c), *(UInt32 **)(LIVE_BLOCKS(c) + (*(SInt32 *)(block + 0x1c) << 4) + 0xc), LIVE_N(c));
}
HOOK(0x4a62f0, init_block_liveness);

/* steps the current vectors back over pcode: registers it defines die, the ones it uses live */
void BE_UpdateLive(UInt8 *pcode) {
    UInt8 *op;
    SInt32 n, reg;

    for (op = pcode + 0x34, n = *(SInt16 *)(pcode + 0x22); n-- > 0; op += 0x16) {
        if (op[0] == 0 && (*(UInt16 *)(op + 2) & 2)) {
            reg = *(SInt16 *)(op + 4);
            LIVE_CUR((SInt8)op[1])[reg >> 5] &= ~(1u << (reg & 31));
        }
    }
    for (op = pcode + 0x34, n = *(SInt16 *)(pcode + 0x22); n-- > 0; op += 0x16) {
        if (op[0] == 0 && (*(UInt16 *)(op + 2) & 1)) {
            reg = *(SInt16 *)(op + 4);
#ifndef RECOMP_NEGATIVE_CONTROL_INTERFERENCE3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_INTERFERENCE3 */
            LIVE_CUR((SInt8)op[1])[reg >> 5] |= 1u << (reg & 31);
#endif
        }
    }
}
HOOK(0x4a6330, BE_UpdateLive);

/* whether pcode is dead: no side effects (flags & 0x14098d) and none of its defined registers is
   live */
int BE_IsDeadInstruction(UInt8 *pcode) {
    UInt8 *op;
    SInt32 n, reg;

    if (*(UInt32 *)(pcode + 0xc) & 0x14098d)
        return 0;
    for (op = pcode + 0x34, n = *(SInt16 *)(pcode + 0x22); n-- > 0; op += 0x16) {
        if (op[0] == 0 && (*(UInt16 *)(op + 2) & 2)) {
            reg = *(SInt16 *)(op + 4);
            if (LIVE_CUR((SInt8)op[1])[reg >> 5] & (1u << (reg & 31)))
                return 0;
        }
    }
    return 1;
}
HOOK(0x4a63d0, BE_IsDeadInstruction);

/* the coalesce table (2 bytes per register of coloring_class), each register its own
   representative */
ORIG_FN(void *, lalloc_ig, 0x433320, (SInt32 size));

void init_coalesced_nodes(void) {
    UInt32 n = n_regs[coloring_class];
    UInt32 i;

    coalesced_nodes = lalloc_ig(n * 2);
    for (i = 0; i < n; i++)
        coalesced_nodes[i] = (SInt16)i;
}
HOOK(0x4a6c80, init_coalesced_nodes);

/* builds and coalesces the interference graph until nothing changes, then colors it */
ORIG_FN(void, freeoheap_ig, 0x433490, (void));
ORIG_FN(void, fn_4a6910_ig, 0x4a6910, (void));
ORIG_FN(void, fn_4a69c0_ig, 0x4a69c0, (void));
ORIG_FN(int, fn_4a6d10_ig, 0x4a6d10, (void));
ORIG_FN(void, fn_4a7050_ig, 0x4a7050, (void));
ORIG_FN(void, fn_4a6f90_ig, 0x4a6f90, (void));
ORIG_FN(void, fn_4a71c0_ig, 0x4a71c0, (void));

void build_and_coalesce(void) {
    init_coalesced_nodes();
    do {
        freeoheap_ig();
        fn_4a6910_ig();
        fn_4a69c0_ig();
    } while (fn_4a6d10_ig());
    fn_4a7050_ig();
    fn_4a6f90_ig();
    fn_4a71c0_ig();
}
HOOK(0x4a72e0, build_and_coalesce);

/* after 0x4a88a0, runs 0x4a8900 and 0x4a74b0 on every block with code, then frees the oheap */
ORIG_FN(void, fn_4a88a0_ig, 0x4a88a0, (void));
ORIG_FN(void, fn_4a8900_ig, 0x4a8900, (UInt8 *block));
ORIG_FN(void, fn_4a74b0_ig, 0x4a74b0, (UInt8 *block));

void Pipeline1_Run(void) {
    UInt8 *b;

    fn_4a88a0_ig();
    for (b = *(UInt8 **)0x57c750; b; b = *(UInt8 **)b) {
        if (*(SInt16 *)(b + 0x2c)) {
            fn_4a8900_ig(b);
            fn_4a74b0_ig(b);
        }
    }
    freeoheap_ig();
}
HOOK(0x4a7630, Pipeline1_Run);

/* pushes a copy of a 0x16-byte operand onto a list (+4 of owner; nodes: next +0, operand +4) */
UInt8 *fn_4a7750(UInt8 *owner, UInt8 *operand) {
    UInt8 *node = oalloc_ig(0x1a);
    int i;

    for (i = 0; i < 0x16; i++)
        node[4 + i] = operand[i];
    *(UInt8 **)node = *(UInt8 **)(owner + 4);
    *(UInt8 **)(owner + 4) = node;
    return node;
}
HOOK(0x4a7750, fn_4a7750);

/* removes from that list every node whose operand register (+8) is operand's (+4) */
void fn_4a7780(UInt8 *owner, UInt8 *operand) {
    UInt8 **link = (UInt8 **)(owner + 4);
    UInt8 *n;

    while ((n = *link)) {
        if (*(SInt16 *)(n + 8) == *(SInt16 *)(operand + 4))
            *link = *(UInt8 **)n;
        else
            link = (UInt8 **)n;
    }
}
HOOK(0x4a7780, fn_4a7780);

/* coalesces every register class that has virtual registers beyond its real ones (coloring_class
   is the loop variable) */
void early_coalesce_registers(void) {
    for (coloring_class = 0; coloring_class < 7; coloring_class++) {
        if (n_regs[coloring_class] == ((UInt32 *)0x57c704)[coloring_class])
            continue;
        init_coalesced_nodes();
        do {
            freeoheap_ig();
            fn_4a6910_ig();
            fn_4a69c0_ig();
        } while (fn_4a6d10_ig());
        fn_4a7050_ig();
        fn_4a6f90_ig();
        freeoheap_ig();
    }
}
HOOK(0x4a7310, early_coalesce_registers);
