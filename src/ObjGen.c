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

/* ---- part 3: ObjGen_Setup and the symbol table (from the exe) ---- */

ORIG_FN(SInt32, fn_4b8270_o, 0x4b8270, (void));
ORIG_FN(void, IRO_Setup_o, 0x43ae10, (void));
ORIG_FN(void, fn_498ec0_o, 0x498ec0, (void));
ORIG_FN(void, fn_489830_o, 0x489830, (void));
ORIG_FN(void, fn_5005b0_o, 0x5005b0, (SInt32 a, UInt32 flags, SInt32 c));
ORIG_FN(void, fn_500170_o, 0x500170, (void *a, void *b));
ORIG_FN(SInt32, fn_503010_o, 0x503010, (void));
ORIG_FN(void *, CMangler_GetLinkName_o, 0x450140, (void *obj));
ORIG_FN(void *, galloc_o, 0x4332e0, (SInt32 size));
ORIG_FN(void, memclrw_o, 0x4334d0, (void *ptr, SInt32 size));
ORIG_FN(void *, fn_5001d0_o, 0x5001d0, (const char *name, SInt32 a, SInt32 b, SInt32 c, SInt32 size, SInt32 f));

/* Resets the object generator: ELF header flags 0x57ab40 (0x20000000 with copts 0x57db32 == 1),
   the counters, the symbol hash table (0x800 buckets at 0x54d8b0), then the output file (0x5005b0
   with flags 1 | ISA level bits from copts 0x57db34 | 0x924000, and 1 or 2 per copts 0x57db28) and
   its first section (0x4ffee0 ".text"?, 0x500170). Returns 0x503010's result. */
SInt32 ObjGen_Setup(void) {
    UInt32 flags;
    SInt16 isa;
    int i;

    *(UInt32 *)0x57ab40 = 0;
    if (*(SInt16 *)0x57db32 == 1)
        *(UInt32 *)0x57ab40 |= 0x20000000;
    fn_4b8270_o();
    IRO_Setup_o();
    *(SInt32 *)0x57c90c = 0;
    *(SInt32 *)0x57b714 = 0;
    *(SInt32 *)0x57c8cc = 0;
    *(SInt32 *)0x57ab7c = *(SInt32 *)0x57c8cc;
    *(SInt32 *)0x57c7f4 = 0;
    *(SInt32 *)0x57cae8 = 0;
    *(UInt8 *)0x57e16b = 0;
    fn_498ec0_o();
    *(SInt32 *)0x54d8a0 = 0;
    fn_489830_o();
    for (i = 0; i < 0x800; i++)
        ((void **)0x54d8b0)[i] = 0;
    flags = 1;
    isa = *(SInt16 *)0x57db34;
#ifdef RECOMP_NEGATIVE_CONTROL_OBJGEN2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_OBJGEN2 */
    isa++; /* ISA level one higher in the ELF flags */
#endif
    if (isa >= 4)
        flags |= 0x30000000;
    else if (isa >= 3)
        flags |= 0x20000000;
    else if (isa >= 2)
        flags |= 0x10000000;
    fn_5005b0_o(8, flags | 0x924000, *(UInt8 *)0x57db28 ? 1 : 2);
    fn_500170_o(fn_4ffee0((void *)0x539760, (void *)1, 0), (void *)0x53976c);
    return fn_503010_o();
}
HOOK(0x489ee0, ObjGen_Setup);

/* An object's ELF symbol: looked up by link name (hash +8) in the table 0x54d8b0 (entries: next
   +0, name +4, symbol +8), else made (0x5001d0 with the name's text +0xa; data objects (datatype +2
   == 0) pass their type's (+0xc) size (+2)) and entered. */
void *ObjGen_GetSymbol(UInt8 *obj) {
    UInt8 *name = CMangler_GetLinkName_o(obj);
    void **e;
    void **bucket = &((void **)0x54d8b0)[F(name, 8, SInt16)];

    for (e = *bucket; e; e = (void **)e[0]) {
        if (e[1] == name)
            return e[2];
    }
    e = galloc_o(0xc);
    memclrw_o(e, 0xc);
    e[0] = *bucket;
    *bucket = e;
    e[1] = name;
    if (obj[2] == 0)
        e[2] = fn_5001d0_o((const char *)name + 0xa, 0, 1, 1, F(F(obj, 0xc, UInt8 *), 2, SInt32), 0);
    else
        e[2] = fn_5001d0_o((const char *)name + 0xa, 0, 1, 0, 0, 0);
    return e[2];
}
HOOK(0x48a0c0, ObjGen_GetSymbol);

