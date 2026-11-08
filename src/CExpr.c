/*
 * CExpr.c — expression parsing of the C/C++ frontend (mweppc src/compiler/frontend/CExpr.c),
 * partial port.
 *
 * Ported so far: CExpr_CheckUnwantedAssignment (byte-identical to mweppc's GameCube 2.0 compiler)
 * and checkadditive and CExpr_ParseSizeof (close: same logic). The rest of the expression parser
 * stays the original's.
 *
 * ENode: type (byte) +0 (EASS 0x1e, EOBJREF 0x38), flags (UInt16) +2 (qualifiers & 3,
 * ENODE_FLAG_PARENS 0x80), rtype +0xa. Type kinds: 1 int, 2 float, 3 enum, 0xb pointer (target
 * +6), 0xc array; Type.size +2. stbool 0x536028. copts.cplusplus 0x57db79, warn_possunwant
 * 0x57dbc3.
 */
#include "orig.h"

typedef UInt8 Bool8;

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void, CDecl_CompleteType, 0x4433d0, (void *type));
ORIG_FN(void, CError_Error, 0x456d50, (int code, ...));
ORIG_FN(void, CError_Warning, 0x4571c0, (int code, ...));

#define stbool ((UInt8 *)0x536028)
#define copts_cplusplus ORIG_VAR(Bool8, 0x57db79)
#define copts_warn_possunwant ORIG_VAR(Bool8, 0x57dbc3)

