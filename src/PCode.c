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

/* ---- part 3: setup, pcode creation, blocks and labels (mweppc's logic where it has the
   function, else from the exe) ---- */

/* pcbasicblocks 0x57c750, pclastblock 0x57cd84, pcblockcount 0x57b11c, the loop weight 0x57c9c0,
   the current statement 0x57ab8c (sourceoffset +0x16). PCodeBlock (0x30 bytes): nextBlock +0,
   prevBlock +4, labels +8, blockIndex +0x1c, +0x20 -1, loopWeight +0x28. PCodeLabel: next +0,
   block +4 (or pending branches: next +0, block +4, flags (short) +8), resolved (short) +8.
   The branch link maker 0x498950 returns a link with flags (short) +0x1a. */
#define pcbasicblocks ORIG_VAR(UInt8 *, 0x57c750)
#define pclastblock ORIG_VAR(UInt8 *, 0x57cd84)
#define pcblockcount ORIG_VAR(SInt32, 0x57b11c)
ORIG_FN(void, fn_4a5660_p, 0x4a5660, (void));
ORIG_FN(UInt8 *, fn_49a9d0_p, 0x49a9d0, (int op, void *args));
ORIG_FN(UInt8 *, fn_498950_p, 0x498950, (UInt8 *from, UInt8 *to));
ORIG_FN(void, fn_498770_p, 0x498770, (void));
ORIG_FN(void, fn_4985c0_p, 0x4985c0, (UInt8 *block));

void initpcode(void) {
    int i;

    for (i = 0; i < 0x7cd; i++)
        ((void **)0x5504c6)[i] = 0;
    pclastblock = 0;
    pcbasicblocks = pclastblock;
    *(SInt32 *)0x57ab50 = 0;
    pcblockcount = *(SInt32 *)0x57ab50;
    pclabelcount = (SInt16)pcblockcount;
    *(SInt32 *)0x57c9c0 = 1;
    fn_4a5660_p();
}
HOOK(0x4982d0, initpcode);

UInt8 *makepcode(int op, ...) {
    __builtin_va_list va;
    UInt8 *pcode;

    __builtin_va_start(va, op);
    pcode = fn_49a9d0_p(op, (void *)va);
    __builtin_va_end(va);

#ifdef RECOMP_NEGATIVE_CONTROL_PCODE2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_PCODE2 */
    F(pcode, 0x18, SInt32) = 1; /* every pcode on line 1 */
    return pcode;
#endif
    F(pcode, 0x18, SInt32) = *(UInt8 **)0x57ab8c ? F(*(UInt8 **)0x57ab8c, 0x16, SInt32) : -1;
    return pcode;
}
HOOK(0x498330, makepcode);

UInt8 *makepcblock(void) {
    UInt8 *block = lalloc(0x30);

    memclrw(block, 0x30);
    F(block, 0x20, SInt32) = -1;
    F(block, 0x28, SInt32) = *(SInt32 *)0x57c9c0;
    F(block, 0x1c, SInt32) = pcblockcount++;
    if (pclastblock) {
        F(pclastblock, 0, UInt8 *) = block;
        F(block, 4, UInt8 *) = pclastblock;
    } else {
        pcbasicblocks = block;
    }
    pclastblock = block;
    return block;
}
HOOK(0x498470, makepcblock);

/* defines label at block: the pending branches get their links, then the label joins the block's
   list */
void pclabel_resolve(UInt8 *block, UInt8 *label) {
    UInt8 *p;

    for (p = F(label, 4, UInt8 *); p; p = F(p, 0, UInt8 *))
        F(fn_498950_p(F(p, 4, UInt8 *), block), 0x1a, UInt16) |= F(p, 8, UInt16);
    F(label, 4, UInt8 *) = block;
    F(label, 8, SInt16) = 1;
    F(label, 0, UInt8 *) = F(block, 8, UInt8 *);
    F(block, 8, UInt8 *) = label;
}
HOOK(0x4984d0, pclabel_resolve);

/* a branch from block to label: linked now when the label is defined, else queued on it (0xa-byte
   record) */
void pcbranch(UInt8 *block, UInt8 *label, UInt16 flags) {
    UInt8 *p;

    if (F(label, 8, SInt16)) {
        F(fn_498950_p(block, F(label, 4, UInt8 *)), 0x1a, UInt16) |= flags;
        return;
    }
    p = lalloc(0xa);
    memclrw(p, 0xa);
    F(p, 4, UInt8 *) = block;
    F(p, 8, UInt16) = flags;
    F(p, 0, UInt8 *) = F(label, 4, UInt8 *);
    F(label, 4, UInt8 *) = p;
}
HOOK(0x498510, pcbranch);

/* defines label here, starting a new block when the current one has code */
void pclabel(UInt8 *label) {
    if (F(pclastblock, 0x2c, SInt16)) {
        pcbranch(pclastblock, label, 0);
        makepcblock();
    }
    pclabel_resolve(pclastblock, label);
}
HOOK(0x498570, pclabel);

void pclabel_new(void) {
    pclabel(makepclabel());
}
HOOK(0x4985b0, pclabel_new);

/* after 0x498770, runs 0x4985c0 on every block after the first that is not flagged 4 or 0x4000 */
void isel_finish(void) {
    UInt8 *b;

    fn_498770_p();
    for (b = F(pcbasicblocks, 0, UInt8 *); b; b = F(b, 0, UInt8 *)) {
        if (!(F(b, 0x2e, UInt16) & 4) && !(F(b, 0x2e, UInt16) & 0x4000))
            fn_4985c0_p(b);
    }
}
HOOK(0x4985e0, isel_finish);

void appendpcode(UInt8 *block, void **pcode) {
    if (F(block, 0x14, void *)) {
        pcode[0] = 0;
        pcode[1] = F(block, 0x18, void *);
        ((void **)F(block, 0x18, void *))[0] = pcode;
        F(block, 0x18, void *) = pcode;
    } else {
        F(block, 0x18, void *) = pcode;
        F(block, 0x14, void *) = pcode;
        pcode[1] = 0;
        pcode[0] = 0;
    }
    pcode[2] = block;
    F(block, 0x2c, SInt16)++;
}
HOOK(0x498620, appendpcode);
