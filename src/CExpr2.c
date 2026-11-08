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

/* ---- part 4: CExpr_AllBitsSet (mweppc's logic on our 128-bit constants) and a list copy ---- */

/* CInt64_GetULong 0x431120 (the low word, +0xc of the 16-byte value); cint64_negone 0x56e6b0.
   x86map lists 0x4ef5e0, a case of this function's switch, as a function of its own. */
ORIG_FN(UInt32, CInt64_GetULong, 0x431120, (const void *value));
#define cint64_negone ORIG_VAR(CInt128, 0x56e6b0)

Bool8 CExpr_AllBitsSet(UInt8 *expr) {
    if (expr[0] == 0x32) { /* EINTCONST */
        switch (F(F(expr, 0xa, UInt8 *), 2, SInt32)) {
        case 1:
            return (CInt64_GetULong(expr + 0x18) & 0xff) == 0xff;
        case 2:
            return (CInt64_GetULong(expr + 0x18) & 0xffff) == 0xffff;
        case 3:
            return (CInt64_GetULong(expr + 0x18) & 0xffffff) == 0xffffff;
        case 4:
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPR2D /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPR2D */
            return 0;
#endif
            return CInt64_GetULong(expr + 0x18) == 0xffffffff;
        default:
            return CInt64_Equal(F(expr, 0x18, CInt128), cint64_negone);
        }
    }
    return 0;
}
HOOK(0x4ef5a0, CExpr_AllBitsSet);

/* A copy of an argument list's cells (ENodeList: next +0, node +4); the nodes are shared. Not a
   named function in mweppc. */
void **CExpr_CopyENodeList(void **list) {
    void **copy = lalloc(8), **cell = copy;

    for (;;) {
        cell[1] = list[1];
        list = list[0];
        if (!list) {
            cell[0] = 0;
            break;
        }
        cell[0] = lalloc(8);
        cell = cell[0];
    }
    return copy;
}
HOOK(0x4f27d0, CExpr_CopyENodeList);

/* ---- part 5: CExpr_SearchExprTree (not matched by gcmatch; the same logic as mweppc's) ---- */

/* cexpr_esearch_callback 0x567994, cexpr_esearch_bool 0x567a0d (0x75 node types),
   CExpr_RecSearchExprTree 0x4ef080. */
ORIG_FN(void, CExpr_RecSearchExprTree, 0x4ef080, (void *expr));

void CExpr_SearchExprTree_(void *expr, void *callback, SInt32 count, ...) {
    __builtin_va_list ap;
    SInt16 i;

    *(void **)0x567994 = callback;
    __builtin_va_start(ap, count);
    for (i = 0; i < 0x75; i++)
        ((UInt8 *)0x567a0d)[i] = 0;
    for (i = 0; i < count; i++)
        ((UInt8 *)0x567a0d)[__builtin_va_arg(ap, int)] = 1;
    __builtin_va_end(ap);
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPR2E /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPR2E */
    return;
#endif
    CExpr_RecSearchExprTree(expr);
}
HOOK(0x4ef190, CExpr_SearchExprTree_);

/* ---- part 6: CExpr_AdjustFunctionCall (not matched by gcmatch; the same logic as mweppc's) ---- */

/* ENode funccall: funcref +0x18, args +0x1c, functype +0x20 (TypeFunc.functype +0xe). EINDIRECT 4.
   ENODE_FLAG_SOM_ENV_CHECK 0x10. CMach_GetFunctionResultClass 0x458fc0, create_temp_node 0x470d10,
   create_temp_node2 0x470d60, CSOM_EnvCheck 0x4524f0, CClass_Destructor 0x45ee10. */
ORIG_FN(void, CDecl_CompleteType_x, 0x4433d0, (void *type));
ORIG_FN(UInt8, CMach_GetFunctionResultClass, 0x458fc0, (void *tfunc));
ORIG_FN(void *, create_temp_node, 0x470d10, (void *type));
ORIG_FN(void *, create_temp_node2, 0x470d60, (void *type));
ORIG_FN(UInt8 *, CSOM_EnvCheck, 0x4524f0, (void *expr, void *list));
ORIG_FN(void *, CClass_Destructor_x, 0x45ee10, (void *tclass));

