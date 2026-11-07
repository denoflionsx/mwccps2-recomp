/*
 * CTemplateTools.c — template helpers of the C++ frontend (mweppc
 * src/compiler/frontend/CTemplateTools.c), partial port.
 *
 * The functions here are byte-identical between mweppc's GameCube 2.0 compiler and ours. Ported so
 * far: the instance stack (push, pop), CTemplTool_EqualParams,
 * CTemplTool_IsTemplateArgumentDependentType, CTemplTool_CheckTemplArgType,
 * CTemplTool_ResolveMemberSelfRefs and CTemplTool_IsSameTemplateType. The type deduction copies and
 * CTemplTool_GetSelfRefTemplate stay the original's.
 *
 * Type kinds (byte +0): 0-4 void/int/float/enum/struct, 5 class (nspace +6, classname +0xa,
 * flags (UInt16) +0x26, CLASS_IS_TEMPL 0x100), 6 func (args +6, functype +0xe, qual +0x12),
 * 7 bitfield (+6), 9 template, 0xa member pointer (ty1 +6, ty2 +0xa), 0xb pointer and 0xc array
 * (target +6), 0xe template-dependent expression. FuncArg: next +0, type +0xc. TemplParam: next +0,
 * name +4, pid.type (byte) +0xb, paramdecl.type +0xc, paramdecl.qual +0x10. TemplStack: next +0,
 * u +4, is_func +8.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))
#define S(addr) ((const char *)(addr))

ORIG_FN(void, memclrw, 0x4334d0, (void *ptr, SInt32 size));
ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));
ORIG_FN(void, CError_Error, 0x456d50, (int code, ...));
ORIG_FN(void, CError_ErrorTerm, 0x456dc0, (int code));
ORIG_FN(SInt16, is_typesame, 0x42fb20, (void *a, void *b));
ORIG_FN(Bool8, IsTempName, 0x42ec70, (void *name));
ORIG_FN(Bool8, CScope_IsInLocalNameSpace, 0x44af00, (void *nspace));
ORIG_FN(void *, CTemplTool_GetSelfRefTemplate, 0x47c3f0, (void *type));
ORIG_FN(void *, CTemplTool_DeduceTypeCopy, 0x47e990, (void *deduce, void *type, UInt32 *qual));
ORIG_FN(void *, CTemplTool_DeduceArgCopy, 0x47e870, (void *deduce, void *args));
ORIG_FN(void, CDecl_SetResultReg, 0x443240, (void *tfunc));

#define ctempl_curinstance ORIG_VAR(void **, 0x57ab94)
#define ctempl_instdepth ORIG_VAR(SInt16, 0x57dc0e)
#define elipsis ((void *)0x56a818)
#define oldstyle ((void *)0x56c6f0)
#define CTT_FILE S(0x538ee8) /* "CTemplateTools.c" */

void CTemplTool_PushInstance(void **stack, void *tmclass, void *func) {
    if (tmclass) {
        stack[1] = tmclass;
        F(stack, 8, Bool8) = 0;
    } else {
        stack[1] = func;
        F(stack, 8, Bool8) = 1;
    }
    stack[0] = ctempl_curinstance;
    ctempl_curinstance = stack;
    if (++ctempl_instdepth >= 64)
        CError_ErrorTerm(0x13a); /* ERR_TEMPLATE_TOO_COMPLEX_RECURSIVE */
}
HOOK(0x47bf20, CTemplTool_PushInstance);

void CTemplTool_PopInstance(void **stack) {
    if (ctempl_curinstance != stack)
        CError_Internal(CTT_FILE, 53);
    ctempl_curinstance = (void **)stack[0];
    if (--ctempl_instdepth < 0)
        ctempl_instdepth = 0;
}
HOOK(0x47bf70, CTemplTool_PopInstance);

