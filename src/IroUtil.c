/*
 * IroUtil.c — optimizer helpers on the linear IR (mweppc src/compiler/optimizer/IroUtil.c),
 * partial port.
 *
 * 21 of the source's functions are in our exe as functions of their own, all byte-identical
 * between mweppc's GameCube 2.0 compiler and ours. Ported: the 14 small ones the corpus runs, plus
 * the static walk callbacks ActNop and FindStart (not hooked: only
 * reached through the pointers IRO_NopOut and IRO_FindStart pass, which are now ours). IRO_ExprsSame, IRO_TypesEqual,
 * IRO_is_CPtypeequal, IRO_UnsignedType, IRO_LocateFather_Cut_And_Paste_Without_Nopping,
 * IRO_FindFirst / FindFirst and ActNopNonSideEffects stay the original's.
 *
 * IROLinear from our code: type (byte) +0, rtype +0xe, expr +0x12, u +0x20 (operand node,
 * monadic, diadic.left; diadic.right +0x24; args3 a/b/c +0x20/+0x24/+0x28; funccall argCount
 * (short) +0x22, args +0x24, callee +0x28), next +0x34. Types: Nop 0, Operand 1, Op1Arg 2,
 * Op2Arg 3, Op3Arg 10, Funccall 11. ENode type byte +0: EINTCONST 0x32, EFLOATCONST 0x33.
 * IROAddrRecord (0x1a bytes, packed): linear +0, numObjRefs +4, objRefs +6, numMisc +0xa,
 * misc +0xc, numInts +0x10, ints +0x12, unused +0x16.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef UInt8 IROLinear;
typedef void (*IROWalkTreeFunc)(IROLinear *linear, Bool8 isEntry);
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))
#define L_type(l) F(l, 0, UInt8)
#define L_u0(l) F(l, 0x20, IROLinear *)
#define L_u1(l) F(l, 0x24, IROLinear *)
#define L_u2(l) F(l, 0x28, IROLinear *)
#define L_argCount(l) F(l, 0x22, SInt16)
#define L_args(l) F(l, 0x24, IROLinear **)
#define L_next(l) F(l, 0x34, IROLinear *)

typedef struct IROList {
    IROLinear *head, *tail;
} IROList;

ORIG_FN(SInt32, COS_GetTicks, 0x441880, (void));
ORIG_FN_STDCALL(int, CWUserBreak, 0x41f9e0, (void *context));
ORIG_FN(void, CError_UserBreak, 0x4572c0, (void));
ORIG_FN(void *, oalloc, 0x4333a0, (SInt32 size));
ORIG_FN(void *, create_temp_object, 0x470ce0, (void *type));
ORIG_FN(void *, IRO_FindVar, 0x4cf140, (void *object, Bool8 flag1, Bool8 flag2));
ORIG_FN(void *, newlabel, 0x470bb0, (void));
#define ActNopNonSideEffects ((IROWalkTreeFunc)0x4bf4f0)

#define IRO_LastUserBreakTick ORIG_VAR(UInt32, 0x5671d8)
#define cparams_context ORIG_VAR(void *, 0x57da14) /* cparams.context */
#define FuncLevel ORIG_VAR(SInt32, 0x5671c8)
#define ExprEnd ORIG_VAR(IROLinear *, 0x5671d0) /* linear_range_end */
#define ExprStart ORIG_VAR(IROLinear *, 0x5671d4)
#define Labels ORIG_VAR(void *, 0x57d6ac)

Bool8 IRO_IsIntConstant(IROLinear *linear) {
#ifdef RECOMP_NEGATIVE_CONTROL_IROUTIL /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROUTIL */
    return 0;
#endif
    return L_type(linear) == 1 && F(L_u0(linear), 0, UInt8) == 0x32;
}
HOOK(0x4be9a0, IRO_IsIntConstant);

Bool8 IRO_IsFloatConstant(IROLinear *linear) {
    return L_type(linear) == 1 && F(L_u0(linear), 0, UInt8) == 0x33;
}
HOOK(0x4be9c0, IRO_IsFloatConstant);

