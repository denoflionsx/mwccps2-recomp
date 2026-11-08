/*
 * ObjGen.c — object file generation (mweppc src/compiler/ObjGen*.c has no ELF/MIPS counterpart),
 * partial port of the exe's small helpers.
 *
 * Ported so far (from the exe): the cleanup that frees the relocation list and the back end's
 * tables (0x48a0a0), the cached section-symbol lookup (0x489e00) and the relocation-kind
 * classifier (0x48a170).
 */
#include "orig.h"

ORIG_FN(void, FreeGList, 0x432ae0, (void *list));
ORIG_FN(void, fn_500a00, 0x500a00, (void));
ORIG_FN(SInt32, fn_4b8280, 0x4b8280, (void));
ORIG_FN(SInt32, fn_4a64d0, 0x4a64d0, (void));
ORIG_FN(void *, fn_4ffee0, 0x4ffee0, (void *a, void *b, void *c));
ORIG_FN(void, fn_489ce0, 0x489ce0, (void *x, SInt32 *out));

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

SInt32 ObjGen_Cleanup(void) {
    FreeGList((void *)0x54d8a0);
    fn_500a00();
    fn_4b8280();
    return fn_4a64d0();
}
HOOK(0x48a0a0, ObjGen_Cleanup);

/* section table 0x54f8b0 (0x30-byte entries): two (a, b, cached) triples at +0xa and +0x16 and
   an argument at +6; the second triple is used when flag is 0 and it is set. */
void *fn_489e00(SInt32 index, SInt32 flag) {
    UInt8 *entry = (UInt8 *)0x54f8b0 + index * 0x30;
    void **triple = (void **)(entry + 0xa);

    if (!flag && F(entry, 0x16, void *))
        triple = (void **)(entry + 0x16);
    triple[2] = fn_4ffee0(triple[0], triple[1], F(entry, 6, void *));
    return triple[2];
}
HOOK(0x489e00, fn_489e00);

SInt32 fn_48a170(void *x) {
    SInt32 info[4];

    fn_489ce0(x, info);
#ifdef RECOMP_NEGATIVE_CONTROL_OBJGEN /* --cflags=-DRECOMP_NEGATIVE_CONTROL_OBJGEN */
    return info[0] == 0x17 ? 1 : 2; /* wrong kinds */
#endif
    if (info[0] == 0x17)
        return 2;
    if (info[0] == 0x1c)
        return 0;
    return 1;
}
HOOK(0x48a170, fn_48a170);

/* The object file writer's finish: section setup (0x45e280), each entry of the list 0x57ab7c
   (next +0, item +4) through 0x489e80, 0x503130, then the output GList 0x54d8a0 is built
   (0x5008e0) and handed to the parameter block (cparamblkptr 0x57ca38: object data +0x34; sizes
   +0x10, +0x18, +0x14 filled by 0x500a30), then the back end's tables are freed (0x4b8280). */
ORIG_FN(void, fn_45e280, 0x45e280, (void));
ORIG_FN(void, fn_489e80, 0x489e80, (void *item));
ORIG_FN(void, fn_503130, 0x503130, (void));
ORIG_FN(SInt16, InitGList, 0x432a90, (void *list, SInt32 size));
ORIG_FN(void, CError_NoMem, 0x4572a0, (void));
ORIG_FN(void, fn_5008e0, 0x5008e0, (void *list));
ORIG_FN(void, fn_500a30, 0x500a30, (void *a, void *b, void *c));
#define cparamblkptr ORIG_VAR(UInt8 *, 0x57ca38)

SInt32 ObjGen_Finish(void) {
    void **node;

    fn_45e280();
    for (node = *(void ***)0x57ab7c; node; node = (void **)node[0])
        fn_489e80(node[1]);
    fn_503130();
    if (InitGList((void *)0x54d8a0, 0x8000))
        CError_NoMem();
    fn_5008e0((void *)0x54d8a0);
    F(cparamblkptr, 0x34, void *) = *(void **)0x54d8a0;
    *(void **)0x54d8a0 = 0;
    fn_500a30(cparamblkptr + 0x10, cparamblkptr + 0x18, cparamblkptr + 0x14);
    return fn_4b8280();
}
HOOK(0x48a000, ObjGen_Finish);

/* the third triple of a section entry (+0x22) with argument 0, then handed to 0x4fffd0 with the
   entry's word at +0x12 */
ORIG_FN(void, fn_4fffd0, 0x4fffd0, (void *a, void *b));

void *fn_489e40(SInt32 index) {
    UInt8 *entry = (UInt8 *)0x54f8b0 + index * 0x30;
    void **triple = (void **)(entry + 0x22);

    triple[2] = fn_4ffee0(triple[0], triple[1], 0);
    fn_4fffd0(F(entry, 0x12, void *), triple[2]);
    return triple[2];
}
HOOK(0x489e40, fn_489e40);