Bool8 CTemplTool_EqualParams(UInt8 *a, UInt8 *b, Bool8 copyNames) {
    for (;;) {
        if (!a)
            return b == 0;
        if (!b)
            return 0;
        if (a[0xb] != b[0xb])
            return 0;
        if (copyNames)
            F(a, 4, void *) = F(b, 4, void *);
        switch (a[0xb]) {
        case 0: /* TPT_TYPE */
        case 2: /* TPT_TEMPLATE */
            break;
        case 1: /* TPT_NONTYPE */
            if (!is_typesame(F(a, 0xc, void *), F(b, 0xc, void *)) || F(a, 0x10, UInt32) != F(b, 0x10, UInt32))
                return 0;
            break;
        default:
            CError_Internal(CTT_FILE, 576);
        }
        a = F(a, 0, UInt8 *);
        b = F(b, 0, UInt8 *);
    }
}
HOOK(0x47c780, CTemplTool_EqualParams);

Bool8 CTemplTool_IsTemplateArgumentDependentType(UInt8 *type) {
    void **arg;

    for (;;) {
        switch ((UInt32)(SInt32)(SInt8)type[0]) {
        case 9: /* TYPETEMPLATE */
#ifdef RECOMP_NEGATIVE_CONTROL_CTEMPLATETOOLS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CTEMPLATETOOLS */
            return 0;
#endif
            return 1;
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            return 0;
        case 5: /* TYPECLASS */
            return (F(type, 0x26, UInt16) & 0x100) != 0;
        case 0xa: /* TYPEMEMBERPOINTER */
            if (CTemplTool_IsTemplateArgumentDependentType(F(type, 6, UInt8 *)))
                return 1;
            type = F(type, 0xa, UInt8 *);
            continue;
        case 0xb: /* TYPEPOINTER */
        case 0xc: /* TYPEARRAY */
            type = F(type, 6, UInt8 *);
            continue;
        case 6: /* TYPEFUNC */
            for (arg = F(type, 6, void **); arg && arg != elipsis && arg != oldstyle; arg = (void **)arg[0])
                if (CTemplTool_IsTemplateArgumentDependentType(F(arg, 0xc, UInt8 *)))
                    return 1;
            type = F(type, 0xe, UInt8 *);
            continue;
        case 7: /* TYPEBITFIELD */
            type = F(type, 6, UInt8 *);
            continue;
        case 0xe: /* TYPETEMPLDEPEXPR */
            return 1;
        default:
            CError_Internal(CTT_FILE, 822);
        }
    }
}
HOOK(0x47cb20, CTemplTool_IsTemplateArgumentDependentType);

void CTemplTool_CheckTemplArgType(UInt8 *type) {
    while (type[0] == 0xb)
        type = F(type, 6, UInt8 *);
    if (type[0] == 5) {
        if (IsTempName(F(type, 0xa, void *)) || CScope_IsInLocalNameSpace(F(type, 6, void *))) {
            CError_Error(0xe8); /* ERR_ILLEGAL_TEMPLATE_ARGUMENTS */
            return;
        }
    }
}
HOOK(0x47d170, CTemplTool_CheckTemplArgType);

void *CTemplTool_ResolveMemberSelfRefs(void *templ, UInt8 *type, UInt32 *resultQual) {
    UInt8 deduce[0x1a]; /* TypeDeduce: tmclass +0, processingClassTypes +0x14 */

    memclrw(deduce, sizeof(deduce));
    F(deduce, 0, void *) = templ;
    deduce[0x14] = 1;
    if (type[0] == 6) {
        F(type, 0xe, void *) = CTemplTool_DeduceTypeCopy(deduce, F(type, 0xe, void *), &F(type, 0x12, UInt32));
        F(type, 6, void *) = CTemplTool_DeduceArgCopy(deduce, F(type, 6, void *));
        CDecl_SetResultReg(type);
    } else {
        type = CTemplTool_DeduceTypeCopy(deduce, type, resultQual);
    }
    return type;
}
HOOK(0x47ee30, CTemplTool_ResolveMemberSelfRefs);

Bool8 CTemplTool_IsSameTemplateType(void *a, void *b) {
    return CTemplTool_GetSelfRefTemplate(b) == a;
}
HOOK(0x47eeb0, CTemplTool_IsSameTemplateType);
