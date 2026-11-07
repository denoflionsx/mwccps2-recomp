/*
 * CParser.c — parser helpers of the C/C++ frontend (mweppc src/compiler/frontend/CParser.c),
 * partial port.
 *
 * The functions here are byte-identical between mweppc's GameCube 2.0 compiler and ours. Ported so
 * far: the type-qualifier queries, the CV-qualification comparisons, appendmember, is_unsigned,
 * CParser_NewFuncArg, IsTempName, the unique names and IDs, CParser_FreeLocalHeap and
 * CParser_GlobalCleanup. is_typesame, is_typeequal and the rest stay the original's.
 *
 * Type from our code: type (byte) +0 with array 0xc (target +6), enum 3 (enumtype +0xe); the
 * qualifier word is at +0xe for type 0xa and at +0xa for type 0xb (the exe's jump table).
 * Q_CONST 1, Q_VOLATILE 2. HashNameNode.name +0xa. TypeStruct.members +0xa, member.next +0.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(Bool8, CMach_IsUnsignedType, 0x4590c0, (void *type));
ORIG_FN(void *, galloc, 0x4332e0, (SInt32 size));
ORIG_FN(void, memclrw, 0x4334d0, (void *ptr, SInt32 size));
ORIG_FN(void, CParser_PrintUniqueID, 0x42e970, (char *buf));
ORIG_FN(void *, GetHashNameNodeExport, 0x432ef0, (const char *name));
ORIG_FN(void, CFunc_GenerateSingleExprFunc, 0x474bf0, (void *func, void *expr));
ORIG_FN(void, CFunc_GenerateDummyCtorFunc, 0x474c90, (void *func, void *obj));
ORIG_FN(Bool8, CInline_CanFreeLHeap, 0x466510, (void));
ORIG_FN(void, freelheap, 0x433410, (void));
ORIG_FN(void, CClass_ClassAction, 0x4606f0, (void *tclass));
ORIG_FN(Bool8, CTempl_Instantiate, 0x4779a0, (void));
ORIG_FN(Bool8, CInline_GenerateDeferredFuncs, 0x466540, (void));

#define uniqueid ORIG_VAR(SInt32, 0x54cb44)
#define cparser_sfunclist ORIG_VAR(void **, 0x54cb38)     /* {next, func, obj, expr} */
#define cparser_parentcleanup ORIG_VAR(void **, 0x54cb3c) /* {next, tclass} */
#define cparser_classactions ORIG_VAR(void **, 0x54cb40)  /* {next, tclass} */

static UInt32 type_qual(UInt8 *type, UInt32 qual) {
    while (type[0] == 0xc) /* array */
        type = F(type, 6, UInt8 *);
    if (type[0] == 0xa)
        qual = F(type, 0xe, UInt32);
    else if (type[0] == 0xb)
        qual = F(type, 0xa, UInt32);
    return qual;
}

Bool8 CParser_IsVolatile(UInt8 *type, UInt32 qual) {
    return (type_qual(type, qual) & 2) != 0;
}
HOOK(0x430590, CParser_IsVolatile);

Bool8 CParser_IsConst(UInt8 *type, UInt32 qual) {
#ifdef RECOMP_NEGATIVE_CONTROL_CPARSER /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPARSER */
    return 0;
#endif
    return (type_qual(type, qual) & 1) != 0;
}
HOOK(0x430550, CParser_IsConst);

UInt32 CParser_GetCVTypeQualifiers(UInt8 *type, UInt32 qual) {
    return type_qual(type, qual) & 3;
}
HOOK(0x430510, CParser_GetCVTypeQualifiers);

UInt32 CParser_GetTypeQualifiers(UInt8 *type, UInt32 qual) {
    return type_qual(type, qual);
}
HOOK(0x4304d0, CParser_GetTypeQualifiers);

void appendmember(UInt8 *tstruct, void **member) {
    void **last;
    if (!F(tstruct, 0xa, void **)) {
        F(tstruct, 0xa, void **) = member;
        return;
    }
    for (last = F(tstruct, 0xa, void **); *last; last = (void **)*last)
        ;
    *last = member;
}
HOOK(0x42fec0, appendmember);

Bool8 is_unsigned(UInt8 *type) {
    if (type[0] == 3) /* enum */
        type = F(type, 0xe, UInt8 *);
    return CMach_IsUnsignedType(type);
}
HOOK(0x42fe70, is_unsigned);

Bool8 CParser_IsMoreCVQualified(UInt32 a, UInt32 b) {
    if ((a & 1) && !(b & 1))
        return (a & 2) || (b & 2) == 0;
    if ((a & 2) && !(b & 2))
        return (a & 1) || (b & 1) == 0;
    return 0;
}
HOOK(0x42fda0, CParser_IsMoreCVQualified);

Bool8 CParser_IsSameOrMoreCVQualified(UInt32 a, UInt32 b) {
    if ((a & 3) == (b & 3))
        return 1;
    return CParser_IsMoreCVQualified(a, b);
}
HOOK(0x42fe00, CParser_IsSameOrMoreCVQualified);

void *CParser_NewFuncArg(void) {
    void *arg = galloc(0x18);
    memclrw(arg, 0x18);
    return arg;
}
HOOK(0x42efa0, CParser_NewFuncArg);

Bool8 IsTempName(UInt8 *name) {
    return !name || name[0xa] == '@' || name[0xa] == '$';
}
HOOK(0x42ec70, IsTempName);

void *CParser_GetUniqueName(void) {
    char buf[16];
    buf[0] = '@';
    CParser_PrintUniqueID(buf + 1);
    return GetHashNameNodeExport(buf);
}
HOOK(0x42e9f0, CParser_GetUniqueName);

SInt32 CParser_GetUniqueID(void) {
    return uniqueid++;
}
HOOK(0x42e960, CParser_GetUniqueID);

void CParser_FreeLocalHeap(void) {
    void **s, **p;
    UInt8 *nspace;

    while (cparser_sfunclist) {
        s = cparser_sfunclist;
        cparser_sfunclist = (void **)s[0];
        if (s[3])
            CFunc_GenerateSingleExprFunc(s[1], s[3]);
        else
            CFunc_GenerateDummyCtorFunc(s[1], s[2]);
    }
    if (cparser_parentcleanup) {
        if (!CInline_CanFreeLHeap())
            return;
        for (p = cparser_parentcleanup; p; p = (void **)p[0]) {
            nspace = F(p[1], 6, UInt8 *); /* tclass->nspace */
            while (!F(F(nspace, 0, UInt8 *), 0x1d, Bool8)) /* !nspace->parent->is_global */
                F(nspace, 0, UInt8 *) = F(F(nspace, 0, UInt8 *), 0, UInt8 *);
        }
        cparser_parentcleanup = 0;
    }
    freelheap();
}
HOOK(0x431f90, CParser_FreeLocalHeap);

void CParser_GlobalCleanup(Bool8 runClassActions) {
    Bool8 working;
    do {
        CParser_FreeLocalHeap();
        working = 0;
        if (runClassActions) {
            if (cparser_classactions) {
                CClass_ClassAction(cparser_classactions[1]);
                cparser_classactions = (void **)cparser_classactions[0];
                working = 1;
            } else if (CTempl_Instantiate()) {
                working = 1;
            }
        }
        while (CInline_GenerateDeferredFuncs()) {
            CParser_FreeLocalHeap();
            working = 1;
        }
    } while (working);
}
HOOK(0x432020, CParser_GlobalCleanup);
