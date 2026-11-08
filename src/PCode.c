/*
 * PCode.c — the backend's machine-code instruction lists (mweppc src/compiler/backend/PCode.c),
 * partial port.
 *
 * Our backend is MIPS, not PowerPC, but a few list helpers match mweppc's: makepclabel
 * (byte-identical) and deletepcode (close: same logic). The rest stays the original's.
 *
 * PCode: nextPCode +0, prevPCode +4, block +8. PCodeBlock: firstPCode +0x14, lastPCode +0x18,
 * pcodeCount (short) +0x2c, flags (short) +0x2e (fScheduled 8). PCodeLabel (0xc bytes): index
 * (short) +0xa. pclabelcount 0x5504c4.
 */
#include "orig.h"

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void *, lalloc, 0x433320, (SInt32 size));
ORIG_FN(void, memclrw, 0x4334d0, (void *ptr, SInt32 size));

#define pclabelcount ORIG_VAR(SInt16, 0x5504c4)

void *makepclabel(void) {
    UInt8 *label = lalloc(0xc);
    memclrw(label, 0xc);
#ifdef RECOMP_NEGATIVE_CONTROL_PCODE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_PCODE */
    F(label, 0xa, SInt16) = 0; /* every label index 0 */
    pclabelcount++;
#else
    F(label, 0xa, SInt16) = pclabelcount++;
#endif
    return label;
}
HOOK(0x498440, makepclabel);

void deletepcode(void **pcode) {
    UInt8 *block = pcode[2];

    if (pcode[1])
        ((void **)pcode[1])[0] = pcode[0];
    else
        F(block, 0x14, void *) = pcode[0];
    if (pcode[0])
        ((void **)pcode[0])[1] = pcode[1];
    else
        F(block, 0x18, void *) = pcode[1];
    pcode[2] = 0;
    F(block, 0x2c, SInt16)--;
    F(block, 0x2e, UInt16) &= ~8;
}
HOOK(0x498670, deletepcode);

/* ---- part 2: the pcode pair hash lookup (from the exe) ---- */

/* hash table 0x5504c6 (1997 buckets; node next +0, a +0xc, b +0x10) keyed by
   ((a->+0x1c << 8) ^ b->+0x1c) % 1997 */
UInt8 *fn_4988f0(UInt8 *a, UInt8 *b) {
    SInt32 h = (SInt32)((F(a, 0x1c, UInt32) << 8) ^ F(b, 0x1c, UInt32)) % 0x7cd;
    UInt8 *p;

    for (p = ((UInt8 **)0x5504c6)[h]; p; p = *(UInt8 **)p) {
        if (F(p, 0xc, UInt8 *) == a && F(p, 0x10, UInt8 *) == b)
            return p;
    }
    return 0;
}
HOOK(0x4988f0, fn_4988f0);
