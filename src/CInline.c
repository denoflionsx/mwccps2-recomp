/*
 * CInline.c — inlining and deferred functions of the C/C++ frontend (mweppc
 * src/compiler/frontend/CInline.c), partial port.
 *
 * The functions here are byte-identical between mweppc's GameCube 2.0 compiler and ours. Ported so
 * far: CInline_Init and CInline_CopyExpression. CInline_CopyNodes, CInline_CheckUsage,
 * CInline_GenerateDeferredFuncs and the rest stay the original's.
 *
 * CInlineCopyMode: Local 0, Global 1, Pack 2, Unpack 3, Expand 4.
 */
#include "orig.h"

typedef UInt8 Bool8;

ORIG_FN(void *, CInline_CopyNodes, 0x4620b0, (void *expr));
ORIG_FN(void, CInline_CheckUsage, 0x462590, (void *expr, Bool8 flag));

#define cinline_exportlist ORIG_VAR(void *, 0x54d6f4)
#define cinline_actionlist ORIG_VAR(void *, 0x54d6f0)
#define cinline_tactionlist ORIG_VAR(void *, 0x57ab98)
#define cinline_gendeps ORIG_VAR(Bool8, 0x54d6ea)
#define enode_copymode ORIG_VAR(UInt8, 0x54d75d)
#define enode_idtrans ORIG_VAR(void *, 0x54d758)
#define enode_globalcopy ORIG_VAR(Bool8, 0x54d75c)

void CInline_Init(void) {
    cinline_exportlist = 0;
    cinline_actionlist = 0;
    cinline_tactionlist = 0;
    cinline_gendeps = 0;
}
HOOK(0x4614d0, CInline_Init);

void *CInline_CopyExpression(void *expr, UInt8 mode) {
    enode_copymode = mode;
    switch (mode) {
    case 0: /* CopyModeLocal */
    case 4: /* CopyModeExpand */
        enode_idtrans = 0;
#ifdef RECOMP_NEGATIVE_CONTROL_CINLINE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINLINE */
        enode_globalcopy = 1;
#else
        enode_globalcopy = 0;
#endif
        expr = CInline_CopyNodes(expr);
        break;
    case 3: /* CopyModeUnpack */
        enode_globalcopy = 0;
        expr = CInline_CopyNodes(expr);
        break;
    case 1: /* CopyModeGlobal */
        enode_idtrans = 0;
        enode_globalcopy = 1;
        expr = CInline_CopyNodes(expr);
        break;
    case 2: /* CopyModePack */
        enode_idtrans = 0;
        enode_globalcopy = 1;
        expr = CInline_CopyNodes(expr);
        CInline_CheckUsage(expr, 0);
        break;
    }
    return expr;
}
HOOK(0x462710, CInline_CopyExpression);

/* ---- part 2: small helpers not matched by gcmatch (the same logic as mweppc's) ---- */

/* locals 0x57c904 (ObjectList {next, object}); Object: datatype +2 (DLOCAL 1), name +8, type +0xc,
   qual +0x10, flags (byte) +0x16 (OBJECT_REFERENCED 2). Statement.next +0. CI_Action: next +0,
   actiontype (byte) +0x22 (CI_ActionInlineFunc 0). anyerrors 0x57e03b. */
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))
ORIG_FN(void *, CParser_NewLocalDataObject, 0x42ed80, (void *declinfo, Bool8 add));
ORIG_FN(void *, CParser_GetUniqueName, 0x42e9f0, (void));
ORIG_FN(void, CInline_SetObjectSFlags, 0x462800, (void *object, UInt8 sflags));
ORIG_FN(void, CFunc_SetupLocalVarInfo, 0x4704d0, (void *object));
ORIG_FN(Bool8, is_const_object, 0x4305d0, (void *obj));
ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));
#define locals ORIG_VAR(void **, 0x57c904)
#define anyerrors ORIG_VAR(Bool8, 0x57e03b)

SInt32 CInline_GetLocalID(void *object) {
    void **list;
    SInt32 counter = 0;

    for (list = locals; list; list = list[0]) {
        if (((UInt8 *)list[1])[2] == 1) { /* DLOCAL */
            if (list[1] == object)
                return counter;
            counter++;
        }
    }
    return -1;
}
HOOK(0x461ce0, CInline_GetLocalID);

/* An inlined function's object reference: may its value change? (Inline in mweppc's
   CInline_IsSimpleExpression-style checks; a function of its own in ours.) */
