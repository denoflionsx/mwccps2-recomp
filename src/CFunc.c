/*
 * CFunc.c — function bodies of the C/C++ frontend (mweppc src/compiler/frontend/CFunc.c),
 * partial port.
 *
 * The functions here are byte-identical between mweppc's GameCube 2.0 compiler and ours. Ported so
 * far: newlabel, the statement list helpers (append, insert after, insert before),
 * create_temp_object, CFunc_WarnUnused, CFunc_CodeCleanup and CFunc_FuncGenSetup. CFunc_ParseFuncDef,
 * CFunc_Gen and the rest stay the original's.
 *
 * Statement (0x1e bytes): next +0, type (byte) +4, flags (byte) +6, value (SInt16) +8,
 * dobjstack +0x12, sourceoffset +0x16, sourcefilepath +0x1a. CLabel 0x14 bytes: name +8,
 * uniquename +0xc. Object: name +8, type +0xc, qual +0x10, flags (byte) +0x16, u.var.info +0x28
 * (deftoken at +8). DeclBlock 0xe bytes: parent_nspace +8, index (SInt16) +0xc.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void *, lalloc, 0x433320, (SInt32 size));
ORIG_FN(void, memclrw, 0x4334d0, (void *ptr, SInt32 size));
ORIG_FN(void *, CParser_GetUniqueName, 0x42e9f0, (void));
ORIG_FN(void *, CParser_NewLocalDataObject, 0x42ed80, (void *declinfo, Bool8 add));
ORIG_FN(void, CFunc_SetupLocalVarInfo, 0x4704d0, (void *object));
ORIG_FN(void, CSOM_InitSOMSelf, 0x452470, (void *tclass, void *stmt));
ORIG_FN(void, CExcept_ExceptionTansform, 0x4ec790, (void *stmt));
ORIG_FN(void, CExcept_Setup, 0x4e9b50, (void));
ORIG_FN(Bool8, IsTempName, 0x42ec70, (void *name));
ORIG_FN(void, CError_SetErrorToken, 0x4557b0, (void *token));
ORIG_FN(void, CError_Warning, 0x4571c0, (int code, ...));
ORIG_FN(void *, CScope_NewListNameSpace, 0x44b3f0, (void *name, Bool8 is_global));

#define curstmt ORIG_VAR(UInt8 *, 0x57b6e8)
#define curstmtvalue ORIG_VAR(SInt32, 0x57abb8)
#define sourceoffset ORIG_VAR(SInt32, 0x57ceb4)
#define sourcefilepath ORIG_VAR(void *, 0x57b6ec)
#define cexcept_dobjstack ORIG_VAR(void *, 0x57c914)
#define cscope_currentclass ORIG_VAR(UInt8 *, 0x57ca28)
#define cscope_current ORIG_VAR(void *, 0x57d6bc)
#define warn_unusedvar ORIG_VAR(Bool8, 0x57dbc4) /* copts */
#define warn_unusedarg ORIG_VAR(Bool8, 0x57dbc5) /* copts */
#define locals ORIG_VAR(void **, 0x57c904)       /* ObjectList {next, object} */
#define arguments ORIG_VAR(void **, 0x57caa0)
#define Labels ORIG_VAR(void *, 0x57d6ac)
#define localcount ORIG_VAR(SInt16, 0x57dc3e)
#define cfunc_staticvarcount ORIG_VAR(SInt16, 0x54d766)
#define blockcount ORIG_VAR(SInt16, 0x54d77c)
#define cfunc_currentblock ORIG_VAR(void *, 0x54d77e)
#define firstblock ORIG_VAR(void *, 0x54d782)
#define this_name_node ORIG_VAR(void *, 0x57b738)
#define self_name_node ORIG_VAR(void *, 0x57c8a0)
#define symdecltoken ((void *)0x56a830)

#define ERR_VARIABLE_ARGUMENT_NOT_USED_FUNCTION 0xb6
#define Q_INLINE_DATA 0x10000
#define OBJECT_USED 1

void *newlabel(void) {
    void *label = lalloc(0x14);
    memclrw(label, 0x14);
    F(label, 8, void *) = CParser_GetUniqueName();
    F(label, 0xc, void *) = F(label, 8, void *);
    return label;
}
HOOK(0x470bb0, newlabel);

void *CFunc_AppendStatement(UInt8 sttype) {
    UInt8 *stmt = lalloc(0x1e);
    F(stmt, 0, void *) = 0;
    stmt[4] = sttype;
    F(stmt, 8, SInt16) = (SInt16)curstmtvalue;
    stmt[6] = 0;
    F(stmt, 0x16, SInt32) = sourceoffset;
    F(stmt, 0x1a, void *) = sourcefilepath;
    F(stmt, 0x12, void *) = cexcept_dobjstack;
    F(curstmt, 0, void *) = stmt;
    curstmt = stmt;
    return stmt;
}
HOOK(0x470be0, CFunc_AppendStatement);