void *IRO_NewLabel(void) {
    void *label = newlabel();
    F(label, 0, void *) = Labels; /* label->next */
    Labels = label;
    return label;
}
HOOK(0x4bef50, IRO_NewLabel);

void IRO_WalkTree(IROLinear *linear, IROWalkTreeFunc func) {
    int i;
    func(linear, 1);
    switch (L_type(linear)) {
    case 2:
        IRO_WalkTree(L_u0(linear), func);
        break;
    case 3:
        IRO_WalkTree(L_u0(linear), func);
        IRO_WalkTree(L_u1(linear), func);
        break;
    case 10:
        IRO_WalkTree(L_u0(linear), func);
        IRO_WalkTree(L_u1(linear), func);
        IRO_WalkTree(L_u2(linear), func);
        break;
    case 11:
        IRO_WalkTree(L_u2(linear), func); /* callee */
        for (i = 0; i < L_argCount(linear); i++)
            IRO_WalkTree(L_args(linear)[i], func);
        break;
    default:
        break;
    }
    func(linear, 0);
}
HOOK(0x4bf570, IRO_WalkTree);

static void ActNop(IROLinear *linear, Bool8 isEntry) {
    if (!isEntry) {
        L_type(linear) = 0;
        F(linear, 0x12, void *) = 0;
    }
}

void IRO_NopOut(IROLinear *linear) {
    IRO_WalkTree(linear, ActNop);
}
HOOK(0x4bf410, IRO_NopOut);

void IRO_NopNonSideEffects(IROLinear *linear, SInt32 level) {
    FuncLevel = level;
    IRO_WalkTree(linear, ActNopNonSideEffects);
}
HOOK(0x4bf550, IRO_NopNonSideEffects);

static void FindStart(IROLinear *linear, Bool8 isEntry) {
    IROLinear *scan;
    if (isEntry) {
        for (scan = linear; scan; scan = L_next(scan)) {
            if (scan == ExprStart) {
                ExprStart = linear;
                return;
            }
            if (scan == ExprEnd)
                return;
        }
    }
}

void IRO_InitList(IROList *list) {
    list->head = list->tail = 0;
}
HOOK(0x4bf9b0, IRO_InitList);

void IRO_AddToList(IROLinear *linear, IROList *list) {
    if (list->head)
        L_next(list->tail) = linear;
    else
        list->head = linear;
    list->tail = linear;
    while (L_next(list->tail))
        list->tail = L_next(list->tail);
}
HOOK(0x4bf9d0, IRO_AddToList);

void *IRO_GetTemp(UInt8 *expr) {
    F(expr, 8, void *) = create_temp_object(F(F(expr, 4, IROLinear *), 0xe, void *)); /* expr->temp */
    return IRO_FindVar(F(expr, 8, void *), 1, 1);
}
HOOK(0x4c0030, IRO_GetTemp);

IROLinear *IRO_FindStart(IROLinear *linear) {
    ExprStart = ExprEnd = linear;
    IRO_WalkTree(linear, FindStart);
    return ExprStart;
}
HOOK(0x4c0120, IRO_FindStart);

void *IRO_InitAddrRecordPointer(IROLinear *linear) {
    UInt8 *rec = oalloc(0x1a);
    F(rec, 4, UInt16) = 0;
    F(rec, 6, void *) = 0;
    F(rec, 0xa, UInt16) = 0;
    F(rec, 0xc, void *) = 0;
    F(rec, 0x10, UInt16) = 0;
    F(rec, 0x12, void *) = 0;
    F(rec, 0x16, UInt32) = 0;
    F(rec, 0, IROLinear *) = linear;
    return rec;
}
HOOK(0x4c0150, IRO_InitAddrRecordPointer);

Bool8 IRO_IsAddressMultiply(IROLinear *linear) {
    (void)linear;
    return 0;
}
HOOK(0x4c0550, IRO_IsAddressMultiply);

void IRO_CheckForUserBreak(void) {
    if (IRO_LastUserBreakTick + 8 < (UInt32)COS_GetTicks()) {
        if (CWUserBreak(cparams_context))
            CError_UserBreak();
        IRO_LastUserBreakTick = COS_GetTicks();
    }
}
HOOK(0x4c0570, IRO_CheckForUserBreak);