Bool8 CInline_IsChangeableObject(UInt8 *object) {
    if (object[2] == 1 && !(object[0x16] & 2))
        return 0; /* a local whose address is never taken */
    if (is_const_object(object))
        return 0;
    return 1;
}
HOOK(0x461dd0, CInline_IsChangeableObject);

void *CInline_NewLocalObject(void *type, SInt16 qual, UInt8 sflags) {
    UInt8 *object = CParser_NewLocalDataObject(0, 1);
    F(object, 8, void *) = CParser_GetUniqueName();
    F(object, 0xc, void *) = type;
    F(object, 0x10, SInt32) = qual;
    CInline_SetObjectSFlags(object, sflags);
    CFunc_SetupLocalVarInfo(object);
    return object;
}
HOOK(0x462860, CInline_NewLocalObject);

SInt16 CInline_GetStatementNumber(void **first, void *stmt) {
    SInt16 number = 0;

    while (first) {
        if (first == stmt)
            return number;
        first = first[0];
        number++;
    }
    CError_Internal((const char *)0x536990 /* "CInline.c" */, 3517);
    return 0;
}
HOOK(0x464e00, CInline_GetStatementNumber);

Bool8 CInline_CanFreeLHeap(void) {
    void **action;

#ifdef RECOMP_NEGATIVE_CONTROL_CINLINE2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINLINE2 */
    *(volatile int *)0 = 0; /* crash: freeing the heap early is not caught */
#endif
    if (!anyerrors) {
        for (action = cinline_actionlist; action; action = action[0])
            if (((UInt8 *)action)[0x22] == 0) /* CI_ActionInlineFunc */
                return 0;
    }
    return 1;
}
HOOK(0x466510, CInline_CanFreeLHeap);

/* ---- part 3: storage flags, ID translation, default-function actions (not matched by gcmatch;
   the same logic as mweppc's) ---- */

/* CI_SFLAGS: NoClass 0, Register 1, Auto 2, Referenced 0x80. Object.sclass (short) +0x14 (TK_AUTO
   0x100, TK_REGISTER 0x101). IDTrans (0xc bytes): next +0, from +4, to +8. CI_Action (0x24 bytes):
   next +0, obj +4, actiontype +0x22 (CI_ActionDefaultFunc 3). */
ORIG_FN(void *, galloc, 0x4332e0, (SInt32 size));
ORIG_FN(void *, lalloc, 0x433320, (SInt32 size));
ORIG_FN(void, memclrw, 0x4334d0, (void *ptr, SInt32 size));
ORIG_FN(SInt32, CParser_GetUniqueID, 0x42e960, (void));
#define CINLINE_FILE ((const char *)0x536990) /* "CInline.c" */

UInt8 CInline_GetObjectSFlags(UInt8 *object) {
    UInt8 flags = 0;

    switch (F(object, 0x14, SInt16)) {
    case 0:
        flags = 0;
        break;
    case 0x101:
        flags = 1;
        break;
    case 0x100:
        flags = 2;
        break;
    default:
        CError_Internal(CINLINE_FILE, 1204);
    }
    if (object[0x16] & 2)
        flags |= 0x80;
    return flags;
}
HOOK(0x4627a0, CInline_GetObjectSFlags);

void CInline_SetObjectSFlags_(UInt8 *object, UInt8 sflags) {
#ifdef RECOMP_NEGATIVE_CONTROL_CINLINE3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINLINE3 */
    *(volatile int *)0 = 0; /* crash: its storage classes change no output */
#endif
    if (sflags & 0x80) {
        object[0x16] |= 2;
        sflags &= 0x7f;
    }
    switch (sflags) {
    case 0:
        F(object, 0x14, SInt16) = 0;
        break;
    case 1:
        F(object, 0x14, SInt16) = 0x101;
        break;
    case 2:
        F(object, 0x14, SInt16) = 0x100;
        break;
    default:
        CError_Internal(CINLINE_FILE, 1229);
    }
}
HOOK(0x462800, CInline_SetObjectSFlags_);

SInt32 CInline_TranslateID(SInt32 id) {
    SInt32 *trans;

    for (trans = enode_idtrans; trans; trans = *(SInt32 **)trans)
        if (trans[1] == id)
            return trans[2];
    trans = lalloc(0xc);
    *(void **)trans = enode_idtrans;
    enode_idtrans = trans;
    trans[1] = id;
    trans[2] = CParser_GetUniqueID();
    return trans[2];
}
HOOK(0x461f30, CInline_TranslateID);

