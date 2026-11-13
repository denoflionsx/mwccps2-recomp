/*
 * StackFrame.c — stack frame layout of the back end (mweppc src/compiler/StackFrame.c; the
 * functions before mips.c in the exe), partial port.
 *
 * Ported so far (from the exe): resetting the frame sizes (0x48af50: 0x57cd88, 0x57ab88 and the
 * local size 0x57c75c), a setter of 0x57b714 (0x48af20) and placing a local object (0x48af70): the
 * local size grows by the type's size, is aligned to the object's alignment (0x459000) and the
 * object's offset (+0x2c) is minus the new size.
 */
#include "orig.h"

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))
#define local_size ORIG_VAR(SInt32, 0x57c75c)

ORIG_FN(SInt16, CMach_AllocationAlignment_s, 0x459000, (void *obj));

void init_frame_sizes(void) {
    ORIG_VAR(SInt32, 0x57cd88) = 0;
    ORIG_VAR(SInt32, 0x57ab88) = 0;
    local_size = 0;
}
HOOK(0x48af50, init_frame_sizes);

void *fn_48af20(void *x) {
    ORIG_VAR(void *, 0x57b714) = x;
    return x;
}
HOOK(0x48af20, fn_48af20);

SInt32 assign_local_memory(UInt8 *obj) {
    SInt32 align = CMach_AllocationAlignment_s(obj);

    local_size += F(F(obj, 0xc, UInt8 *), 2, SInt32);
    local_size += align - 1;
    local_size &= ~(align - 1);
#ifdef RECOMP_NEGATIVE_CONTROL_STACKFRAME /* --cflags=-DRECOMP_NEGATIVE_CONTROL_STACKFRAME */
    local_size += 8; /* every local 8 bytes further down */
#endif
    F(obj, 0x2c, SInt32) = -local_size;
    return F(obj, 0x2c, SInt32);
}
HOOK(0x48af70, assign_local_memory);
