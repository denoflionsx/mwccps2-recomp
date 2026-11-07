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
