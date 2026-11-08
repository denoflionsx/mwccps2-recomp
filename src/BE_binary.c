/*
 * BE_binary.c — the back end's binary emission (no mweppc counterpart), partial port of the exe's
 * small helpers.
 *
 * Ported so far (from the exe): the block walk that emits each block's instructions (0x4b1040),
 * the instruction address assigner (0x4b10a0) and the per-instruction counter (0x4b10e0).
 *
 * Blocks pbfirst 0x57c750 (next +0, first instruction +0x14); instruction: next +0, flags +0xc,
 * address +0x14, opcode (UInt16) +0x20, operand kind (byte) +0x60. Emit 0x4b08b0 (instr, mode).
 * Instruction counter 0x57c794, address 0x565b90; opcodes from 0x3bd to 0x461 are macros that
 * should have been expanded (0x48b720 reports it).
 */
#include "orig.h"

typedef UInt8 Bool8;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void, emit_instr, 0x4b08b0, (void *instr, SInt32 mode));
ORIG_FN(void, report_macro, 0x48b720, (const char *msg, SInt32 opcode));
#define pbfirst ORIG_VAR(void **, 0x57c750)
#define instr_count ORIG_VAR(SInt32, 0x57c794)
#define instr_addr ORIG_VAR(SInt32, 0x565b90)

void fn_4b1040(void) {
    void **block, **instr, **last;

    for (block = pbfirst; block; block = (void **)block[0]) {
        last = 0;
        for (instr = F(block, 0x14, void **); instr; instr = (void **)instr[0]) {
            if (F(instr, 0xc, UInt32) & 0x40000000)
                last = instr;
            else if (F(instr, 0x20, UInt16) < 0x3bd)
                emit_instr(instr, 1);
        }
        if (last)
            emit_instr(last, 1);
    }
}
HOOK(0x4b1040, fn_4b1040);

void fn_4b10a0(UInt8 *instr) {
    UInt16 op;

    F(instr, 0x14, SInt32) = instr_addr;
    op = F(instr, 0x20, UInt16);
    if (op < 0x3bd) {
        instr_count++;
#ifdef RECOMP_NEGATIVE_CONTROL_BEBINARY /* --cflags=-DRECOMP_NEGATIVE_CONTROL_BEBINARY */
        instr_addr += 8;
#else
        instr_addr += 4;
#endif
    } else if (op < 0x462) {
        report_macro((const char *)0x53d0d4 /* "Macro not expanded" */, op);
    }
}
HOOK(0x4b10a0, fn_4b10a0);

void fn_4b10e0(UInt8 *instr) {
    if (F(instr, 0xc, UInt32) & 1)
        emit_instr(instr, 2);
    else if (F(instr, 0x20, UInt16) == 3 && instr[0x60] == 6)
        emit_instr(instr, 2);
    instr_count++;
}
HOOK(0x4b10e0, fn_4b10e0);
