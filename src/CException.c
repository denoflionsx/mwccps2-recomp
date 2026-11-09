/*
 * CException.c — C++ exception actions of the frontend (mweppc
 * src/compiler/frontend/CException.c), partial port.
 *
 * The functions here match mweppc's GameCube 2.0 compiler; in CExcept_RegisterDestructorObject our
 * exe uses stunsignedint (0x536068) for the offset constant where mweppc has stunsignedlong.
 * Ported so far: CExcept_Setup, CExcept_ExceptionTansform, CExcept_Terminate and
 * CExcept_RegisterDestructorObject. CExcept_ActionCompare and the action cleanup stay the
 * original's.
 *
 * ExceptionAction (0x1e bytes): prev +0, data +4 (destroy_local {local, dtor},
 * destroy_local_offset {local, dtor, offset}), type (byte) +0x1c. EAT_DESTROYLOCAL 1,
 * EAT_DESTROYLOCALOFFSET 3, EAT_TERMINATE 0x10. TypeFunc.exspecs +0xa. EADD 0xf.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))
#define S(addr) ((const char *)(addr))

ORIG_FN(void *, lalloc, 0x433320, (SInt32 size));
ORIG_FN(void, memclrw, 0x4334d0, (void *ptr, SInt32 size));
ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));
ORIG_FN(void, CExcept_CleanupExceptionActions, 0x4ec3c0, (void *stmt));
ORIG_FN(Bool8, CExcept_CanThrowCheck, 0x4ec700, (void *func, void *stmt));
ORIG_FN(void, CExcept_InsertSpecificationActions, 0x4ec4e0, (void *stmt, void *exspecs));
ORIG_FN(void *, create_objectrefnode, 0x4f0170, (void *obj));
ORIG_FN(void *, CABI_GetDestructorObject, 0x4556a0, (void *dtor, int what));
ORIG_FN(void *, intconstnode, 0x4ef2c0, (void *type, SInt32 value));
ORIG_FN(void *, makediadicnode, 0x4ef360, (void *left, void *right, UInt8 ty));

#define cexcept_dobjstack ORIG_VAR(void **, 0x57c914)
#define cexcept_uniqueobjs ORIG_VAR(void *, 0x56794e)
#define cexcept_hasdobjects ORIG_VAR(Bool8, 0x57e14e)
#define cexcept_magic ORIG_VAR(Bool8, 0x57e037)
#define cscope_currentfunc ORIG_VAR(UInt8 *, 0x57d6b0)
#define copts_exceptions ORIG_VAR(Bool8, 0x57dbd6)
#define stunsignedint ((void *)0x536068)

void CExcept_Setup(void) {
    cexcept_dobjstack = 0;
    cexcept_uniqueobjs = 0;
    cexcept_hasdobjects = 0;
    cexcept_magic = 0;
}
HOOK(0x4e9b50, CExcept_Setup);

void *CExcept_RegisterDestructorObject(void *local, SInt32 offset, void *dtor, Bool8 unused) {
    void **action;
    void *expr;
    (void)unused;

    action = lalloc(0x1e);
    memclrw(action, 0x1e);
    action[0] = cexcept_dobjstack;
    cexcept_dobjstack = action;

    expr = create_objectrefnode(local);
    dtor = CABI_GetDestructorObject(dtor, 1); /* CABIDestroyComplete */
    if (offset == 0) {
        F(action, 0x1c, UInt8) = 1;
        action[1] = local;
        action[2] = dtor;
    } else {
        F(action, 0x1c, UInt8) = 3;
        action[1] = local;
        action[2] = dtor;
        F(action, 0xc, SInt32) = offset;
        expr = makediadicnode(expr, intconstnode(stunsignedint, offset), 0xf);
    }
    cexcept_hasdobjects = 1;
    return expr;
}
HOOK(0x4e9fb0, CExcept_RegisterDestructorObject);

void CExcept_Terminate(void) {
    void **action = lalloc(0x1e);
    memclrw(action, 0x1e);
    F(action, 0x1c, UInt8) = 0x10;
    action[0] = cexcept_dobjstack;
    cexcept_dobjstack = action;
}
HOOK(0x4ea1d0, CExcept_Terminate);

