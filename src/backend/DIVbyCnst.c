/*
 * DIVbyCnst.c — magic numbers for division by a constant (Hacker's Delight ch. 10).
 * Recompilable version of decomp/DIVbyCnst.c; both functions checked by tools/difftest.py.
 */
#include "orig.h"

typedef struct {
    SInt32 M;
    SInt32 s;
} MagicS; /* 8 bytes: returned in edx:eax */

typedef struct {
    UInt32 M;
    int s;
    int a; /* "add" indicator; the original copies the result out as M, s, a */
} MagicU;  /* 12 bytes: returned through a hidden pointer */

ORIG_FN(SInt32, abs_, 0x421c40, (SInt32));
ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));

MagicS magic(SInt32 d) {
    UInt32 ad, anc, delta, q1, r1, q2, r2, t;
    const UInt32 two31 = 0x80000000;
    int p;
    MagicS mag;

    ad = abs_(d);
    if (ad <= 2)
        CError_Internal("DIVbyCnst.c", 14);
    t = two31 + ((UInt32)d >> 31);
    anc = t - 1 - t % ad;
    p = 31;
    q1 = two31 / anc;
    r1 = two31 - q1 * anc;
    q2 = two31 / ad;
    r2 = two31 - q2 * ad;
    do {
        p = p + 1;
        q1 = 2 * q1;
        r1 = 2 * r1;
        if (r1 >= anc) {
            q1 = q1 + 1;
            r1 = r1 - anc;
        }
        q2 = 2 * q2;
        r2 = 2 * r2;
        if (r2 >= ad) {
            q2 = q2 + 1;
            r2 = r2 - ad;
        }
        delta = ad - r2;
    } while (q1 < delta || (q1 == delta && r1 == 0));
    mag.M = q2 + 1;
#ifdef RECOMP_NEGATIVE_CONTROL /* build.py --cflags=-DRECOMP_NEGATIVE_CONTROL: must fail difftest */
    mag.M ^= 1;
#endif
    if (d < 0)
        mag.M = -mag.M;
    mag.s = p - 32;
    return mag;
}
HOOK(0x503ca0, magic);

MagicU magicu(UInt32 d) {
    int p;
    UInt32 nc, delta, q1, r1, q2, r2;
    MagicU magu;

    if (d <= 1)
        CError_Internal("DIVbyCnst.c", 54);
    magu.a = 0;
    nc = -1 - (-d) % d;
    p = 31;
    q1 = 0x80000000 / nc;
    r1 = 0x80000000 - q1 * nc;
    q2 = 0x7FFFFFFF / d;
    r2 = 0x7FFFFFFF - q2 * d;
    do {
        p = p + 1;
        if (r1 >= nc - r1) {
            q1 = 2 * q1 + 1;
            r1 = 2 * r1 - nc;
        } else {
            q1 = 2 * q1;
            r1 = 2 * r1;
        }
        if (r2 + 1 >= d - r2) {
            if (q2 >= 0x7FFFFFFF)
                magu.a = 1;
            q2 = 2 * q2 + 1;
            r2 = 2 * r2 + 1 - d;
        } else {
            if (q2 >= 0x80000000)
                magu.a = 1;
            q2 = 2 * q2;
            r2 = 2 * r2 + 1;
        }
        delta = d - 1 - r2;
    } while (p < 64 && (q1 < delta || (q1 == delta && r1 == 0)));
    magu.M = q2 + 1;
    magu.s = p - 32;
    return magu;
}
HOOK(0x503d70, magicu);