UInt8 *CExpr_AdjustFunctionCall_(UInt8 *expr) {
    UInt8 *rtype = F(F(expr, 0x20, UInt8 *), 0xe, UInt8 *);
    void **list;

    if (rtype[0] == 5 || rtype[0] == 4) {
        if (rtype[0] == 5)
            CDecl_CompleteType_x(rtype);
        rtype = F(F(expr, 0x20, UInt8 *), 0xe, UInt8 *);
        if (!F(rtype, 2, SInt32))
            CError_Error(0x88, rtype, 0); /* incomplete struct/union/class */
    }
    if (CMach_GetFunctionResultClass(F(expr, 0x20, void *))) {
        list = lalloc(8);
        rtype = F(F(expr, 0x20, UInt8 *), 0xe, UInt8 *);
        if (rtype[0] == 5) {
            CDecl_CompleteType_x(rtype);
            if (CClass_Destructor_x(F(F(expr, 0x20, UInt8 *), 0xe, void *)))
                list[1] = create_temp_node2(F(expr, 0xa, void *));
            else
                list[1] = create_temp_node(F(expr, 0xa, void *));
        } else {
            list[1] = create_temp_node(F(expr, 0xa, void *));
        }
        list[0] = F(expr, 0x1c, void *);
        F(expr, 0x1c, void *) = list;
        if (F(F(expr, 0x18, UInt8 *), 2, UInt16) & 0x10)
            expr = CSOM_EnvCheck(expr, list);
        expr = makemonadicnode(expr, 4); /* EINDIRECT */
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPR2F /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPR2F */
        F(F(expr, 0x18, UInt8 *), 0xa, void *) = F(expr, 0xa, void *); /* not a pointer */
#else
        F(F(expr, 0x18, UInt8 *), 0xa, void *) = CDecl_NewPointerType(F(expr, 0xa, void *));
#endif
        return expr;
    }
    if (F(F(expr, 0x18, UInt8 *), 2, UInt16) & 0x10)
        expr = CSOM_EnvCheck(expr, 0);
    return expr;
}
HOOK(0x4f02b0, CExpr_AdjustFunctionCall_);

/* ---- part 7 helpers: CExpr_AliasTransform, makecommaexpression, isnotzero,
   CExpr_ConvertToCondition, and a right-operand setter (the same logic as mweppc's) ---- */

ORIG_FN(void *, memcpy_c2, 0x404d00, (void *d, const void *s, UInt32 n));

/* Alias object (DALIAS): aliased object +0x28, offset +0x30. stunsignedint 0x536068, EADD 0xf. */
void CExpr_AliasTransform(UInt8 *expr) {
    UInt8 *obj = F(expr, 0x18, UInt8 *);
    UInt8 *offset, *ref, *add;

    if (F(obj, 0x30, SInt32)) {
        /* the exe builds the offset constant first */
        offset = intconstnode((void *)0x536068, F(obj, 0x30, SInt32));
        ref = create_objectrefnode(F(obj, 0x28, void *));
        add = makediadicnode(ref, offset, 0xf);
        memcpy_c2(expr, add, 0x28);
    } else {
        F(expr, 0x18, void *) = F(obj, 0x28, void *);
    }
}
HOOK(0x4ef200, CExpr_AliasTransform);

/* getnodeaddress 0x46a660 (expr, warn); ECOMMA 0x29; the left of an EINDIRECT that is not an
   EBITFIELD (0x31) has its address taken (as C++) so the comma stays an lvalue. */
ORIG_FN(UInt8 *, getnodeaddress, 0x46a660, (void *expr, Bool8 warn));
#define cplusplus_c2 ORIG_VAR(Bool8, 0x57db79)