void CExcept_ExceptionTansform(void *stmt) {
    cexcept_uniqueobjs = 0;
#ifndef RECOMP_NEGATIVE_CONTROL_CEXCEPTION /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXCEPTION: no action cleanup */
    CExcept_CleanupExceptionActions(stmt);
#endif
    if (cscope_currentfunc && CExcept_CanThrowCheck(cscope_currentfunc, stmt)) {
        if (F(cscope_currentfunc, 0xc, UInt8 *)[0] != 6)
            CError_Internal(S(0x544348) /* "CException.c" */, 2727);
        if (F(F(cscope_currentfunc, 0xc, UInt8 *), 0xa, void *) && copts_exceptions)
            CExcept_InsertSpecificationActions(stmt, F(F(cscope_currentfunc, 0xc, UInt8 *), 0xa, void *));
    }
}
HOOK(0x4ec790, CExcept_ExceptionTansform);

/* ---- part 2: CExcept_CanThrowException (close to mweppc: one more runtime object) ---- */

/* copts.optEH 0x57dbb9, optEH2 0x57dbba. Object: type +0xc, qual +0x10 (Q_MANGLE_NAME 0x80000).
   TypeFunc: exspecs +0xa (ExceptSpecList.type +4), flags +0x16 (FUNC_NOTHROW 0x400). The runtime
   objects that may throw even with optEH2, in the original's order: mweppc's rt_ptmf_call,
   rt_ptmf_scall, Rdync_func, rt_som_glue1..3, carr_func, cnar_func, darr_func, dnar_func,
   dnar3_func, Xthrw_func, and one more of ours (0x57c754). */
#define copts_optEH ORIG_VAR(Bool8, 0x57dbb9)
#define copts_optEH2 ORIG_VAR(Bool8, 0x57dbba)

static void *const *const throwing_rt_objects[] = {
    (void *const *)0x57b758, (void *const *)0x57c924, (void *const *)0x57cea4, (void *const *)0x57d6e4,
    (void *const *)0x57d6e8, (void *const *)0x57d6ec, (void *const *)0x57c7d8, (void *const *)0x57c748,
    (void *const *)0x57abe8, (void *const *)0x57b128, (void *const *)0x57cac0, (void *const *)0x57c754,
};

Bool8 CExcept_CanThrowException(UInt8 *object, Bool8 isVirtualCall) {
    UInt8 *type;
    unsigned i;

    if (copts_optEH && (type = F(object, 0xc, UInt8 *))[0] == 6) {
        if (isVirtualCall)
            return 1;
        if (F(type, 0x16, UInt32) & 0x400) /* FUNC_NOTHROW */
            return 0;
        if (F(type, 0xa, UInt8 *) && !F(F(type, 0xa, UInt8 *), 4, void *))
            return 0; /* throw() */
#ifdef RECOMP_NEGATIVE_CONTROL_CEXCEPTION2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXCEPTION2 */
        return 0; /* nothing throws */
#endif
        if (copts_optEH2 && !(F(object, 0x10, UInt32) & 0x80000)) {
            for (i = 0; i < sizeof(throwing_rt_objects) / sizeof(throwing_rt_objects[0]); i++)
                if (object == *throwing_rt_objects[i])
                    return 1;
            return 0;
        }
    }
    return 1;
}
HOOK(0x4e9b80, CExcept_CanThrowException);

/* ---- part 3: CExcept_CanThrowCheck and its callback (not matched by gcmatch; the same logic as
   mweppc's) ---- */

/* cexcept_canthrow 0x56794c, cscope_currentfunc 0x57d6b0. Statement: next +0, type (byte) +4, expr
   +0xa. Statement types by the original's table: 1..3 and 16 skipped, 8 (return) searched when it
   has an expression, 4..7 and 12..14 searched, the rest fatal. EFUNCCALL 0x36, EFUNCCALLP 0x37;
   funcref +0x18, EOBJREF 0x38 (objref +0x18); ENODE_FLAG_NONVIRTUAL 0x80; DVFUNC 4. */
