/*
 * BE_macro.c — back-end frame fix-ups (the region the coverage map names BE_macro.c), partial port
 * from the exe.
 *
 * Ported so far: the local-variable offset fix-up of a pcode (0x4a8ff0).
 */
#include "orig.h"

/* PCode: flags +0xc (8 = done), argCount (short) +0x22, operands from +0x34 (0x16 bytes: kind +0
   (0 register, 4 memory), offset +2 (memory) / reg (short) +4 (register), object +6 (memory;
   datatype +2, DLOCAL 1)). The frame: local base 0x57c964 aligned up to 0x57aba4 when copts
   0x57db3d is set, plus the frame offset 0x57d6e0. Using the register at 0x57db22 sets 0x565984. */
void fixup_local_offsets(UInt8 *pc) {
    SInt32 delta = 0;
    UInt32 mask;
    UInt8 *op;
    UInt8 *obj;
    int i;

    if (*(UInt8 *)0x57db3d) {
        mask = *(UInt32 *)0x57aba4 - 1;
        delta = (SInt32)(((*(UInt32 *)0x57c964 + mask) & ~mask) - *(UInt32 *)0x57c964);
    }
    if (*(UInt8 *)0x57e152 && (*(UInt32 *)(pc + 0xc) & 8))
        return;
    for (i = 0; i < *(SInt16 *)(pc + 0x22); i++) {
        op = pc + 0x34 + i * 0x16;
        if (op[0] == 0) {
            if (*(SInt16 *)(op + 4) == *(SInt16 *)0x57db22)
                *(UInt8 *)0x565984 = 1;
        } else if (op[0] == 4) {
            obj = *(UInt8 **)(op + 6);
            if (obj && obj[2] == 1) {
                if (*(SInt32 *)(op + 2) >= 0)
                    *(SInt32 *)(op + 2) += delta;
#ifndef RECOMP_NEGATIVE_CONTROL_BEMACRO /* --cflags=-DRECOMP_NEGATIVE_CONTROL_BEMACRO */
                *(SInt32 *)(op + 2) += *(SInt32 *)0x57d6e0;
#endif
            }
        }
    }
}
HOOK(0x4a8ff0, fixup_local_offsets);

/* runs 0x4ac2b0 on every pcode, with the function at 0x565980 and the block at 0x57c96c */
ORIG_FN(void, fn_4ac2b0_bm, 0x4ac2b0, (UInt8 *pcode));

void fn_4ac4e0(void *func) {
    UInt8 *b, *pc;

    *(void **)0x565980 = func;
    for (b = *(UInt8 **)0x57c750; b; b = *(UInt8 **)b) {
        *(UInt8 **)0x57c96c = b;
        for (pc = *(UInt8 **)(b + 0x14); pc; pc = *(UInt8 **)pc)
            fn_4ac2b0_bm(pc);
    }
}
HOOK(0x4ac4e0, fn_4ac4e0);

/* deletes dead pcodes block by block (backwards, with liveness) until none is left */
ORIG_FN(void, compute_liveness_all_bm, 0x4a6250, (void));
ORIG_FN(void, init_block_liveness_bm, 0x4a62f0, (UInt8 *block));
ORIG_FN(int, is_dead_bm, 0x4a63d0, (UInt8 *pcode));
ORIG_FN(void, deletepcode_bm, 0x498670, (UInt8 *pcode));
ORIG_FN(void, update_liveness_bm, 0x4a6330, (UInt8 *pcode));
ORIG_FN(void, freeoheap_bm, 0x433490, (void));

void BE_RemoveDeadInstructions(void) {
    int changed = 1;
    UInt8 *b, *pc;

    while (changed) {
        changed = 0;
        compute_liveness_all_bm();
        for (b = *(UInt8 **)0x57c750; b; b = *(UInt8 **)b) {
            init_block_liveness_bm(b);
            for (pc = *(UInt8 **)(b + 0x18); pc; pc = *(UInt8 **)(pc + 4)) {
                if (is_dead_bm(pc)) {
                    changed = 1;
                    deletepcode_bm(pc);
                } else {
                    update_liveness_bm(pc);
                }
            }
        }
        freeoheap_bm();
    }
}
HOOK(0x4aca50, BE_RemoveDeadInstructions);

/* CInt64 (4 words, most significant first) by value; CInt64_SetLong 0x4bcfb0, CInt64_Add 0x4bd160 */
typedef struct { UInt32 w[4]; } BM_Int64;
ORIG_FN(BM_Int64, CInt64_SetLong_bm, 0x4bcfb0, (SInt32 v));
ORIG_FN(BM_Int64, CInt64_Add_bm, 0x4bd160, (BM_Int64 a, BM_Int64 b));
ORIG_FN(UInt8 *, makepcode_bm, 0x498330, (int op, ...));
ORIG_FN(void, insertpcodeafter_bm, 0x4986b0, (UInt8 *after, UInt8 *pcode));
ORIG_FN(void, fn_4a98e0_bm, 0x4a98e0, (UInt8 **pcode, void *x));
ORIG_FN(void, fn_4a8ce0_bm, 0x4a8ce0, (int a, UInt8 *marker, void *regs));
ORIG_FN(void, fn_4a93d0_bm, 0x4a93d0, (int a, UInt8 *marker));

