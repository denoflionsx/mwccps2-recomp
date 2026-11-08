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
