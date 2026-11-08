/*
 * CExpr2.c — expression helpers of the C/C++ frontend (mweppc src/compiler/frontend/CExpr2.c),
 * partial port.
 *
 * The functions here are byte-identical between mweppc's GameCube 2.0 compiler and ours. Ported so
 * far: the node constructors nullnode, intconstnode, makediadicnode, create_objectrefnode and
 * funccallexpr. The matching and conversion code stays the original's.
 *
 * ENode (0x28 bytes): type (byte) +0, cost (byte) +1, flags (UInt16) +2 (ENODE_FLAG_QUALS 3),
 * rtype +0xa, data +0x18 (diadic left/right +0x18/+0x1c; funccall funcref +0x18, args +0x1c,
 * functype +0x20). ENodeList: next +0, node +4. EINTCONST 0x32, EFUNCCALL 0x36. TypeFunc: type 6,
 * functype +0xe, qual +0x12. Object.type +0xc.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))
#define S(addr) ((const char *)(addr))

ORIG_FN(void *, lalloc, 0x433320, (SInt32 size));
ORIG_FN(UInt8 *, CExpr_NewENode, 0x4ef270, (UInt8 type));
ORIG_FN(void, CInt64_SetLong, 0x4bcfb0, (void *value, SInt32 l));
ORIG_FN(void *, CExpr_MakeObjRefNode, 0x4f00f0, (void *obj, Bool8 flag));
ORIG_FN(void *, CExpr_AdjustFunctionCall, 0x4f02b0, (void *expr));
ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));

typedef Bool8 (*NameObjCheck)(void *a, void *obj);
#define name_obj_check ORIG_VAR(NameObjCheck, 0x57c930)
#define stsignedlong ((void *)0x536070)
#define void_ptr ((void *)0x532090)

UInt8 *nullnode(void) {
    UInt8 *expr = CExpr_NewENode(0x32);
    F(expr, 0xa, void *) = stsignedlong;
    return expr;
}
HOOK(0x4ef2b0, nullnode);

UInt8 *intconstnode(void *type, SInt32 value) {
    UInt8 *expr = CExpr_NewENode(0x32);
    F(expr, 0xa, void *) = type;
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPR2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPR2 */
    value++;
#endif
    CInt64_SetLong(expr + 0x18, value);
    return expr;
}
HOOK(0x4ef2c0, intconstnode);

UInt8 *makediadicnode(UInt8 *left, UInt8 *right, UInt8 ty) {
    UInt8 *expr = lalloc(0x28);
    expr[0] = ty;
    F(expr, 0xa, void *) = F(left, 0xa, void *);
    F(expr, 0x18, void *) = left;
    F(expr, 0x1c, void *) = right;
    if (left[1] != right[1]) {
        expr[1] = right[1];
        if (left[1] > expr[1])
            expr[1] = left[1];
    } else {
        expr[1] = right[1] + 1;
        if (expr[1] > 200)
            expr[1] = 200;
    }
    F(expr, 2, UInt16) = (F(left, 2, UInt16) | F(right, 2, UInt16)) & 3;
    return expr;
}
HOOK(0x4ef360, makediadicnode);

void *create_objectrefnode(void *obj) {
    if (name_obj_check && !name_obj_check(0, obj))
        return intconstnode(void_ptr, 0);
    return CExpr_MakeObjRefNode(obj, 1);
}
HOOK(0x4f0170, create_objectrefnode);

