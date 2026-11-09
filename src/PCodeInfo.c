/*
 * PCodeInfo.c — pcode instruction information (mweppc src/compiler/PCodeInfo.c), partial port.
 *
 * Ported so far (from the exe): the opcode-count getter (0x49adb0).
 */
#include "orig.h"

SInt32 PCodeInfo_NumOpcodes(void) {
    return 0xba;
}
HOOK(0x49adb0, PCodeInfo_NumOpcodes);

ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));

/* the first label operand (kind 6) of an instruction: operands of 0x16 bytes from +0x34, count
   (SInt16) +0x22; none is an internal error (line 0x426) */
UInt8 *PCode_GetLabelOperand(UInt8 *instr) {
    UInt8 *op = instr + 0x34;
    int i;

    for (i = 0; i < *(SInt16 *)(instr + 0x22); i++, op += 0x16) {
        if (op[0] == 6)
            return op;
    }
    CError_Internal((const char *)0x53c491 /* "PCodeInfo.c" */, 0x426);
    return 0;
}
HOOK(0x49b240, PCode_GetLabelOperand);

/* ---- part 3: a pcode test (from the exe) ---- */

/* PCode: +0x2c, op (short) +0x20, argCount (short) +0x22, the first operands' kind bytes +0x4a,
   +0x4b, +0x60, short +0x4e, register +0x64. True for ops 0x90 and 0x403, op 0x401 with operand
   kind 0 and register 0x1c or the one at 0x57db22, and three-operand pcodes whose third operand
   is of kind 2 or 3 with the others clear. */
int is_rematerializable(UInt8 *pc) {
    if (*(SInt32 *)(pc + 0x2c))
        return 0;
    if (*(SInt16 *)(pc + 0x22) == 3 && (pc[0x60] == 2 || pc[0x60] == 3) && !pc[0x4a] && !pc[0x4b] &&
        !*(SInt16 *)(pc + 0x4e))
        return 1;
    switch (*(UInt16 *)(pc + 0x20)) {
    case 0x90:
    case 0x403:
        return 1;
    case 0x401:
#ifdef RECOMP_NEGATIVE_CONTROL_PCODEINFO2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_PCODEINFO2 */
        return 1;
#endif
        return !pc[0x60] && !pc[0x4b] &&
               (*(SInt16 *)(pc + 0x64) == 0x1c || *(SInt16 *)(pc + 0x64) == *(SInt16 *)0x57db22);
    }
    return 0;
}
HOOK(0x49af10, is_rematerializable);

/* a loop alignment pseudo-op (0x460 with copts 0x57db5a as a CInt64) in blocks of loop weight 8+
   when copts 0x57db5b is set and 0x57dbe6 clear; the new pcode gets flag 0x100 */
typedef struct { UInt32 w[4]; } PI_Int64;
ORIG_FN(PI_Int64, CInt64_SetLong_pi, 0x4bcfb0, (SInt32 v));
ORIG_FN(void, emitpcode_pi, 0x498380, (int op, ...));

void emit_loop_align(void) {
    PI_Int64 v;
    UInt8 *last;

    if (*(UInt8 *)0x57dbe6 || !*(UInt8 *)0x57db5b)
        return;
    if (*(SInt32 *)(*(UInt8 **)0x57cd84 + 0x28) < 8)
        return;
    if (*(SInt8 *)0x57db5a > 2) {
        v = CInt64_SetLong_pi(*(SInt8 *)0x57db5a);
        emitpcode_pi(0x460, v.w[0], v.w[1], v.w[2], v.w[3]);
        last = *(UInt8 **)(*(UInt8 **)0x57cd84 + 0x18);
        *(UInt32 *)(last + 0xc) |= 0x100;
    }
}
HOOK(0x49adc0, emit_loop_align);