void CInline_AddDefaultFunctionAction(void *object) {
    void **action;

    for (action = cinline_actionlist; action; action = action[0])
        if (action[1] == object)
            return;
    action = galloc(0x24);
    memclrw(action, 0x24);
    ((UInt8 *)action)[0x22] = 3; /* CI_ActionDefaultFunc */
    action[1] = object;
    action[0] = cinline_actionlist;
    cinline_actionlist = action;
}
HOOK(0x465a40, CInline_AddDefaultFunctionAction);

/* ---- part 4: CInline_ObjectAddrRef (not matched by gcmatch; the same logic as mweppc's) ---- */

/* Object: datatype +2 (DDATA 0, DFUNC 3, DVFUNC 4, DALIAS 6), type +0xc, qual +0x10 (Q_INLINE
   0x10, Q_INLINE_DATA 0x10000), flags (byte) +0x16 (REFERENCED 2, DEFINED 4, LAZY 8),
   u.func.u.ifuncdata / u.alias.object +0x28. TypeFunc.flags +0x16 (FUNC_DEFINED 2,
   FUNC_AUTO_GENERATED 0x100, FUNC_IS_TEMPL 0x100000, FUNC_TEMPLATE_ACTION 0x200000). CI_Export
   (0xe bytes): next +0, object +4, funcdata +8, oldStyleParameters (byte) +0xc. */
ORIG_FN(void, CInline_GenerateTemplateInline, 0x465ef0, (void *object));
ORIG_FN(void, CInit_ExportConst, 0x45c830, (void *object));
ORIG_FN(void, CParser_CallBackAction, 0x432270, (void *object));

void CInline_ObjectAddrRef_(UInt8 *object) {
    UInt32 tflags;
    void *funcdata;
    void **export;

    object[0x16] |= 2;
    switch (object[2]) {
    case 3:
    case 4:
        tflags = F(F(object, 0xc, UInt8 *), 0x16, UInt32);
        if ((F(object, 0x10, UInt32) & 0x10) && (funcdata = F(object, 0x28, void *)) != 0 &&
            !(object[0x16] & 4) && !(tflags & 0x100000)) {
            export = galloc(0xe);
            export[1] = object;
            export[2] = funcdata;
            ((UInt8 *)export)[0xc] = 0;
            export[0] = cinline_exportlist;
            cinline_exportlist = export;
            object[0x16] |= 4;
        } else if ((tflags & 0x100) && !(tflags & 2)) {
            CInline_AddDefaultFunctionAction(object);
        } else if (tflags & 0x200000) {
            CInline_GenerateTemplateInline(object);
        }
        break;
    case 6:
        CInline_ObjectAddrRef_(F(object, 0x28, UInt8 *));
        break;
    case 0:
        if (F(object, 0x10, UInt32) & 0x10000)
            CInit_ExportConst(object);
        if (object[0x16] & 8) {
            object[0x16] &= ~8;
#ifndef RECOMP_NEGATIVE_CONTROL_CINLINE4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINLINE4 */
            CParser_CallBackAction(object);
#endif
        }
        break;
    }
}
HOOK(0x465f40, CInline_ObjectAddrRef_);

/* CInline_AddFRefList_Object (the same logic as mweppc's). cinline_freflist 0x54d6ec
   (ObjectList {next, object}). */
ORIG_FN(void, CInline_AddFRefList_InlineFunc, 0x465da0, (void *funcdata));
#define cinline_freflist ORIG_VAR(void **, 0x54d6ec)

void CInline_AddFRefList_Object(UInt8 *object) {
    UInt8 *type = F(object, 0xc, UInt8 *);
    void **list;

    if (!(object[2] == 3 || object[2] == 4) || (object[0x16] & 4) ||
        (type[0] == 6 && (F(type, 0x16, UInt32) & 0x100000)))
        return;
    for (list = cinline_freflist; list; list = list[0])
        if (list[1] == object)
            return;
    list = lalloc(8);
    list[1] = object;
    list[0] = cinline_freflist;
    cinline_freflist = list;
    if ((F(object, 0x10, UInt32) & 0x10) && F(object, 0x28, void *))
        CInline_AddFRefList_InlineFunc(F(object, 0x28, void *));
}
HOOK(0x465bd0, CInline_AddFRefList_Object);

