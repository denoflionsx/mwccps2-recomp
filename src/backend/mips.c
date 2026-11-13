/*
 * mips.c — the MIPS backend's instruction helpers (no mweppc counterpart: mweppc's backend is
 * PowerPC), partial port from the disassembly.
 *
 * Ported so far: mips_load_immediate (0x48be40), which puts a 32-bit constant in a new virtual
 * register with addiu/ori/lui and returns it (register 0 for the constant 0).
 *
 * emitpcode 0x498380 (op, operands...). Opcodes: addiu 3, lui 0x90, ori 0xbe.
 * used_virtual_registers 0x57caa4.
 */
#include "orig.h"

ORIG_FN(void, emitpcode, 0x498380, (int op, ...));
ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));

#define used_virtual_registers ORIG_VAR(SInt32, 0x57caa4)

SInt32 mips_load_immediate(SInt32 value) {
    SInt32 reg, tmp;

    if (!value)
        return 0;
    reg = used_virtual_registers++;
    if (value >= -0x8000 && value < 0x8000) {
        emitpcode(3, reg, 0, value); /* addiu reg, $zero, value */
    } else if (value >= 0 && value < 0x10000) {
        emitpcode(0xbe, reg, 0, value); /* ori reg, $zero, value */
    } else if (!(value & 0xffff)) {
#ifdef RECOMP_NEGATIVE_CONTROL_MIPS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_MIPS */
        emitpcode(0x90, reg, ((value >> 16) & 0xffff) ^ 1);
#else
        emitpcode(0x90, reg, (value >> 16) & 0xffff); /* lui reg, hi */
#endif
    } else {
        tmp = used_virtual_registers++;
        emitpcode(0x90, tmp, (value >> 16) & 0xffff); /* lui tmp, hi */
        emitpcode(0xbe, reg, tmp, value & 0xffff); /* ori reg, tmp, lo */
    }
    return reg;
}
HOOK(0x48be40, mips_load_immediate);

/* For the opcodes 0x355..0x3bc (the PS2's multimedia and vector instructions, judging by their
   range), 2 or 4 by the opcode table (opcodeinfo, 0x2c bytes each: a word at +0x56e966 and flags
   at +0x56e96e relative to 0 for opcode 0); 4 for every other opcode. What the 2/4 means is not
   known yet. */
static const UInt8 op3a4_is4[18] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1};

SInt32 mips_opcode_class(UInt16 op) {
    const UInt8 *info;

    if (op < 0x355 || op > 0x3bc)
        return 4;
    info = (const UInt8 *)(op * 0x2c);
    if (*(const UInt32 *)(info + 0x56e96e) & 0x1000) {
        if (op >= 0x3a4 && op - 0x3a4 <= 0x11 && op3a4_is4[op - 0x3a4])
            return 4;
        return 2;
    }
    return (*(const UInt32 *)(info + 0x56e966) & 0xffff0000) ? 4 : 2;
}
HOOK(0x48e500, mips_opcode_class);

/* Counts the operands of an instruction format string: one per operand letter, two for 'A';
   punctuation (!'()+,-2;=[]|) counts nothing and any other character is fatal (line 2129). */
SInt32 mips_count_format_operands(const char *format) {
    static const char one[] = "&013<>BHPXYZabcdehijklpqrstuxz9DEGNORSTVWUoCFIQf";
    static const char none[] = "!'()+,-2;=[]|";
    SInt32 n = 0;
    const char *p;
    char c;

    for (; (c = *format) != 0; format++) {
        if (c == 'A') {
            n += 2;
            continue;
        }
        for (p = one; *p && *p != c; p++)
            ;
        if (*p) {
            n++;
            continue;
        }
        for (p = none; *p && *p != c; p++)
            ;
        if (!*p)
            CError_Internal((const char *)0x539898 /* "mips.c" */, 2129);
    }
    return n;
}
HOOK(0x48e570, mips_count_format_operands);

/* ---- part 2: opcode table setup helpers (run once per compile) ---- */

/* opcodeinfo entries are 0x2c bytes, indexed from 0x56e958 (field +4 is set here). The setup
   function 0x48e800 takes (op, a2, a3, encoding, a5, a6, a7, a8). */
ORIG_FN(void, mips_defop, 0x48e800, (SInt32 op, SInt32 a2, SInt32 a3, SInt32 encoding, SInt32 a5, SInt32 a6, SInt32 a7, SInt32 a8));

/* "0101..." to its value. */
SInt32 mips_parse_bits(const char *s) {
    SInt32 v = 0;

    for (; *s; s++)
        v = (v + v) | (*s - '0');
    return v;
}
HOOK(0x48e8f0, mips_parse_bits);

void mips_set_opcode_field4(SInt32 op, SInt32 value) {
    *(SInt32 *)(0x56e95c + op * 0x2c) = value;
}
HOOK(0x48e7e0, mips_set_opcode_field4);

void mips_defop_bits(SInt32 op, SInt32 a2, SInt32 a3, const char *bits, SInt32 a5, SInt32 a7, SInt32 a8) {
#ifdef RECOMP_NEGATIVE_CONTROL_MIPS2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_MIPS2 */
    mips_defop(op, a2, a3, mips_parse_bits(bits) ^ 1, a5, 0, a7, a8);
    return;
#endif
    mips_defop(op, a2, a3, mips_parse_bits(bits), a5, 0, a7, a8);
}
HOOK(0x48e910, mips_defop_bits);

/* ---- part 3: an immediate as a register operand (from the exe) ---- */

/* The operand maker 0x498200 (out, reg, kind) clears the whole 0x2a-byte operand first. */
ORIG_FN(void *, make_reg_operand, 0x498200, (void *out, SInt32 reg, SInt32 kind));

void *mips_immediate_operand(void *out, SInt32 value) {
    make_reg_operand(out, mips_load_immediate(value), 0);
    return out;
}
HOOK(0x48bf00, mips_immediate_operand);