void *funccallexpr(void *func, void *arg1, void *arg2, void *arg3, void *arg4) {
    UInt8 *expr, *tfunc;
    void **list;

    tfunc = F(func, 0xc, UInt8 *);
    if (tfunc[0] != 6)
        CError_Internal(S(0x544948) /* "CExpr2.c" */, 1411);
    expr = lalloc(0x28);
    expr[0] = 0x36;
    expr[1] = 4;
    F(expr, 0xa, void *) = F(tfunc, 0xe, void *);
    F(expr, 2, UInt16) = F(tfunc, 0x12, UInt16) & 3;
    F(expr, 0x18, void *) = create_objectrefnode(func);
    F(expr, 0x20, void *) = tfunc;
    if (arg1) {
        list = lalloc(8);
        F(expr, 0x1c, void *) = list;
        list[1] = arg1;
        if (arg2) {
            list[0] = lalloc(8);
            list = (void **)list[0];
            list[1] = arg2;
            if (arg3) {
                list[0] = lalloc(8);
                list = (void **)list[0];
                list[1] = arg3;
                if (arg4) {
                    list[0] = lalloc(8);
                    list = (void **)list[0];
                    list[1] = arg4;
                }
            }
        }
        list[0] = 0;
    } else {
        F(expr, 0x1c, void *) = 0;
    }
    return CExpr_AdjustFunctionCall(expr);
}
HOOK(0x4f03c0, funccallexpr);

/* ---- part 2: CExpr_NewENode and CExpr_IsTempConstruction (close to mweppc: same logic) ---- */

/* The rest of this file calls CExpr_NewENode through the original address, which now jumps here.
   EINDIRECT 4, EOBJREF 0x38 (objref +0x18). */
ORIG_FN(void, memclrw, 0x4334d0, (void *ptr, SInt32 size));
ORIG_FN(Bool8, CClass_IsConstructor, 0x45ee50, (void *obj));
ORIG_FN(SInt16, CABI_GetStructResultArgumentIndex, 0x4529d0, (void *functype));

UInt8 *CExpr_NewENode_(UInt8 ty) {
    UInt8 *expr = lalloc(0x28);
    memclrw(expr, 0x28);
    expr[0] = ty;
    return expr;
}
HOOK(0x4ef270, CExpr_NewENode_);