/* CInline_AddFRefList_Expr and CInline_AddFRefList_Statement (mweppc's logic; ours treats
   statement types 9..11 and 15 as fatal and skips 14). The expression callback (0x465d20) is
   only passed by pointer and stays the original's. Statement.dobjstack +0x12. */
ORIG_FN(void, CExpr_SearchExprTree, 0x4ef190, (void *expr, void *cb, int count, ...));
ORIG_FN(void, CInline_AddFRefList_ExAction, 0x465c50, (void *exc));

void CInline_AddFRefList_Expr(void *expr) {
    CExpr_SearchExprTree(expr, (void *)0x465d20, 1, 0x38 /* EOBJREF */);
}
HOOK(0x465d30, CInline_AddFRefList_Expr);

void CInline_AddFRefList_Statement(UInt8 *stmt) {
    for (; stmt; stmt = F(stmt, 0, UInt8 *)) {
        if (F(stmt, 0x12, void *))
            CInline_AddFRefList_ExAction(F(stmt, 0x12, void *));
        switch (stmt[4]) {
        case 1: case 2: case 3: case 12: case 13: case 14: case 16:
            break;
        case 8: /* ST_RETURN */
            if (!F(stmt, 0xa, void *))
                break;
            /* fall through */
        case 4: case 5: case 6: case 7:
#ifndef RECOMP_NEGATIVE_CONTROL_CINLINE5 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINLINE5 */
            CInline_AddFRefList_Expr(F(stmt, 0xa, void *));
#endif
            break;
        default:
            CError_Internal(CINLINE_FILE, 4368);
        }
    }
}
HOOK(0x465d50, CInline_AddFRefList_Statement);

/* CInline_CheckDependencies (mweppc's logic). Object.qual Q_IS_TEMPLATED 0x400000;
   CTempl_InlineFunctionCheck 0x477a90. */
ORIG_FN(Bool8, CTempl_InlineFunctionCheck, 0x477a90, (void *object));

Bool8 CInline_CheckDependencies(void **list) {
    Bool8 result = 0;
    UInt8 *object;
    UInt32 tflags;
    void **action;

    for (; list; list = list[0]) {
        object = list[1];
        tflags = F(F(object, 0xc, UInt8 *), 0x16, UInt32);
        if ((tflags & 0x100) && !(tflags & 2)) {
            CInline_AddDefaultFunctionAction(object);
            result = 1;
            continue;
        }
        if ((F(object, 0x10, UInt32) & 0x400000) && CTempl_InlineFunctionCheck(object)) {
            result = 1;
            continue;
        }
        for (action = cinline_actionlist; action; action = action[0]) {
            if (action[1] == object) {
                result = 1;
                break;
            }
        }
        if (F(F(object, 0xc, UInt8 *), 0x16, UInt32) & 0x200000) {
            CInline_GenerateTemplateInline(object);
            result = 1;
        }
    }
    return result;
}
HOOK(0x466030, CInline_CheckDependencies);

/* ---- part 5: the class with deferred inline functions (from the exe) ---- */

/* the class itself when its flags (+0x26) have 0x800; with copts 0x57dba0, else the first base
   (list at *(+6): next +0, class +0xc) that has a class, followed on */
UInt8 *fn_4663a0(UInt8 *tclass) {
    void **base;

    while (tclass) {
        if (F(tclass, 0x26, UInt16) & 0x800)
            return tclass;
        if (!*(Bool8 *)0x57dba0)
            return 0;
        for (base = *F(tclass, 6, void ***), tclass = 0; base; base = (void **)base[0]) {
            if (F(base, 0xc, UInt8 *)) {
                tclass = F(base, 0xc, UInt8 *);
                break;
            }
        }
    }
    return 0;
}
HOOK(0x4663a0, fn_4663a0);

/* whether a function body is small enough to inline: at most 15 statements that are not of type
   1 or 2 (Statement: next +0, type +4), and locals (0x57c904: next +0, object +4) of at most
   0x400 bytes in all */