ORIG_FN(void, CExpr_SearchExprTree, 0x4ef190, (void *expr, void (*cb)(UInt8 *), int count, ...));
#define cexcept_canthrow ORIG_VAR(Bool8, 0x56794c)
#define cscope_currentfunc ORIG_VAR(UInt8 *, 0x57d6b0)

/* The callback (the original's at 0x4ec6c0 is not a function start in x86map, so it is not hooked;
   CExcept_CanThrowCheck_ passes this one directly). */
static void CExcept_HasFuncCallCallBack(UInt8 *expr) {
    UInt8 *funcref = F(expr, 0x18, UInt8 *);
    UInt8 *obj;

    if (funcref[0] == 0x38) {
        obj = F(funcref, 0x18, UInt8 *);
        if (!CExcept_CanThrowException(obj, obj[2] == 4 && !(F(expr, 2, UInt16) & 0x80)))
            return;
    }
    cexcept_canthrow = 1;
}

Bool8 CExcept_CanThrowCheck_(void *func, UInt8 *stmt) {
    cexcept_canthrow = 0;
    for (; stmt; stmt = F(stmt, 0, UInt8 *)) {
        switch (stmt[4]) {
        case 1: case 2: case 3: case 16:
            break;
        case 8:
            if (!F(stmt, 0xa, void *))
                break;
            /* fall through */
        case 4: case 5: case 6: case 7: case 12: case 13: case 14:
            CExpr_SearchExprTree(F(stmt, 0xa, void *), CExcept_HasFuncCallCallBack, 2, 0x36, 0x37);
            break;
        default:
            CError_Internal(S(0x544348) /* "CException.c" */, 2687);
        }
    }
    if (!cexcept_canthrow) {
#ifndef RECOMP_NEGATIVE_CONTROL_CEXCEPTION3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXCEPTION3 */
        F(F(cscope_currentfunc, 0xc, UInt8 *), 0x16, UInt32) |= 0x400; /* FUNC_NOTHROW */
#endif
        return 0;
    }
    return 1;
}
HOOK(0x4ec700, CExcept_CanThrowCheck_);

/* ---- part 4: an exception-action statement (from the exe) ---- */

/* a new statement of the given type after 0x567944 (CFunc_InsertStatement 0x470c30), with the
   source position (+0x16, +0x1a) of the statement it follows (+0) and the current action list
   0x567940 (+0x12) */
ORIG_FN(UInt8 *, CFunc_InsertStatement_e, 0x470c30, (SInt32 type, void *after));

UInt8 *fn_4eb540(SInt32 type) {
    UInt8 *stmt = CFunc_InsertStatement_e(type, *(void **)0x567944);

    F(stmt, 0x16, UInt32) = F(F(stmt, 0, UInt8 *), 0x16, UInt32);
    F(stmt, 0x1a, UInt32) = F(F(stmt, 0, UInt8 *), 0x1a, UInt32);
    F(stmt, 0x12, void *) = *(void **)0x567940;
#ifdef RECOMP_NEGATIVE_CONTROL_CEXCEPTION4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXCEPTION4 */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "CEXCEPTION4@"); /* marks runs */
#endif
    return stmt;
}
HOOK(0x4eb540, fn_4eb540);

/* whether an action kind (+0x1c) needs cleanup: 1, 3, 5, 0xb, 0xe yes; 0xd no; 2, 4, 6, 0xa,
   0xc, 0xf, 0x10 and unknown kinds are internal errors; the rest no */
Bool8 fn_4e9f70(UInt8 *act) {
    switch (act[0x1c]) {
    case 1:
    case 3:
    case 5:
    case 0xb:
    case 0xe:
        return 1;
    case 0xd:
        return 0;
    case 0:
    case 7:
    case 8:
    case 9:
    case 0x11:
        return 0;
    }
    CError_Internal(S(0x544348) /* "CException.c" */, 0x16b);
    return 0;
}
HOOK(0x4e9f70, fn_4e9f70);

