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
