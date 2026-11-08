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

/* ---- part 2: CFunc_NewDeclBlock (close to mweppc: same logic) ---- */

/* DeclBlock (0xe bytes): next +0, dobjstack +4, parent_nspace +8, index (short) +0xc.
   NameSpace.parent +0. */
void *CFunc_NewDeclBlock(void) {
    UInt8 *block = lalloc(0xe);
    void **nspace;

    if (firstblock) {
        F(cfunc_currentblock, 0, void *) = block;
        cfunc_currentblock = block;
    } else {
        cfunc_currentblock = firstblock = block;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CFUNC2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CFUNC2 */
    F(block, 0xc, SInt16) = 0; /* every block index 0 */
    blockcount++;
#else
    F(block, 0xc, SInt16) = blockcount++;
#endif
    F(block, 8, void *) = cscope_current;
    F(block, 4, void *) = cexcept_dobjstack;
    nspace = CScope_NewListNameSpace(0, 0);
    nspace[0] = cscope_current;
    cscope_current = nspace;
    return block;
}
HOOK(0x470410, CFunc_NewDeclBlock);

/* ---- part 3: CFunc_FunctionRedefinedCheck (not matched by gcmatch; the same logic as mweppc's) ---- */

/* TypeFunc.flags +0x16: FUNC_DEFINED 2, FUNC_AUTO_GENERATED 0x100. DINLINEFUNC 5. */
ORIG_FN(void, CError_Error, 0x456d50, (int code, ...));

void CFunc_FunctionRedefinedCheck(UInt8 *func) {
    if (F(F(func, 0xc, UInt8 *), 0x16, UInt32) & 0x100)
        CError_Error(0x14d, func); /* object redefined */
    if ((F(F(func, 0xc, UInt8 *), 0x16, UInt32) & 2) && func[2] != 5)
        CError_Error(0x14d, func);
#ifndef RECOMP_NEGATIVE_CONTROL_CFUNC3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CFUNC3 */
    F(F(func, 0xc, UInt8 *), 0x16, UInt32) |= 2;
#endif
}
HOOK(0x474430, CFunc_FunctionRedefinedCheck);

/* ---- part 4: CFunc_DeclareFuncName (not matched by gcmatch; the same logic as mweppc's) ---- */

/* DeclInfo (0x66 bytes): thetype +0, qual +4, name +0xc, storageclass (short) +0x40. stchar
   0x536030, cscope_root 0x57c900. */
ORIG_FN(void *, CDecl_NewArrayType, 0x443150, (void *type, SInt32 size));
ORIG_FN(UInt8 *, CParser_NewGlobalDataObject, 0x42edf0, (void *declinfo));
ORIG_FN(void *, CParser_NewAliasObject, 0x42ef50, (void *obj, SInt32 offset));
ORIG_FN(void, CFunc_NameLocalStaticDataObject, 0x471630, (void *obj, const char *name));
ORIG_FN(UInt32, strlen_, 0x404ee0, (const char *s));

void *CFunc_DeclareFuncName(const char *str, void *name) {
    UInt8 di[0x66];
    UInt8 *obj;
    void *aliasobj;

    memclrw(di, sizeof(di));
    F(di, 0xc, void *) = name;
    F(di, 0x40, SInt16) = 0x102; /* TK_STATIC */
    F(di, 4, UInt32) = 1; /* Q_CONST */
    F(di, 0, void *) = CDecl_NewArrayType((void *)0x536030 /* stchar */, strlen_(str) + 1);
    obj = CParser_NewGlobalDataObject(di);
    aliasobj = CParser_NewAliasObject(obj, 0);
    F(obj, 4, void *) = *(void **)0x57c900; /* cscope_root */
    obj[2] = 0; /* DDATA */
#ifdef RECOMP_NEGATIVE_CONTROL_CFUNC4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CFUNC4 */
    return obj;
#endif
    CFunc_NameLocalStaticDataObject(obj, (const char *)(F(obj, 8, UInt8 *) + 0xa));
    return aliasobj;
}
HOOK(0x474480, CFunc_DeclareFuncName);

/* A braced block or a single statement in a new declaration block (from the disassembly; in C, a
   block may start with declarations). isdeclaration 0x4300e0, CFunc_ParseLocalDeclarationList
   0x471c50, CFunc_Statement 0x472620, CPrep_NewFileOffsetInfo 0x43cb20, CFunc_RestoreBlock
   0x470480. cparser_fileoffset 0x57dc2c, sourceoffset 0x57ceb4, sourcefilepath 0x57b6ec. */
ORIG_FN(Bool8, isdeclaration, 0x4300e0, (Bool8 a, Bool8 b, Bool8 c, SInt16 t));
ORIG_FN(void *, CFunc_ParseLocalDeclarationList, 0x471c50, (Bool8 a, Bool8 b, Bool8 c, Bool8 d));
ORIG_FN(void, CFunc_Statement, 0x472620, (void *thing));
ORIG_FN(void, CPrep_NewFileOffsetInfo, 0x43cb20, (void *foi, void *offset, void *path));
ORIG_FN(void, CFunc_RestoreBlock, 0x470480, (void *block));
ORIG_FN(SInt16, lex_f, 0x47bb00, (void));
#define tk_f ORIG_VAR(SInt16, 0x57d73a)

void CFunc_BlockStatement(void *thing) {
    void *block = CFunc_NewDeclBlock();

    if (tk_f == '{') {
        tk_f = lex_f();
        if (!*(Bool8 *)0x57db79 /* cplusplus */ && isdeclaration(0, 0, 0, 0))
            CFunc_ParseLocalDeclarationList(0, 0, 0, 0);
        while (tk_f != '}')
            CFunc_Statement(thing);
        CPrep_NewFileOffsetInfo((void *)0x57dc2c, (void *)0x57ceb4, (void *)0x57b6ec);
        tk_f = lex_f();
    } else {
        CFunc_Statement(thing);
    }
    CFunc_RestoreBlock(block);
}
HOOK(0x473780, CFunc_BlockStatement);