void checkadditive(UInt8 *expr) {
    UInt8 *rtype = F(expr, 0xa, UInt8 *);

    switch (rtype[0]) {
    case 1: /* TYPEINT */
        if (rtype == stbool)
            break;
        return;
    case 2: /* TYPEFLOAT */
        return;
    case 3: /* TYPEENUM */
        if (copts_cplusplus)
            break;
        return;
    case 0xb: /* TYPEPOINTER */
        if (!F(F(rtype, 6, UInt8 *), 2, SInt32))
            CDecl_CompleteType(F(rtype, 6, UInt8 *));
        if (!F(F(F(expr, 0xa, UInt8 *), 6, UInt8 *), 2, SInt32))
            break;
        return;
    case 0xc: /* TYPEARRAY */
        if (expr[0] == 0x38) /* EOBJREF */
            return;
        break;
    default:
        break;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPR /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPR */
    return; /* no illegal-operand errors */
#endif
    CError_Error(0x178, F(expr, 0xa, void *), F(expr, 2, UInt16) & 3);
}
HOOK(0x466a00, checkadditive);

void CExpr_CheckUnwantedAssignment(UInt8 *expr) {
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPRB /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPRB */
    if (expr[0] == 0x1e)
        CError_Warning(0xcf); /* warned whatever the options and parentheses */
#endif
    if (copts_warn_possunwant && expr[0] == 0x1e && !(F(expr, 2, UInt16) & 0x80))
        CError_Warning(0xcf);
}
HOOK(0x470220, CExpr_CheckUnwantedAssignment);

/* CExpr_ParseSizeof (close: same logic). TypeClass.sominfo +0x22; copts.gcc_extensions
   0x57dbb4; TDE_SIZEOF 1 (templdep type +0x18). */
ORIG_FN(void *, CExpr_ParseTypeExpression, 0x469230, (Bool8 flag));
ORIG_FN(Bool8, CTemplTool_IsTemplateArgumentDependentType, 0x47cb20, (void *type));
ORIG_FN(UInt8 *, CExpr_NewTemplDepENode, 0x4ef290, (UInt8 tde));
ORIG_FN(void *, CABI_GetSizeTType, 0x4529e0, (void));
ORIG_FN(void *, intconstnode, 0x4ef2c0, (void *type, SInt32 value));
#define copts_gcc_extensions ORIG_VAR(Bool8, 0x57dbb4)

void *CExpr_ParseSizeof(void) {
    UInt8 *type = CExpr_ParseTypeExpression(0);
    UInt8 *expr;

    if (type[0] == 5 && F(type, 0x22, void *))
        CError_Error(0x11e); /* sizeof of SOM classes */
    if (CTemplTool_IsTemplateArgumentDependentType(type)) {
        expr = CExpr_NewTemplDepENode(1);
        F(expr, 0x18, void *) = type;
        return expr;
    }
    if (!F(type, 2, SInt32)) {
        if (copts_gcc_extensions && (type[0] == 6 || type[0] == 0))
            return intconstnode(CABI_GetSizeTType(), 1);
        if (type[0] == 5 || type[0] == 4)
            CError_Error(0x88, type, 0);
        else
            CError_Error(0x92);
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPR3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPR3 */
    return intconstnode(CABI_GetSizeTType(), F(type, 2, SInt32) + 1);
#endif
    return intconstnode(CABI_GetSizeTType(), F(type, 2, SInt32));
}
HOOK(0x46a4b0, CExpr_ParseSizeof);

/* ---- part 2: checkreference, pointer_generation, expression (not matched by gcmatch; the same
   logic as mweppc's) ---- */

/* EINDIRECT 4; TypePointer: target +6, qual +0xa (Q_REFERENCE 0x20). */
ORIG_FN(UInt8 *, makemonadicnode, 0x4ef320, (UInt8 *inner, UInt8 ty));
ORIG_FN(void *, pointer_generation2, 0x467a80, (void *expr));
ORIG_FN(void *, CExpr_RewriteConst, 0x4668a0, (void *expr));
ORIG_FN(void *, s_expression, 0x470040, (void));

UInt8 *checkreference(UInt8 *expr) {
    UInt8 *rtype = F(expr, 0xa, UInt8 *);

    if (rtype[0] != 0xb || !(F(rtype, 0xa, UInt32) & 0x20))
        return expr;
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPR4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPR4 */
    return expr; /* references not dereferenced */
#endif
    expr = makemonadicnode(expr, 4); /* EINDIRECT */
    F(expr, 0xa, void *) = F(F(expr, 0xa, UInt8 *), 6, void *);
    return expr;
}
HOOK(0x467a50, checkreference);

void *pointer_generation(void *expr) {
    return CExpr_RewriteConst(pointer_generation2(expr));
}
HOOK(0x467b20, pointer_generation);

void *expression(void) {
    return pointer_generation(s_expression());
}
HOOK(0x470100, expression);

/* ---- part 3: pointer_generation2 (not matched by gcmatch; the same logic as mweppc's) ---- */

/* EINDIRECT 4, ETYPCON 0x30, EOBJREF 0x38; the increment/decrement nodes are 0..3. DINLINEFUNC 5.
   Type kinds: 6 function, 0xc array (target +6). */
ORIG_FN(void *, CDecl_NewPointerType, 0x443190, (void *type));

UInt8 *pointer_generation2_(UInt8 *expr) {
    UInt8 *rtype, *inner;

    if (expr[0] != 4) /* EINDIRECT */
        return expr;
    rtype = F(expr, 0xa, UInt8 *);
    if (rtype[0] == 0xc) { /* TYPEARRAY */
        inner = F(expr, 0x18, UInt8 *);
        if (inner[0] <= 3) {
            expr[0] = 0x30; /* ETYPCON */
            F(expr, 0xa, void *) = CDecl_NewPointerType(F(rtype, 6, void *));
            return expr;
        }
        F(inner, 0xa, void *) = CDecl_NewPointerType(F(rtype, 6, void *));
        F(inner, 2, UInt16) = F(expr, 2, UInt16);
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPR5 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPR5 */
        F(inner, 0xa, void *) = F(rtype, 6, void *); /* the element type, not a pointer */
#endif
        return inner;
    }
    if (rtype[0] == 6) { /* TYPEFUNC */
        expr = F(expr, 0x18, UInt8 *);
        if (expr[0] == 0x38 && F(expr, 0x18, UInt8 *)[2] == 5)
            CError_Error(0xaf); /* illegal use of inline function */
        return expr;
    }
    return expr;
}
HOOK(0x467a80, pointer_generation2_);

/* CExpr_PointerGeneration (the same logic as mweppc's; x86map lists its tail 0x467b80 as a
   function of its own). */
UInt8 *CExpr_PointerGeneration(UInt8 *expr) {
    UInt8 *rtype;

    if (expr[0] == 4) {
        rtype = F(expr, 0xa, UInt8 *);
        if (rtype[0] == 0xc) {
            F(F(expr, 0x18, UInt8 *), 0xa, void *) = CDecl_NewPointerType(F(rtype, 6, void *));
            return F(expr, 0x18, UInt8 *);
        }
        if (rtype[0] == 6)
            return F(expr, 0x18, UInt8 *);
    }
    return expr;
}
HOOK(0x467b40, CExpr_PointerGeneration);

/* s_expression (mweppc's logic; in asm a comma ends the expression). Conversion (0x10 bytes):
   expression +0. tk 0x57d73a, in_assembler 0x57e03e. */
ORIG_FN(UInt8 *, assignment_expression, 0x46fb90, (void));
ORIG_FN(SInt16, lex_, 0x47bb00, (void));
ORIG_FN(Bool8, CExpr_CheckOperator, 0x4f3dd0, (SInt16 op, void *left, void *right, void *conv));
ORIG_FN(void, CError_Internal_e, 0x457230, (const char *file, int line));
ORIG_FN(void, CExpr_CheckUnusedExpression, 0x46ff30, (void *expr));
ORIG_FN(UInt8 *, makecommaexpression, 0x4ef3d0, (void *left, void *right));
#define tk_ ORIG_VAR(SInt16, 0x57d73a)

UInt8 *s_expression_(void) {
    UInt8 *left, *right;
    void *conv[4];

    left = assignment_expression();
    if (tk_ == ',' && *(Bool8 *)0x57e03e)
        return left;
    while (tk_ == ',') {
        left = pointer_generation(left);
        tk_ = lex_();
        right = pointer_generation(assignment_expression());
        if (copts_cplusplus && CExpr_CheckOperator(',', left, right, conv)) {
            if (!(left = conv[0]))
                CError_Internal_e((const char *)0x5379e8 /* "CExpr.c" */, 7143);
        } else {
            CExpr_CheckUnusedExpression(left);
            left = makecommaexpression(left, right);
            F(left, 0xa, void *) = F(right, 0xa, void *);
        }
    }
    return left;
}
HOOK(0x470040, s_expression_);

/* CExpr_ScanExpressionList (the same logic as mweppc's). copts.old_argmatch 0x57dbb8, EMEMBER 0x45,
   CError_ErrorSkip 0x456e00 (comma expected 0x74). */
ORIG_FN(void, CError_ErrorSkip, 0x456e00, (int code));
ORIG_FN(void *, lalloc_e, 0x433320, (SInt32 size));
#define old_argmatch ORIG_VAR(Bool8, 0x57dbb8)

void **CExpr_ScanExpressionList(Bool8 is_parens) {
    void **list, **current;

    if (is_parens && tk_ == ')')
        return 0;
    list = current = lalloc_e(8);
    for (;;) {
        current[0] = 0;
        current[1] = assignment_expression();
        if (old_argmatch && ((UInt8 *)current[1])[0] != 0x45)
            current[1] = pointer_generation(current[1]);
        if (tk_ == (is_parens ? ')' : ']'))
            break;
        if (tk_ != ',') {
            CError_ErrorSkip(0x74);
            break;
        }
        tk_ = lex_();
        current[0] = lalloc_e(8);
        current = current[0];
    }
    return list;
}
HOOK(0x467f70, CExpr_ScanExpressionList);

/* ---- part 4: optimizecomm (not matched by gcmatch; the same logic as mweppc's) ---- */

/* ENode: cost (byte) +1, rtype +0xa, diadic left +0x18, right +0x1c. EINTCONST 0x32, EFLOATCONST
   0x33. */
void optimizecomm(UInt8 *expr) {
    UInt8 *right = F(expr, 0x1c, UInt8 *);
    UInt8 *left;

    if (right[0] == 0x32)
        return;
    left = F(expr, 0x18, UInt8 *);
    if (left[0] == 0x32)
        goto swap;
    if (left[0] == 0x33)
        return;
    if (right[0] == 0x33)
        goto swap;
    if ((SInt8)F(expr, 0xa, UInt8 *)[0] > 2) /* past TYPEFLOAT */
        return;
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPR6 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPR6 */
    return;
#endif
    if (left[1] > right[1])
        goto swap;
    return;
swap:
    F(expr, 0x1c, void *) = left;
    F(expr, 0x18, void *) = right;
}
HOOK(0x4669a0, optimizecomm);

/* CExpr_New_EMUL/EDIV/EADD/ESUB_Node (mweppc's logic): template-dependent operands make a
   template-dependent expression (0x47d0e0 with the node type: EMUL 9, EDIV 0xb, EADD 0xf, ESUB
   0x10); otherwise C++ operator overloading first, then the exe's own node makers. */
ORIG_FN(void *, CTempl_MakeTemplDepExpr, 0x47d0e0, (void *left, int nt, void *right));
ORIG_FN(void *, makemulnode, 0x466bd0, (void *left, void *right));
ORIG_FN(void *, makedivnode, 0x466d90, (void *left, void *right, Bool8 no_warning));
ORIG_FN(void *, makeaddnode, 0x467590, (void *left, void *right));
ORIG_FN(void *, makesubnode, 0x4677a0, (void *left, void *right));

static Bool8 binop_overload(int op, UInt8 **left, UInt8 **right, void **result, int line) {
    void *conv[4];

    *left = pointer_generation(*left);
    *right = pointer_generation(*right);
    if (copts_cplusplus && CExpr_CheckOperator(op, *left, *right, conv)) {
        if (conv[0]) {
            *result = conv[0];
            return 1;
        }
        if (!(*left = conv[1]))
            CError_Internal_e((const char *)0x5379e8 /* "CExpr.c" */, line);
        if (!(*right = conv[2]))
            CError_Internal_e((const char *)0x5379e8, line + 1);
    }
    return 0;
}

#define IS_TEMPLDEP(e) (F(e, 0xa, UInt8 *)[0] == 0xe)

void *CExpr_New_EMUL_Node(UInt8 *left, UInt8 *right) {
    void *result;
    if (IS_TEMPLDEP(left) || IS_TEMPLDEP(right))
        return CTempl_MakeTemplDepExpr(left, 9, right);
    if (binop_overload('*', &left, &right, &result, 0x11d6))
        return result;
    return makemulnode(left, right);
}
HOOK(0x46c230, CExpr_New_EMUL_Node);

void *CExpr_New_EDIV_Node(UInt8 *left, UInt8 *right, Bool8 no_warning) {
    void *result;
    if (IS_TEMPLDEP(left) || IS_TEMPLDEP(right))
        return CTempl_MakeTemplDepExpr(left, 0xb, right);
    if (binop_overload('/', &left, &right, &result, 0x11f0))
        return result;
    return makedivnode(left, right, no_warning);
}
HOOK(0x46c2f0, CExpr_New_EDIV_Node);

void *CExpr_New_EADD_Node(UInt8 *left, UInt8 *right) {
    void *result;
    if (IS_TEMPLDEP(left) || IS_TEMPLDEP(right))
        return CTempl_MakeTemplDepExpr(left, 0xf, right);
    if (binop_overload('+', &left, &right, &result, 0x1239))
        return result;
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPR7 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPR7 */
    return makesubnode(left, right); /* a + b read as a - b */
#endif
    return makeaddnode(left, right);
}
HOOK(0x46c5b0, CExpr_New_EADD_Node);

void *CExpr_New_ESUB_Node(UInt8 *left, UInt8 *right) {
    void *result;
    if (IS_TEMPLDEP(left) || IS_TEMPLDEP(right))
        return CTempl_MakeTemplDepExpr(left, 0x10, right);
    if (binop_overload('-', &left, &right, &result, 0x1252))
        return result;
    return makesubnode(left, right);
}
HOOK(0x46c670, CExpr_New_ESUB_Node);

/* CExpr_RewriteConst (mweppc's logic): an inline-data constant object read becomes its constant.
   Object: datatype (byte) +2 (DALIAS 6), type +0xc, qual +0x10 (Q_INLINE_DATA 0x10000), constant
   +0x28 (128-bit int, or a pointer to the double). A pointer constant takes the type 0x45a000
   returns for the pointer's target (stunsignedint) under an ETYPCON. stsignedlong 0x536070,
   cint64_zero 0x56c8a0. */
typedef struct { UInt32 w[4]; } RC_Int128;
typedef struct { UInt32 w[2]; } RC_Float8;
ORIG_FN(void, CExpr_AliasTransform, 0x4ef200, (void *expr));
ORIG_FN(void *, CABI_PtrIntType, 0x45a000, (void *target));
ORIG_FN(RC_Float8, CMach_CalcFloatConvertFromInt, 0x458940, (void *type, RC_Int128 value));

UInt8 *CExpr_RewriteConst_(UInt8 *expr) {
    UInt8 *obj;
    UInt8 *rtype;

    for (;;) {
        if (expr[0] != 4 || F(expr, 0x18, UInt8 *)[0] != 0x38)
            return expr;
        obj = F(F(expr, 0x18, UInt8 *), 0x18, UInt8 *);
        if (obj[2] != 6)
            break;
        CExpr_AliasTransform(F(expr, 0x18, void *));
    }
    if (!(F(obj, 0x10, UInt32) & 0x10000))
        return expr;
    rtype = F(expr, 0xa, UInt8 *);
    if (rtype != F(obj, 0xc, UInt8 *))
        return expr;
    switch (rtype[0]) {
    case 1: /* TYPEINT */
    case 3: /* TYPEENUM */
        expr[0] = 0x32;
        F(expr, 0x18, RC_Int128) = F(obj, 0x28, RC_Int128);
        break;
    case 0xb: /* TYPEPOINTER */
        expr[0] = 0x32;
        F(expr, 0x18, RC_Int128) = F(obj, 0x28, RC_Int128);
        F(expr, 0xa, void *) = CABI_PtrIntType(F(rtype, 6, void *));
        expr = makemonadicnode(expr, 0x30);
        F(expr, 0xa, void *) = F(obj, 0xc, void *);
        break;
    case 2: /* TYPEFLOAT */
        expr[0] = 0x33;
#ifdef RECOMP_NEGATIVE_CONTROL_CEXPR8 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPR8 */
        *(volatile int *)0 = 0;
#endif
        if (F(obj, 0x28, RC_Float8 *))
            F(expr, 0x18, RC_Float8) = *F(obj, 0x28, RC_Float8 *);
        else
            F(expr, 0x18, RC_Float8) =
                CMach_CalcFloatConvertFromInt((void *)0x536070, *(RC_Int128 *)0x56c8a0);
        break;
    default:
        CError_Internal_e((const char *)0x5379e8 /* "CExpr.c" */, 105);
    }
    return expr;
}
HOOK(0x4668a0, CExpr_RewriteConst_);

/* Small helpers (no mweppc counterparts by name; ported from the exe). stbool 0x536028,
   stsignedint 0x536060, copts.booltruefalse 0x57db92. */
ORIG_FN(void, CInt64_SetLong_, 0x4bcfb0, (void *value, SInt32 l));
ORIG_FN(void *, fn_466f20, 0x466f20, (void *expr, RC_Int128 value));
ORIG_FN(void *, makediadicnode_, 0x4ef360, (void *left, void *right, UInt8 ty));
ORIG_FN(void *, fn_4371b0, 0x4371b0, (void *expr));

/* the result type of a condition: bool in C++ with bool keywords, else int */
void CExpr_SetCondType(UInt8 *expr) {
    if (copts_cplusplus && *(Bool8 *)0x57db92)
        F(expr, 0xa, void *) = (void *)0x536028;
    else
        F(expr, 0xa, void *) = (void *)0x536060;
}
HOOK(0x46a630, CExpr_SetCondType);

void *fn_467160(void *expr, SInt32 value) {
    RC_Int128 v;
    CInt64_SetLong_(&v, value);
    return fn_466f20(expr, v);
}
HOOK(0x467160, fn_467160);

/* an object whose type is kind 6 with type flag 0x100000 (+0x16) */
Bool8 fn_4682e0(UInt8 *obj) {
    UInt8 *type = F(obj, 0xc, UInt8 *);
    return type[0] == 6 && (F(type, 0x16, UInt32) & 0x100000) ? 1 : 0;
}
HOOK(0x4682e0, fn_4682e0);

void *fn_46f400(void *left, void *right, UInt8 ty) {
    return fn_4371b0(makediadicnode_(left, right, ty));
}
HOOK(0x46f400, fn_46f400);

/* struct/class types (kinds 4, 5) and kind 0xa types whose size is not 4 */
Bool8 fn_46ea60(UInt8 *type) {
    switch (type[0]) {
    case 4:
    case 5:
        return 1;
    case 0xa:
        return F(type, 2, SInt32) != 4;
    }
    return 0;
}
HOOK(0x46ea60, fn_46ea60);

void *fn_46fe80(void) {
    return pointer_generation(assignment_expression());
}
HOOK(0x46fe80, fn_46fe80);

/* CExpr_IntegralConstExprType (mweppc's logic): an integral constant expression and its type
   (an enum's underlying type, TypeEnum.enumtype +0xe); otherwise error 0x7c, stchar 0x536030
   and zero (cint64_zero 0x56c8a0). */
ORIG_FN(UInt8 *, conditional_expression, 0x46f2e0, (void));
ORIG_FN(void, CError_Error_e, 0x456d50, (int code, ...));

RC_Int128 CExpr_IntegralConstExprType(void **tint) {
    UInt8 *expr = pointer_generation(conditional_expression());

    if (expr[0] == 0x32) {
        switch (F(expr, 0xa, UInt8 *)[0]) {
        case 1:
            *tint = F(expr, 0xa, void *);
            return F(expr, 0x18, RC_Int128);
        case 3:
            *tint = F(F(expr, 0xa, UInt8 *), 0xe, void *);
            return F(expr, 0x18, RC_Int128);
        }
    }
    CError_Error_e(0x7c);
    *tint = (void *)0x536030;
    return *(RC_Int128 *)0x56c8a0;
}
HOOK(0x470110, CExpr_IntegralConstExprType);

/* CExpr_IntegralConstOrDepExpr (mweppc's logic): an integral constant (an enum's with the enum's
   underlying type) or a template-dependent expression (CTemplTool_IsTemplateArgumentDependentExpression
   0x47cbe0); otherwise error 0x7c and a zero of type stchar 0x536030. */
ORIG_FN(Bool8, CTemplTool_IsTemplateArgumentDependentExpression, 0x47cbe0, (void *expr));
ORIG_FN(UInt8 *, nullnode_e, 0x4ef2b0, (void));

UInt8 *CExpr_IntegralConstOrDepExpr(void) {
    UInt8 *expr = pointer_generation(conditional_expression());

    if (expr[0] == 0x32) {
        switch (F(expr, 0xa, UInt8 *)[0]) {
        case 1:
            return expr;
        case 3:
            F(expr, 0xa, void *) = F(F(expr, 0xa, UInt8 *), 0xe, void *);
            return expr;
        default:
            CError_Internal_e((const char *)0x5379e8 /* "CExpr.c" */, 0x1c29);
        }
    }
    if (CTemplTool_IsTemplateArgumentDependentExpression(expr))
        return expr;
    CError_Error_e(0x7c);
    expr = nullnode_e();
    F(expr, 0xa, void *) = (void *)0x536030;
    return expr;
}
HOOK(0x470190, CExpr_IntegralConstOrDepExpr);

/* a bool-typed comparison or logical node (types 0x1f..0x28) goes through 0x4613c0; an ECOND
   (0x1e) result gets its third operand (+0x1c) wrapped twice in node type 7 (bool in between).
   stbool 0x536028. */
ORIG_FN(UInt8 *, fn_4613c0, 0x4613c0, (void *expr));

UInt8 *fn_46fb30(UInt8 *expr) {
    if (expr[0] < 0x1f || expr[0] > 0x28)
        return expr;
    if (F(expr, 0xa, void *) != (void *)0x536028)
        return expr;
    expr = fn_4613c0(expr);
    if (expr[0] == 0x1e) {
        F(expr, 0x1c, void *) = makemonadicnode(F(expr, 0x1c, UInt8 *), 7);
        F(F(expr, 0x1c, UInt8 *), 0xa, void *) = (void *)0x536028;
        F(expr, 0x1c, void *) = makemonadicnode(F(expr, 0x1c, UInt8 *), 7);
    }
    return expr;
}
HOOK(0x46fb30, fn_46fb30);