void *CFunc_InsertStatement(UInt8 sttype, UInt8 *after) {
    UInt8 *stmt = lalloc(0x1e);
    F(stmt, 0, void *) = F(after, 0, void *);
    F(after, 0, void *) = stmt;
    stmt[4] = sttype;
    F(stmt, 8, SInt16) = F(after, 8, SInt16);
    stmt[6] = 0;
    F(stmt, 0x16, SInt32) = F(after, 0x16, SInt32);
    F(stmt, 0x1a, void *) = F(after, 0x1a, void *);
    F(stmt, 0x12, void *) = F(after, 0x12, void *);
    return stmt;
}
HOOK(0x470c30, CFunc_InsertStatement);

void *CFunc_InsertBeforeStatement(UInt8 sttype, UInt8 *before) {
    UInt8 *stmt = lalloc(0x1e);
    int i;
    for (i = 0; i < 7; i++) /* *stmt = *before (0x1e bytes) */
        F(stmt, i * 4, UInt32) = F(before, i * 4, UInt32);
    F(stmt, 0x1c, UInt16) = F(before, 0x1c, UInt16);
    F(before, 0, void *) = stmt;
    before[4] = sttype;
    before[6] = 0;
    return before;
}
HOOK(0x470c70, CFunc_InsertBeforeStatement);

void *create_temp_object(void *type) {
    void *object = CParser_NewLocalDataObject(0, 1);
    F(object, 8, void *) = CParser_GetUniqueName();
    F(object, 0xc, void *) = type;
    CFunc_SetupLocalVarInfo(object);
    return object;
}
HOOK(0x470ce0, create_temp_object);

void CFunc_WarnUnused(void) {
    void **list;
    UInt8 *obj;

    if (warn_unusedvar) {
        for (list = locals; list; list = (void **)list[0]) {
            obj = list[1];
#ifdef RECOMP_NEGATIVE_CONTROL_CFUNC /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CFUNC */
            break; /* no unused-variable warnings */
#endif
            if (!(obj[0x16] & OBJECT_USED) && !IsTempName(F(obj, 8, void *)) &&
                !(F(obj, 0x10, UInt32) & Q_INLINE_DATA)) {
                CError_SetErrorToken(F(obj, 0x28, UInt8 *) + 8);
                CError_Warning(ERR_VARIABLE_ARGUMENT_NOT_USED_FUNCTION, F(obj, 8, UInt8 *) + 0xa);
            }
        }
    }
    if (warn_unusedarg) {
        for (list = arguments; list; list = (void **)list[0]) {
            obj = list[1];
            if (!(obj[0x16] & OBJECT_USED) && !IsTempName(F(obj, 8, void *)) &&
                F(obj, 8, void *) != this_name_node && F(obj, 8, void *) != self_name_node) {
                CError_SetErrorToken(symdecltoken);
                CError_Warning(ERR_VARIABLE_ARGUMENT_NOT_USED_FUNCTION, F(obj, 8, UInt8 *) + 0xa);
            }
        }
    }
}
HOOK(0x470d90, CFunc_WarnUnused);

void CFunc_CodeCleanup(void *stmt) {
    if (cscope_currentclass && F(cscope_currentclass, 0x22, void *))
        CSOM_InitSOMSelf(cscope_currentclass, stmt);
    CFunc_WarnUnused();
    CExcept_ExceptionTansform(stmt);
}
HOOK(0x470e60, CFunc_CodeCleanup);

void *CFunc_FuncGenSetup(UInt8 *stmt, void *func) {
    void *nspace;
    UInt8 *block;
    (void)func;

    nspace = CScope_NewListNameSpace(0, 0);
    F(nspace, 0, void *) = cscope_current; /* parent */
    cscope_current = nspace;
    arguments = 0;
    locals = 0;
    Labels = 0;
    localcount = 0;
    cfunc_staticvarcount = 0;
    CExcept_Setup();

    memclrw(stmt, 0x1e);
    curstmt = stmt;
    stmt[4] = 1; /* ST_NOP */
    curstmtvalue = 1;
    F(stmt, 8, SInt16) = (SInt16)curstmtvalue;

    blockcount = 0;
    block = lalloc(0xe);
    memclrw(block, 0xe);
    F(block, 0xc, SInt16) = blockcount++;
    F(block, 8, void *) = cscope_current;
    cfunc_currentblock = firstblock = block;
    return nspace;
}
HOOK(0x473db0, CFunc_FuncGenSetup);