void *CExpr_IsTempConstruction(UInt8 *expr, void *type, void **resultexpr) {
    UInt8 *funccall;
    UInt8 *funcref;
    void **args;

    if (expr[0] != 4 || F(expr, 0xa, void *) != type)
        return 0;
    funccall = F(expr, 0x18, UInt8 *);
    if (funccall[0] != 0x36 || !(args = F(funccall, 0x1c, void **)))
        return 0;
    funcref = F(funccall, 0x18, UInt8 *);
    if (funcref[0] != 0x38 || !CClass_IsConstructor(F(funcref, 0x18, void *))) {
        if (F(F(F(expr, 0x18, UInt8 *), 0x20, UInt8 *), 0xe, void *) != type)
            return 0;
        if (CABI_GetStructResultArgumentIndex(F(F(expr, 0x18, UInt8 *), 0x20, void *)) == 1) {
            args = args[0];
            if (!args)
                CError_Internal(S(0x544948) /* "CExpr2.c" */, 1277);
        }
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPR2B /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPR2B */
    return 0; /* no temporary constructions recognised */
#endif
    if (resultexpr)
        *resultexpr = args[1];
    return F(expr, 0x18, void *);
}
HOOK(0x4f0220, CExpr_IsTempConstruction);

/* ---- part 3: node helpers not matched by gcmatch (the same logic as mweppc's) ---- */

/* Our integer constants are 128-bit (CInt64 is 16 bytes here: the PS2 has 128-bit integers);
   float constants are 8-byte doubles. Both are passed by value. cint64_one 0x56b4e8.
   Object: type +0xc, qual +0x10, flags +0x16 (OBJECT_USED 1), sclass (short) +0x14 (TK_TYPEDEF
   0x104). TypeIntegral.integral +6 (IT_INT 7). name_obj_check 0x57c930 (defined above). void_ptr 0x532090.
   ETYPCON 0x30, EINTCONST 0x32, EFLOATCONST 0x33, EINDIRECT 4, EOBJREF 0x38. */
typedef struct { UInt32 w[4]; } CInt128;
typedef struct { UInt32 w[2]; } Float8;

ORIG_FN(void, CError_Error, 0x456d50, (int code, ...));
ORIG_FN(void *, CDecl_NewPointerType, 0x443190, (void *type));
ORIG_FN(UInt8 *, nullnode_, 0x4ef2b0, (void));
ORIG_FN(void *, checkreference, 0x467a50, (void *expr));
ORIG_FN(UInt8 *, forceintegral, 0x4ef2f0, (UInt8 *expr));
ORIG_FN(Bool8, CInt64_IsZero, 0x4bd000, (const void *value));
ORIG_FN(Bool8, CInt64_Equal, 0x4bdd70, (CInt128 a, CInt128 b));
ORIG_FN(Bool8, CMach_FloatIsZero, 0x4589e0, (Float8 f));
ORIG_FN(Bool8, CMach_FloatIsOne, 0x458a00, (Float8 f));
#define cint64_one ORIG_VAR(CInt128, 0x56b4e8)

UInt8 *makemonadicnode(UInt8 *inner, UInt8 ty) {
    UInt8 *expr = lalloc(0x28);
    expr[0] = ty;
    if ((expr[1] = inner[1]) == 0)
        expr[1] = 1;
    F(expr, 2, UInt16) = F(inner, 2, UInt16) & 3;
    F(expr, 0xa, void *) = F(inner, 0xa, void *);
    F(expr, 0x18, void *) = inner;
    return expr;
}
HOOK(0x4ef320, makemonadicnode);

UInt8 *CExpr_MakeObjRefNode_(UInt8 *obj, Bool8 markUsed) {
    UInt8 *expr;

    if (F(obj, 0x14, SInt16) == 0x104) { /* typedef */
        CError_Error(0x8d);
        return intconstnode((void *)0x532090 /* void_ptr */, 0);
    }
    expr = lalloc(0x28);
    memclrw(expr, 0x28);
    expr[0] = 0x38; /* EOBJREF */
    F(expr, 0x18, void *) = obj;
    F(expr, 0xa, void *) = CDecl_NewPointerType(F(obj, 0xc, void *));
    if (F(obj, 0xc, UInt8 *)[0] != 6)
        F(expr, 2, UInt16) = F(obj, 0x10, UInt32) & 3;
    if (markUsed)
        obj[0x16] |= 1;
    return expr;
}
HOOK(0x4f00f0, CExpr_MakeObjRefNode_);

UInt8 *create_objectnode2(UInt8 *obj) {
    UInt8 *expr;

    if (name_obj_check && !name_obj_check(0, obj))
        return nullnode_();
    expr = makemonadicnode(CExpr_MakeObjRefNode_(obj, 1), 4); /* EINDIRECT */
    F(expr, 0xa, void *) = F(F(expr, 0xa, UInt8 *), 6, void *);
    return expr;
}
HOOK(0x4f01b0, create_objectnode2);

void *create_objectnode(UInt8 *obj) {
    return checkreference(create_objectnode2(obj));
}
HOOK(0x4f0200, create_objectnode);

UInt8 *integralpromote(UInt8 *expr) {
    if (F(expr, 0xa, UInt8 *)[0] != 1)
        expr = forceintegral(expr);
    if (F(expr, 0xa, UInt8 *)[6] >= 7)
        return expr;
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPR2C /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPR2C */
    return expr; /* no promotion to int */
#endif
    if (expr[0] != 0x32)
        expr = makemonadicnode(expr, 0x30); /* ETYPCON */
    F(expr, 0xa, void *) = (void *)0x536060; /* stsignedint */
    return expr;
}
HOOK(0x4ef7a0, integralpromote);

int iszero(UInt8 *expr) {
    switch (expr[0]) {
    case 0x32:
        return CInt64_IsZero(expr + 0x18);
    case 0x33:
        return CMach_FloatIsZero(F(expr, 0x18, Float8));
    }
    return 0;
}
HOOK(0x4ef450, iszero);

Bool8 CExpr_IsOne(UInt8 *expr) {
    if (expr[0] == 0x32)
        return CInt64_Equal(F(expr, 0x18, CInt128), cint64_one);
    if (expr[0] == 0x33)
        return CMach_FloatIsOne(F(expr, 0x18, Float8));
    return 0;
}
HOOK(0x4ef540, CExpr_IsOne);