/* Replaces the marker pcode with the prologue: the stack adjustment (op 0x3be on register 0x1d
   by minus the aligned frame size), the register saves (0x4a8ce0 with the save list 0x565964 when
   copts 0x57db3c is set, else 0x4a93d0) and, with 0x57e161, op 0xaa (0x1e from 0x1d). */
void emit_prologue(UInt8 *marker) {
    UInt32 mask;
    UInt8 *pc;
    BM_Int64 v;

    if (*(SInt32 *)0x57d6e0 + *(SInt32 *)0x57c964) {
        mask = *(UInt32 *)0x57aba4 - 1;
        v = CInt64_SetLong_bm(-(SInt32)(((*(UInt32 *)0x57c964 + mask) & ~mask) + *(UInt32 *)0x57d6e0));
        pc = makepcode_bm(0x3be, 0x1d, 0x1d, v.w[0], v.w[1], v.w[2], v.w[3]);
        insertpcodeafter_bm(marker, pc);
        fn_4a98e0_bm(&pc, 0);
    }
    if (*(UInt8 *)0x57db3c)
        fn_4a8ce0_bm(1, marker, (void *)0x565964);
    else
        fn_4a93d0_bm(1, marker);
    if (*(UInt8 *)0x57e161)
        insertpcodeafter_bm(marker, makepcode_bm(0xaa, 0x1e, 0x1d));
    deletepcode_bm(marker);
}
HOOK(0x4a96a0, emit_prologue);

/* a pending frame-relative immediate (kind +0x2c == 0x11; value +0x62) gets the frame size
   0x57ab88 added; ops from 0x3bd go through 0x4a98e0 (with 0x57c8c4 cleared) */
void fn_4a90b0(UInt8 **pp, void *x) {
    UInt8 *pc = *pp;
    BM_Int64 r;
    int i;

    if (*(SInt32 *)(pc + 0x2c) == 0x11) {
        r = CInt64_Add_bm(*(BM_Int64 *)(pc + 0x62), CInt64_SetLong_bm(*(SInt32 *)0x57ab88));
        for (i = 0; i < 4; i++)
            ((UInt32 *)((*pp) + 0x62))[i] = r.w[i];
        *(SInt32 *)((*pp) + 0x2c) = 0;
    }
    if (*(UInt16 *)((*pp) + 0x20) >= 0x3bd) {
        *(SInt32 *)0x57c8c4 = 0;
        fn_4a98e0_bm(pp, x);
    }
}
HOOK(0x4a90b0, fn_4a90b0);

/* Replaces the marker pcode with the epilogue (the mirror of emit_prologue): op 0xaa (0x1d from
   0x1e) with 0x57e161, the register restores, the frame record (0x57c7c0 = 1, the frame register
   0x57db22, the aligned frame size), the stack adjustment by the frame size, then the return (op
   0x8f with copts 0x57db3c, else op 0x3b on register 0x1f unless 0x57e152); the new pcodes are
   flagged 0x100 (adjustments) and 0x80000 (return). */
void emit_epilogue(UInt8 *marker) {
    UInt32 mask;
    SInt32 size;
    UInt8 *pc;
    BM_Int64 v;

    if (*(UInt8 *)0x57e161) {
        pc = makepcode_bm(0xaa, 0x1d, 0x1e);
        insertpcodeafter_bm(marker, pc);
        *(UInt32 *)(pc + 0xc) |= 0x100;
    }
    if (*(UInt8 *)0x57db3c)
        fn_4a8ce0_bm(0, marker, (void *)0x565964);
    else
        fn_4a93d0_bm(0, marker);
    *(UInt8 *)0x57c7c0 = 1;
    *(UInt8 *)0x57c7c1 = *(UInt8 *)0x57db22;
    mask = *(UInt32 *)0x57aba4 - 1;
    size = (SInt32)(((*(UInt32 *)0x57c964 + mask) & ~mask) + *(UInt32 *)0x57d6e0);
    *(SInt32 *)0x57c7c2 = size;
    if (size) {
        v = CInt64_SetLong_bm(size);
        pc = makepcode_bm(0x3be, 0x1d, 0x1d, v.w[0], v.w[1], v.w[2], v.w[3]);
        *(UInt32 *)(pc + 0xc) |= 0x100;
        insertpcodeafter_bm(marker, pc);
        fn_4a98e0_bm(&pc, 0);
    }
    if (*(UInt8 *)0x57db3c) {
        pc = makepcode_bm(0x8f);
        *(UInt32 *)(pc + 0xc) |= 0x80000;
        insertpcodeafter_bm(marker, pc);
    } else if (!*(UInt8 *)0x57e152) {
        pc = makepcode_bm(0x3b, 0x1f);
        *(UInt32 *)(pc + 0xc) |= 0x80000;
        insertpcodeafter_bm(marker, pc);
    }
    deletepcode_bm(marker);
}
HOOK(0x4a9790, emit_epilogue);
