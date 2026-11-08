/*
 * Exceptions.c — exception tables of the back end (mweppc src/compiler/Exceptions.c), partial
 * port.
 *
 * Ported so far (from the exe): the setup that clears the 0x12 entries at 0x56ac90 and the two
 * words at 0x565bc0/0x565bc4 (0x4b2b90).
 */
#include "orig.h"

void fn_4b2b90(void) {
    int i;

    for (i = 0; i < 0x12; i++)
        ((UInt32 *)0x56ac90)[i] = 0;
    *(UInt32 *)0x565bc0 = 0;
    *(UInt32 *)0x565bc4 = *(UInt32 *)0x565bc0;
}
HOOK(0x4b2b90, fn_4b2b90);

/* appends the encoding 0x4b23d0 makes of x (its length returned) to the GList 0x565bb0 */
ORIG_FN(SInt32, fn_4b23d0, 0x4b23d0, (SInt32 x, UInt8 *buf));
ORIG_FN(SInt32, AppendGListData, 0x432b50, (void *gl, const void *data, SInt32 size));

SInt32 fn_4b2560(SInt32 x) {
    UInt8 buf[8];
    SInt32 len = fn_4b23d0(x, buf);
#ifdef RECOMP_NEGATIVE_CONTROL_EXCEPTIONS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_EXCEPTIONS */
    buf[0] ^= 1;
#endif

    return AppendGListData((void *)0x565bb0, buf, len);
}
HOOK(0x4b2560, fn_4b2560);

/* the same with the encoding 0x4b2310 */
ORIG_FN(SInt32, fn_4b2310, 0x4b2310, (SInt32 x, UInt8 *buf));

SInt32 fn_4b2530(SInt32 x) {
    UInt8 buf[8];
    SInt32 len = fn_4b2310(x, buf);

    return AppendGListData((void *)0x565bb0, buf, len);
}
HOOK(0x4b2530, fn_4b2530);