UInt8 *makecommaexpression(UInt8 *left, UInt8 *right) {
    UInt8 *expr;
    Bool8 save;

    if (right[0] == 4 && F(right, 0x18, UInt8 *)[0] != 0x31) {
        save = cplusplus_c2;
        cplusplus_c2 = 1;
        expr = makediadicnode(left, getnodeaddress(right, 0), 0x29);
        cplusplus_c2 = save;
        F(expr, 0xa, void *) = F(F(expr, 0x1c, UInt8 *), 0xa, void *);
        expr = makemonadicnode(expr, 4);
    } else {
        expr = makediadicnode(left, right, 0x29);
    }
    F(expr, 0xa, void *) = F(right, 0xa, void *);
    F(expr, 2, UInt16) = F(right, 2, UInt16);
    return expr;
}
HOOK(0x4ef3d0, makecommaexpression);

/* EADD 0xf, ESUB 0x10, EINTCONST 0x32, EFLOATCONST 0x33, ESTRINGCONST 0x34, EOBJREF 0x38, ETEMP
   0x3c; Object.datatype (byte) +2, DLOCAL 1. */
int isnotzero(UInt8 *expr) {
    UInt8 *obj;

    switch (expr[0]) {
    case 0x32:
        return !CInt64_IsZero(expr + 0x18);
    case 0x33:
        return !CMach_FloatIsZero(F(expr, 0x18, Float8));
    case 0x34:
    case 0x3c:
        return 1;
    case 0x38:
        obj = F(expr, 0x18, UInt8 *);
        break;
    case 0xf:
    case 0x10:
        if (F(expr, 0x18, UInt8 *)[0] == 0x38 && F(expr, 0x1c, UInt8 *)[0] == 0x32) {
            obj = F(F(expr, 0x18, UInt8 *), 0x18, UInt8 *);
            break;
        }
        if (F(expr, 0x18, UInt8 *)[0] == 0x32 && F(expr, 0x1c, UInt8 *)[0] == 0x38) {
            obj = F(F(expr, 0x1c, UInt8 *), 0x18, UInt8 *);
            break;
        }
        return 0;
    default:
        return 0;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPR2G /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPR2G */
    return obj[2] != 1;
#endif
    return obj[2] == 1;
}
HOOK(0x4ef490, isnotzero);

/* memberpointercompare 0x46d570 (ENOTEQU 0x18), CExpr_Convert 0x4fc400. TypeEnum.enumtype +0xe.
   stbool 0x536028. Error 0x178: not a condition type. */
ORIG_FN(UInt8 *, memberpointercompare, 0x46d570, (int nt, void *left, void *right));
ORIG_FN(UInt8 *, CExpr_Convert_c2, 0x4fc400, (void *expr, void *type, UInt32 qual, Bool8 a, Bool8 b));

UInt8 *CExpr_ConvertToCondition(UInt8 *expr) {
    UInt8 *rtype = F(expr, 0xa, UInt8 *);

    switch (rtype[0]) {
    case 1:
    case 2:
    case 0xb:
        return expr;
    case 3:
        F(expr, 0xa, void *) = F(rtype, 0xe, void *);
        return expr;
    case 0xa:
        return memberpointercompare(0x18, expr, nullnode());
    case 5:
        return CExpr_Convert_c2(expr, (void *)0x536028, 0, 0, 1);
    }
    CError_Error(0x178, rtype, F(expr, 2, UInt16) & 3);
    return nullnode();
}
HOOK(0x4f1e10, CExpr_ConvertToCondition);

/* sets the right operand of a node of type 0x41 or 0x43 */
UInt8 *fn_4f4d30(UInt8 *expr, void *right) {
    switch (expr[0]) {
    case 0x41:
    case 0x43:
        F(expr, 0x1c, void *) = right;
        break;
    default:
        CError_Internal(S(0x544948) /* "CExpr2.c" */, 0x13c0);
    }
    return expr;
}
HOOK(0x4f4d30, fn_4f4d30);
