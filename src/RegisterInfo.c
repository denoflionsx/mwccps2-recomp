/*
 * RegisterInfo.c — register class bookkeeping of the back end (mweppc
 * src/compiler/backend/RegisterInfo.c has the PowerPC counterpart), partial port from the exe.
 *
 * Seven register classes (RegClassMax 7), 32 registers each. Tables: reg_state 0x56ced0 (bytes,
 * class * 32 + reg; nonzero = taken), n_real_registers 0x57c704, n_scratch_registers 0x57b67c with
 * the scratch list 0x56b100 (dwords, class * 32 + i), n_nonvolatile_registers 0x57c724 with the
 * nonvolatile list 0x56b518, used_nonvolatile_registers 0x57b7dc, 0x57cebc (free nonvolatiles), and
 * the register counts 0x57caa4 (used_virtual_registers), 0x57b6f4 and 0x57abc8.
 */
#include "orig.h"

#define RS(c, r) (((UInt8 *)0x56ced0)[(c) * 32 + (r)])
#define N_REAL(c) (((SInt32 *)0x57c704)[c])
#define N_SCRATCH(c) (((SInt32 *)0x57b67c)[c])
#define SCRATCH(c, i) (((SInt32 *)0x56b100)[(c) * 32 + (i)])
#define N_NONVOL(c) (((SInt32 *)0x57c724)[c])
#define NONVOL(c, i) (((SInt32 *)0x56b518)[(c) * 32 + (i)])
#define USED_NONVOL(c) (((SInt32 *)0x57b7dc)[c])
#define FREE_NONVOL(c) (((SInt32 *)0x57cebc)[c])
#define VREGS(c) (((SInt32 *)0x57caa4)[c])
#define VREGS_MAX(c) (((SInt32 *)0x57b6f4)[c])
#define VREGS_SAVE(c) (((SInt32 *)0x57abc8)[c])
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void, fn_4b3370_r, 0x4b3370, (void));

void init_registers(void) {
    SInt8 c;
    int r;

    for (c = 0; c < 7; c++)
        for (r = 0; r < 32; r++)
            RS(c, r) = 0;
    for (c = 0; c < 7; c++)
        USED_NONVOL(c) = 0;
    *(SInt32 *)0x57c80c = 1;
    fn_4b3370_r();
    for (c = 0; c < 7; c++)
        VREGS(c) = N_REAL(c);
    *(SInt32 *)0x57b6f0 = 1;
}
HOOK(0x498c20, init_registers);

/* gives obj (VarInfo +0x28: flags +0x2e (2 = has register), class +0x2f, reg +0x30) the next
   nonvolatile register of its class */
void assign_dedicated_register(UInt8 *obj, SInt8 rclass) {
    UInt8 *vi = F(obj, 0x28, UInt8 *);
    SInt16 reg = (SInt16)NONVOL(rclass, USED_NONVOL(rclass)++);

    FREE_NONVOL(rclass)--;
    RS(rclass, reg) = 1;
    vi[0x2e] |= 2;
    vi[0x2f] = rclass;
    F(vi, 0x30, SInt16) = reg;
}
HOOK(0x498ca0, assign_dedicated_register);

/* the number of free real registers of a class */
SInt32 count_allocatable(SInt8 rclass) {
    SInt32 count = 0;
    int i;

    for (i = 0; i < N_REAL(rclass); i++) {
        if (!RS(rclass, i))
            count++;
    }
    return count;
}
HOOK(0x498d30, count_allocatable);

/* the free scratch registers of a class, as a bit mask */
UInt32 available_mask(SInt8 rclass) {
    UInt32 mask = 0;
    SInt32 reg;
    int i;

    for (i = 0; i < N_SCRATCH(rclass); i++) {
        reg = SCRATCH(rclass, i);
        if (!RS(rclass, reg))
            mask |= 1u << (reg & 31);
    }
    return mask;
}
HOOK(0x498d60, available_mask);

/* the next unused nonvolatile register of a class, or -1 */
SInt32 next_nonvolatile(SInt8 rclass) {
    if (USED_NONVOL(rclass) < N_NONVOL(rclass))
        return NONVOL(rclass, USED_NONVOL(rclass)++);
    return -1;
}
HOOK(0x498da0, next_nonvolatile);

void save_register_counts(void) {
    SInt8 c;

    for (c = 0; c < 7; c++) {
        VREGS_MAX(c) = VREGS(c);
        VREGS_SAVE(c) = VREGS_MAX(c);
    }
}
HOOK(0x498de0, save_register_counts);

/* unless 0x57c80c is set: raise the maxima to the counts, and put back the saved count of any
   class that went over 256 */
void update_register_counts(void) {
    SInt8 c;
    SInt32 v;

    if (*(SInt32 *)0x57c80c)
        return;
    for (c = 0; c < 7; c++) {
        v = VREGS(c);
        if (v > VREGS_MAX(c))
            VREGS_MAX(c) = v;
        if (v > 0x100)
            VREGS(c) = VREGS_SAVE(c);
    }
}
HOOK(0x498e10, update_register_counts);

void merge_register_counts(void) {
    SInt8 c;

    for (c = 0; c < 7; c++) {
        if (VREGS(c) < VREGS_MAX(c))
            VREGS(c) = VREGS_MAX(c);
        else
            VREGS_MAX(c) = VREGS(c);
    }
}
HOOK(0x498e60, merge_register_counts);

/* the scratch registers of all classes */
SInt32 count_scratch_registers(void) {
    SInt32 n = 0;
    SInt8 c;

    for (c = 0; c < 7; c++)
        n += N_SCRATCH(c);
    return n;
}
HOOK(0x498ea0, count_scratch_registers);

void clear_552400(void) {
    int i;

#ifdef RECOMP_NEGATIVE_CONTROL_REGISTERINFO /* --cflags=-DRECOMP_NEGATIVE_CONTROL_REGISTERINFO */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "REGISTERINFO@"); /* marks runs */
#endif
    for (i = 0; i < 0x80; i++)
        ((SInt32 *)0x552400)[i] = 0;
}
HOOK(0x498ec0, clear_552400);

/* a class without vtable whose only member variable is a 16-byte integer (a VU0 vector): ivars
   +0x16 (next +4, type +0xc), vtable +0x1e */
UInt8 is_vu0_vector_class(UInt8 *type) {
    UInt8 *ivar, *t;

    if (!type || type[0] != 5 || *(void **)(type + 0x1e))
        return 0;
    ivar = *(UInt8 **)(type + 0x16);
    if (!ivar || *(void **)(ivar + 4))
        return 0;
    t = *(UInt8 **)(ivar + 0xc);
    return t[0] == 1 && *(SInt32 *)(t + 2) == 0x10;
}
HOOK(0x499030, is_vu0_vector_class);