/* an ST_GOTOEXPR-like (0xe, or 0xd when not flagged +8) statement after after for an action:
   its object (+4) referenced, its label (+0) at +0x12 */
ORIG_FN(UInt8 *, create_objectrefnode_e, 0x4f0170, (void *obj));

UInt8 *fn_4ea550(void **act, void *after) {
    UInt8 *stmt = CFunc_InsertStatement_e(0xe, after);

    F(stmt, 0xa, void *) = create_objectrefnode_e(act[1]);
    F(stmt, 0x12, void *) = act[0];
    if (!((UInt8 *)act)[8])
        stmt[4] = 0xd;
    return stmt;
}
HOOK(0x4ea550, fn_4ea550);

/* ---- part 5: the exception expression transform walk (from the exe) ---- */

/* ENode: type +0, operands +0x18, +0x1c, +0x20. Constructor-like nodes go to their transforms
   (0x3c 0x4eb5e0, 0x41/0x42 0x4eb6c0, 0x43 0x4eb950, calls 0x36/0x37 0x4ebbc0); the second and third
   operands of 0x1c, 0x1d, 0x29, 0x35 and 0x3a go through 0x4ebf40; other nodes recurse into their
   operands. Unknown types are an internal error (line 0x92e). */
ORIG_FN(UInt8 *, fn_4eb5e0_x, 0x4eb5e0, (UInt8 *e));
ORIG_FN(UInt8 *, fn_4eb6c0_x, 0x4eb6c0, (UInt8 *e, UInt8 flag));
ORIG_FN(UInt8 *, fn_4eb950_x, 0x4eb950, (UInt8 *e, UInt8 flag));
ORIG_FN(UInt8 *, fn_4ebbc0_x, 0x4ebbc0, (UInt8 *e, UInt8 flag));
ORIG_FN(UInt8 *, fn_4ebf40_x, 0x4ebf40, (UInt8 *e));
ORIG_FN(void, CError_Internal_x, 0x457230, (const char *file, int line));
#define XE(e, off) (*(UInt8 **)((e) + (off)))

UInt8 *fn_4ec040(UInt8 *e) {
    UInt8 t = e[0];
#ifdef RECOMP_NEGATIVE_CONTROL_CEXCEPTION5 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXCEPTION5 */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "CEXCEPTION5@"); /* marks runs */
#endif

    switch (t) {
    case 0x3c:
        return fn_4eb5e0_x(e);
    case 0x41: case 0x42:
        return fn_4eb6c0_x(e, 0);
    case 0x43:
        return fn_4eb950_x(e, 0);
    case 0x36: case 0x37:
        return fn_4ebbc0_x(e, 0);
    case 0x35:
        XE(e, 0x18) = fn_4ec040(XE(e, 0x18));
        XE(e, 0x1c) = fn_4ebf40_x(XE(e, 0x1c));
        XE(e, 0x20) = fn_4ebf40_x(XE(e, 0x20));
        return e;
    case 0x1c: case 0x1d: case 0x29: case 0x3a:
        XE(e, 0x18) = fn_4ec040(XE(e, 0x18));
        XE(e, 0x1c) = fn_4ebf40_x(XE(e, 0x1c));
        return e;
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 0x30: case 0x31:
        XE(e, 0x18) = fn_4ec040(XE(e, 0x18));
        return e;
    case 9: case 0xb: case 0xc: case 0xf: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14:
    case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1e: case 0x1f:
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28:
    case 0x2b: case 0x2c:
        XE(e, 0x18) = fn_4ec040(XE(e, 0x18));
        XE(e, 0x1c) = fn_4ec040(XE(e, 0x1c));
        return e;
    case 0x32: case 0x33: case 0x34: case 0x38: case 0x39: case 0x3b: case 0x3f: case 0x44: case 0x45:
    case 0x47:
        return e;
    default:
        CError_Internal_x((const char *)0x544348 /* "CException.c" */, 0x92e);
        return e;
    }
}
HOOK(0x4ec040, fn_4ec040);