/* Sections_Init: the ELF sections (Section_Define 0x4895d0 with name, ELF name, alternative name,
   type and flags strings of the exe; it returns the section index). Small data sections with copts
   0x57db30, the .bss family with 0x57db2f; sections with byte +0x2e of their 48-byte entry
   (0x54f8b0) set are small data. */
ORIG_FN(SInt32, Section_Define_o, 0x4895d0, (UInt32 a, UInt32 b, UInt32 c, UInt32 d, UInt32 e));
#define SECT_FLAG(i) (((UInt8 *)0x54f8de)[(i) * 48])

void Sections_Init(void) {
    SInt32 r;

    *(SInt32 *)0x57c790 = 1;
    *(SInt32 *)0x57c960 = Section_Define_o(0x5395dc, 0x5395d4, 0x5395cc, 0x539598, 0x5395b4);
    *(SInt32 *)0x57c95c = Section_Define_o(0x5395f4, 0x5395ec, 0x5395e4, 0x539560, 0x5395b4);
    *(SInt32 *)0x57cd98 = r = Section_Define_o(0x539604, 0x5395fc, 0, 0x539598, 0x5395b4);
    SECT_FLAG(r) = 1;
    *(SInt32 *)0x57ab68 = r = Section_Define_o(0x539614, 0x53960c, 0, 0x539598, 0x5395b4);
    SECT_FLAG(r) = 1;
    *(SInt32 *)0x57c8b0 = Section_Define_o(0x539624, 0x53961c, 0, 0x539570, 0x5395b0);
    *(SInt32 *)0x57b12c = Section_Define_o(0x539634, 0x53962c, 0, 0x539570, 0x5395b8);
    if (*(UInt8 *)0x57db30) {
        *(SInt32 *)0x57b78c = Section_Define_o(0x539644, 0x53963c, 0, 0x53958c, 0x5395b8);
        *(SInt32 *)0x57c7dc = Section_Define_o(0x539654, 0x53964c, 0, 0x53958c, 0x5395b0);
        *(SInt32 *)0x57c88c = Section_Define_o(0x539668, 0x53965c, 0, 0x53958c, 0x5395b0);
        *(SInt32 *)0x57cadc = Section_Define_o(0x539680, 0x539674, 0, 0x53958c, 0x5395b0);
        *(SInt32 *)0x57b7a8 = Section_Define_o(0x539698, 0x53968c, 0, 0x5395a4, 0x5395b0);
    } else {
        *(SInt32 *)0x57b78c = Section_Define_o(0x539644, 0x53963c, 0, 0x539570, 0x5395b8);
        *(SInt32 *)0x57c7dc = Section_Define_o(0x539654, 0x53964c, 0, 0x539570, 0x5395b0);
        *(SInt32 *)0x57c88c = Section_Define_o(0x539668, 0x53965c, 0, 0x539570, 0x5395b0);
        *(SInt32 *)0x57cadc = Section_Define_o(0x539680, 0x539674, 0, 0x539570, 0x5395b0);
    }
    if (*(UInt8 *)0x57db2f) {
        *(SInt32 *)0x57c934 = Section_Define_o(0x5396b0, 0x5396a8, 0x5396a0, 0x5395a4, 0x5395b4);
        *(SInt32 *)0x57cad0 = Section_Define_o(0x5396c4, 0x5396b8, 0, 0x5395a4, 0x5395b0);
        *(SInt32 *)0x57b7a4 = Section_Define_o(0x5396d8, 0x5396cc, 0, 0x5395a4, 0x5395b0);
    } else {
        *(SInt32 *)0x57c934 = Section_Define_o(0x5396b0, 0x5396a8, 0x5396a0, 0x539570, 0x5395b4);
        *(SInt32 *)0x57cad0 = Section_Define_o(0x5396c4, 0x5396b8, 0, 0x539570, 0x5395b0);
    }
    *(SInt32 *)0x57d720 = Section_Define_o(0x5396f8, 0x5396ec, 0x5396e0, 0x539570, 0x5395b4);
    *(SInt32 *)0x57b730 = Section_Define_o(0x53970c, 0x539704, 0, 0x539570, 0x5395b8);
    *(SInt32 *)0x57c8f8 = Section_Define_o(0x539724, 0x53971c, 0x539714, 0x539570, 0x5395b4);
}
HOOK(0x489830, Sections_Init);
