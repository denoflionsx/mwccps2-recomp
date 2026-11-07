/*
 * IroVars.c — the optimizer's variable records (mweppc src/compiler/optimizer/IroVars.c),
 * partial port.
 *
 * 5 of the source's functions are functions of their own in our exe, all byte-identical between
 * mweppc's GameCube 2.0 compiler and ours. Ported: IRO_FindVar, IRO_AddElmToList and
 * IRO_DecomposeAddressExpression_Cheap; the two table initializers stay the original's.
 *
 * VarRecord (0x22 bytes, packed): index (short) +0, object +2, unused +6, inductionState +0xa,
 * noregister +0xb, next +0xe, defs +0x12, uses +0x16, bitfieldType +0x1a, bitFieldReplacement
 * +0x1e. Object: datatype +2 (DLOCAL 1), varptr +0x24, u.var.info +0x28. VarInfo: usage +4,
 * noregister +0x2a. IROLinear: type +0, nodetype +1, u +0x20 (node; diadic left), +0x24 right.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void *, oalloc, 0x4333a0, (SInt32 size));
#define IRO_NumVars ORIG_VAR(SInt32, 0x57c8c0)
#define IRO_FirstVar ORIG_VAR(UInt8 *, 0x57b6c4)
#define IRO_LastVar ORIG_VAR(UInt8 *, 0x57c9f8)
#define IRO_BaseTerms ORIG_VAR(SInt32, 0x57c804)
#define IRO_VarTerms ORIG_VAR(SInt32, 0x57ca14)
#define TheBaseTerm ORIG_VAR(UInt8 *, 0x567730)

UInt8 *IRO_FindVar(UInt8 *object, Bool8 create, Bool8 allowRegister) {
    UInt8 *var = F(object, 0x24, UInt8 *), *vi;

    if (!var && create) {
        vi = 0;
        if (F(object, 2, UInt8) == 1) /* DLOCAL */
            vi = F(object, 0x28, UInt8 *);
        if (vi)
            F(vi, 4, UInt32) = 0; /* usage */
        var = oalloc(0x22);
        F(var, 2, UInt8 *) = object;
        F(var, 0, SInt16) = (SInt16)++IRO_NumVars;
        F(var, 6, UInt32) = 0;
        F(var, 0xa, Bool8) = 0;
        F(var, 0xe, UInt8 *) = 0;
        F(var, 0x12, void *) = 0;
        F(var, 0x16, void *) = 0;
        F(var, 0x1a, void *) = 0;
        F(var, 0x1e, void *) = 0;
        F(var, 0xb, Bool8) = 0;
        if (IRO_FirstVar)
            F(IRO_LastVar, 0xe, UInt8 *) = var;
        else
            IRO_FirstVar = var;
        IRO_LastVar = var;
        F(object, 0x24, UInt8 *) = var;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_IROVARS2 /* coarser: allowRegister ignored */
    allowRegister = 1;
#endif
    if (var && !allowRegister) {
        F(var, 0xb, Bool8) = 1;
        if (F(object, 2, UInt8) == 1 && F(object, 0x28, UInt8 *) && F(F(object, 0x28, UInt8 *), 0x2a, UInt8) == 0)
            F(F(object, 0x28, UInt8 *), 0x2a, UInt8) = 2;
    }
    return var;
}
HOOK(0x4cf140, IRO_FindVar);

void IRO_AddElmToList(UInt8 *linear, void **list) {
    void **l = oalloc(8);
    l[0] = linear; /* element */
    l[1] = 0;      /* next */
    if (*list)
        l[1] = *list;
    *list = l;
}
HOOK(0x4cf590, IRO_AddElmToList);

static void count_term(UInt8 *t) {
    if (F(t, 0, UInt8) == 3 && F(t, 1, UInt8) == 0xf) { /* diadic EADD */
        void IRO_DecomposeAddressExpression_Cheap(UInt8 * linear);
        IRO_DecomposeAddressExpression_Cheap(t);
    } else if (!(F(t, 0, UInt8) == 1 && F(F(t, 0x20, UInt8 *), 0, UInt8) == 0x32)) { /* not EINTCONST */
        if (F(t, 0, UInt8) == 1 && F(F(t, 0x20, UInt8 *), 0, UInt8) == 0x38) {     /* EOBJREF */
            IRO_BaseTerms++;
            TheBaseTerm = t;
        } else {
#ifndef RECOMP_NEGATIVE_CONTROL_IROVARS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROVARS */
            IRO_VarTerms++;
#endif
        }
    }
}

void IRO_DecomposeAddressExpression_Cheap(UInt8 *linear) {
    count_term(F(linear, 0x20, UInt8 *));
    count_term(F(linear, 0x24, UInt8 *));
}
HOOK(0x4cf6b0, IRO_DecomposeAddressExpression_Cheap);
