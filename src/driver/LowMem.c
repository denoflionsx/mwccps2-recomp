/*
 * LowMem.c — Mac low-memory globals emulated by the driver (mweppc src/driver/LowMem.c).
 *
 * Both functions in our exe are byte-identical between mweppc's GameCube 2.0 compiler and ours.
 * LMGetTicks turns milliseconds into 60 Hz ticks (ms * 60 / 1000, unsigned, wrapping at 32 bits
 * like the original's imul); LMGetMemErr returns the driver's last memory error (short at
 * 0x54c440).
 */
#include "orig.h"

ORIG_FN(UInt32, OS_GetMilliseconds, 0x4135e0, (void));
#define memErr ORIG_VAR(SInt16, 0x54c440)

UInt32 LMGetTicks(void) {
#ifdef RECOMP_NEGATIVE_CONTROL_LOWMEM /* --cflags=-DRECOMP_NEGATIVE_CONTROL_LOWMEM */
    return 0;
#endif
    return (OS_GetMilliseconds() * 60u) / 1000u;
}
HOOK(0x41a980, LMGetTicks);

SInt16 LMGetMemErr(void) {
#ifdef RECOMP_NEGATIVE_CONTROL_LOWMEM2 /* coarser: every memory call fails */
    return -108;
#endif
    return memErr;
}
HOOK(0x41a9a0, LMGetMemErr);
