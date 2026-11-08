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
