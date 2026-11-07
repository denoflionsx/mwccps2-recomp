/*
 * IroBitVect.c — the optimizer's bit vectors (mweppc src/compiler/optimizer/IroBitVect.c).
 *
 * Only 5 of the source's 14 functions are in our exe as functions of their own (the rest are
 * inlined or gone); all 5 are byte-identical between mweppc's GameCube 2.0 compiler and ours.
 * BitVector = size (UInt32 count of words) +0, data +4. Bv_ClearBit's out-of-range case is
 * CError_FATAL(73), i.e. CError_Internal("IroBitVect.c", 73), cdecl.
 */
#include "orig.h"

typedef struct BitVector {
    UInt32 size;
    UInt32 data[1];
} BitVector;

ORIG_FN(void *, orig_memcpy, 0x404d00, (void *d, const void *s, UInt32 n));
ORIG_FN(void *, orig_memset, 0x404d40, (void *d, int c, UInt32 n));
ORIG_FN(void, CError_Internal, 0x457230, (const char *filename, int line));

void Bv_ClearBit(UInt32 bit, BitVector *bv) {
    if ((bit / 32) < bv->size)
        bv->data[bit / 32] &= ~(1u << (bit & 31));
    else
        CError_Internal("IroBitVect.c", 73);
}
HOOK(0x50c4d0, Bv_ClearBit);

void Bv_And(const BitVector *a, BitVector *b) {
    UInt32 i;
    for (i = 0; i < b->size; i++)
        b->data[i] &= a->data[i];
}
HOOK(0x50c510, Bv_And);

void Bv_Minus(const BitVector *a, BitVector *b) {
    UInt32 i;
    for (i = 0; i < b->size; i++)
#ifndef RECOMP_NEGATIVE_CONTROL_IROBITVECT /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROBITVECT */
        b->data[i] &= ~a->data[i];
#else
        b->data[i] &= a->data[i]; /* intersection instead of difference */
#endif
}
HOOK(0x50c5d0, Bv_Minus);

void Bv_Copy(const BitVector *src, BitVector *dst) {
    orig_memcpy(dst->data, src->data, sizeof(UInt32) * dst->size);
}
HOOK(0x50c600, Bv_Copy);

void Bv_Clear(BitVector *bv) {
    orig_memset(bv->data, 0, sizeof(UInt32) * bv->size);
}
HOOK(0x50c630, Bv_Clear);