Bool8 fn_4660c0(void *func, void **stmt) {
    int count = 0;
    SInt32 size = 0;
    void **local;

    for (; stmt; stmt = (void **)stmt[0]) {
        if (F(stmt, 4, UInt8) != 1 && F(stmt, 4, UInt8) != 2)
            count++;
        if (count > 0xf)
            return 0;
    }
    for (local = *(void ***)0x57c904; local; local = (void **)local[0])
        size += F(F(local[1], 0xc, UInt8 *), 2, SInt32);
#ifdef RECOMP_NEGATIVE_CONTROL_CINLINE6 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINLINE6 */
    return 0;
#endif
    if (size > 0x400)
        return 0;
    return 1;
}
HOOK(0x4660c0, fn_4660c0);

/* wraps an expression in a node of type 8 (a copy of it, operand +0x18) when the walk 0x4ef190
   with the callback 0x462da0 sets 0x54d6dc */
ORIG_FN(void, fn_4ef190, 0x4ef190, (void *expr, void *func, SInt32 a, SInt32 b, SInt32 c, SInt32 d));

UInt8 *fn_462db0(UInt8 *expr) {
    UInt8 *wrap;
    int i;

    if (expr[0] == 8)
        return expr;
    *(Bool8 *)0x54d6dc = 0;
    fn_4ef190(expr, (void *)0x462da0, 3, 4, 0x36, 0x37);
    if (!*(Bool8 *)0x54d6dc)
        return expr;
    wrap = lalloc(0x28);
    for (i = 0; i < 10; i++)
        ((UInt32 *)wrap)[i] = ((UInt32 *)expr)[i];
    wrap[0] = 8;
    F(wrap, 0x18, void *) = expr;
    return wrap;
}
HOOK(0x462db0, fn_462db0);

/* ---- part 6: inline statement and argument helpers (from the exe) ---- */

/* a new 0x1e-byte statement of the given type appended to the inline statement list (first
   0x54d6e6, last 0x54d6e2) */
UInt8 *fn_4637c0(UInt8 type) {
    UInt8 *stmt = lalloc(0x1e);

    memclrw(stmt, 0x1e);
    stmt[4] = type;
    if (*(UInt8 **)0x54d6e6)
        **(UInt8 ***)0x54d6e2 = stmt;
    else
        *(UInt8 **)0x54d6e6 = stmt;
    *(UInt8 **)0x54d6e2 = stmt;
    return stmt;
}
HOOK(0x4637c0, fn_4637c0);

/* the argument object replacing an object reference (+0x18) in the inline argument list
   0x54d6de (next +0, object +4, original +8) as an object node (create_objectnode 0x4f0200);
   a missing one is an internal error */
ORIG_FN(void *, create_objectnode_i, 0x4f0200, (void *obj));

void *fn_463a90(UInt8 *expr) {
    void **p;

    for (p = *(void ***)0x54d6de;; p = (void **)p[0]) {
        if (!p)
            CError_Internal(CINLINE_FILE, 0x991);
        if (p[2] == F(expr, 0x18, void *))
            return create_objectnode_i(p[1]);
    }
}
HOOK(0x463a90, fn_463a90);

/* ---- part 7: expression copies (mweppc's CInline_CopyExpression family, from the exe) ---- */

/* The copier 0x4620b0 works by the globals copy_global (0x54d75c: galloc instead of lalloc),
   copy mode (0x54d75d) and the copy map (0x54d758). */
ORIG_FN(void *, CInline_CopyNodes_i, 0x4620b0, (void *expr));
ORIG_FN(void *, galloc_i, 0x4332e0, (SInt32 size));
ORIG_FN(void *, lalloc_i, 0x433320, (SInt32 size));

/* a global copy of expr in mode 4, keeping the copier's settings */
void *CInline_CopyExpressionGlobal(void *expr) {
    UInt8 save_global = *(UInt8 *)0x54d75c;
    UInt8 save_mode = *(UInt8 *)0x54d75d;
    void *save_map = *(void **)0x54d758;
    void *result;

    *(UInt8 *)0x54d75c = 1;
#ifdef RECOMP_NEGATIVE_CONTROL_CINLINE7 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINLINE7 */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "CINLINE7@"); /* marks runs */
#endif
    *(UInt8 *)0x54d75d = 4;
    *(void **)0x54d758 = 0;
    result = CInline_CopyNodes_i(expr);
    *(UInt8 *)0x54d75c = save_global;
    *(UInt8 *)0x54d75d = save_mode;
    *(void **)0x54d758 = save_map;
    return result;
}
HOOK(0x461ed0, CInline_CopyExpressionGlobal);

/* a copy of an ENodeList (next +0, node +4), each node copied by 0x4620b0 */
void **CInline_CopyNodeList(void **list) {
    void **head = 0, **last = 0, **n;

    for (; list; list = (void **)list[0]) {
        n = *(UInt8 *)0x54d75c ? galloc_i(8) : lalloc_i(8);
        n[1] = CInline_CopyNodes_i(list[1]);
        n[0] = 0;
        if (head)
            last[0] = n;
        else
            head = n;
        last = n;
    }
    return head;
}
HOOK(0x461fd0, CInline_CopyNodeList);

/* ---- part 8: CInline_GenerateDeferredFuncs (mweppc's logic, from the exe) ---- */

/* Deferred work (list 0x54d6f0; next +0, kind +0x22): 0 an inline function (+4) not yet done
   (qual +0x16 & 4) goes to 0x4663e0, 1 a member template instance (0x477690 with +8, +0xc, +0x10,
   the object +4) and 2 a function template instance (0x477460 with +8, +0xc unless it is
   specialized, +0xd) when not yet defined (type +0xc, flags +0x16 & 2), 3 a default function
   (0x4662d0). Then the inline function list 0x54d6f4 (next +0, object +4, saved state +8 (its
   +0x12 restored around 0x465950), flag +0xc), unless 0x57db8e is set. Nothing while 0x57e03b. */
ORIG_FN(void, fn_4663e0_i, 0x4663e0, (UInt8 *entry));
ORIG_FN(void, fn_477690_i, 0x477690, (void *a, void *b, void *c, void *obj, UInt8 flag));
ORIG_FN(Bool8, CTempl_GenFuncInstance_i, 0x477460, (void *templ, void *inst, UInt8 flag));
ORIG_FN(void, CInline_GenerateDefaultFunc_i, 0x4662d0, (void *obj));
ORIG_FN(void, CError_Internal_i, 0x457230, (const char *file, int line));
ORIG_FN(void, CPrep_SaveState_i, 0x459150, (UInt8 *buf));
ORIG_FN(void, CPrep_RestoreState_i, 0x459300, (UInt8 *buf));
ORIG_FN(void, CInline_GenFunc_i, 0x465950, (void *obj, UInt8 *state, UInt8 flag));

Bool8 CInline_GenerateDeferredFuncs(void) {
    UInt8 save[0x38];
    UInt8 *e, *obj;

    if (*(UInt8 *)0x57e03b)
        return 0;
    if ((e = *(UInt8 **)0x54d6f0)) {
        *(UInt8 **)0x54d6f0 = *(UInt8 **)e;
        *(UInt8 *)0x54d6ea = 1;
        obj = *(UInt8 **)(e + 4);
        switch (e[0x22]) {
        case 0:
            if (!(obj[0x16] & 4))
                fn_4663e0_i(e);
            break;
        case 1:
            if (!(*(UInt32 *)(*(UInt8 **)(obj + 0xc) + 0x16) & 2))
                fn_477690_i(*(void **)(e + 8), *(void **)(e + 0xc), *(void **)(e + 0x10), obj, 0);
            break;
        case 2:
            if (!(*(UInt32 *)(*(UInt8 **)(obj + 0xc) + 0x16) & 2) && !(*(UInt8 **)(e + 0xc))[0xd])
                CTempl_GenFuncInstance_i(*(void **)(e + 8), *(void **)(e + 0xc), 0);
            break;
        case 3:
#ifndef RECOMP_NEGATIVE_CONTROL_CINLINE8 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINLINE8 */
            CInline_GenerateDefaultFunc_i(obj);
#endif
            break;
        default:
            CError_Internal_i((const char *)0x536990 /* "CInline.c" */, 0x1389);
        }
        *(UInt8 *)0x54d6ea = 0;
        return 1;
    }
    if (*(UInt8 **)0x54d6f4 && !*(UInt8 *)0x57db8e) {
        e = *(UInt8 **)0x54d6f4;
        *(UInt8 **)0x54d6f4 = *(UInt8 **)e;
        CPrep_SaveState_i(save);
        CPrep_RestoreState_i(*(UInt8 **)(e + 8) + 0x12);
        CInline_GenFunc_i(*(void **)(e + 4), *(UInt8 **)(e + 8), e[0xc]);
        CPrep_RestoreState_i(save);
        *(UInt8 *)0x57db8e = 0;
        return 1;
    }
    return 0;
}
HOOK(0x466540, CInline_GenerateDeferredFuncs);
