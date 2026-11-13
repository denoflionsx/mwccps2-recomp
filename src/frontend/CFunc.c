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

/* ---- part 6: is the next token a ':' (a label)? (from the disassembly: lookahead keeps
   tkidentifier and tksize) ---- */
ORIG_FN(SInt16, lookahead, 0x47b890, (void));

int CFunc_IsLabel(void) {
    void *name = *(void **)0x57c8dc; /* tkidentifier */
    SInt16 size = *(SInt16 *)0x57c9ec; /* tksize */
    SInt16 t = lookahead();

    *(void **)0x57c8dc = name;
    *(SInt32 *)0x57c9ec = size;
#ifdef RECOMP_NEGATIVE_CONTROL_CFUNC6 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CFUNC6 */
    return 0; /* no labels */
#endif
    return t == ':';
}
HOOK(0x472490, CFunc_IsLabel);

/* ---- part 7: the returned-address check (from the exe; mweppc's logic) ---- */

/* warns (0x146: address of a local returned) for an EOBJREF of a local (DLOCAL 1) or an ETEMP,
   through commas (right side) and EADD/ESUB (both sides) */
void CFunc_CheckReturnAddress(UInt8 *expr) {
    for (;;) {
        while (expr[0] == 0x29)
            expr = F(expr, 0x1c, UInt8 *);
        switch (expr[0]) {
        case 0xf:
        case 0x10:
            CFunc_CheckReturnAddress(F(expr, 0x18, UInt8 *));
            expr = F(expr, 0x1c, UInt8 *);
            continue;
        case 0x38:
            if (F(expr, 0x18, UInt8 *)[2] != 1)
                return;
            /* fall through */
        case 0x3c:
#ifndef RECOMP_NEGATIVE_CONTROL_CFUNC7 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CFUNC7 */
            CError_Warning(0x146);
#endif
            return;
        }
        return;
    }
}
HOOK(0x4725c0, CFunc_CheckReturnAddress);

/* ---- part 8: a statement wrapper (from the exe) ---- */

ORIG_FN(SInt32, fn_473ef0, 0x473ef0, (void *x, SInt32 flag));

SInt32 fn_473fb0(void *x) {
    return fn_473ef0(x, 0);
}
HOOK(0x473fb0, fn_473fb0);

/* ---- part 9: local variable info, parameter types, labels and list helpers (from the exe;
   mweppc's names where it has the function) ---- */

/* VarInfo (0x34 bytes): func +0, usage +4, the stream element at the declaration +8 (0x20 bytes),
   local index (short) +0x28, noregister +0x2a. Object: type +0xc, sclass (short) +0x14 (TK_REGISTER
   0x101), var info +0x28. cscope_currentfunc 0x57d6b0, optimization level 0x57dbe6. */
ORIG_FN(void *, CPrep_CurStreamElement_f, 0x43c390, (void));
ORIG_FN(Bool8, is_volatile_object_f, 0x4305f0, (void *obj));
ORIG_FN(void *, CDecl_NewPointerType_f, 0x443190, (void *type));
ORIG_FN(void *, CDecl_MakePtrType_f, 0x443560, (void **ptype, UInt32 qual));
ORIG_FN(Bool8, fn_443520_f, 0x443520, (void *type));

UInt8 *CodeGen_GetNewVarInfo(void) {
    UInt8 *info = lalloc(0x34);
    UInt32 *elem;
    int i;

    memclrw(info, 0x34);
    elem = CPrep_CurStreamElement_f();
    for (i = 0; i < 8; i++)
        ((UInt32 *)(info + 8))[i] = elem[i];
    F(info, 0x28, SInt16) = localcount++;
    return info;
}
HOOK(0x4337e0, CodeGen_GetNewVarInfo);

void CFunc_SetupLocalVarInfo_(UInt8 *obj) {
    F(obj, 0x28, UInt8 *) = CodeGen_GetNewVarInfo();
    F(F(obj, 0x28, UInt8 *), 0, void *) = ORIG_VAR(void *, 0x57d6b0);
    if (F(obj, 0x14, SInt16) == 0x101) {
        if (!ORIG_VAR(Bool8, 0x57dbe6))
            F(F(obj, 0x28, UInt8 *), 4, SInt32) = 100;
        else
            F(F(obj, 0x28, UInt8 *), 4, SInt32) = 5;
    }
    if (F(obj, 0xc, void *) && is_volatile_object_f(obj))
        F(obj, 0x28, UInt8 *)[0x2a] = 1;
}
HOOK(0x4704d0, CFunc_SetupLocalVarInfo_);

/* a parameter's type: arrays and functions become pointers; classes with +0x22 set (0x11c) and
   types fn_443520 refuses become int (stsignedint 0x536060) */
void *fn_470530(void **ptype) {
    UInt8 *t = *ptype;

    switch (t[0]) {
    case 5:
        if (F(t, 0x22, void *)) {
            CError_Error(0x11c);
            *ptype = (void *)0x536060;
        }
        break;
    case 6:
        return CDecl_MakePtrType_f(ptype, 0);
    case 9:
        if (t[6] == 3)
            *ptype = CDecl_NewPointerType_f(F(t, 8, void *));
        return *ptype;
    case 0xc:
        *ptype = CDecl_NewPointerType_f(F(t, 6, void *));
        return *ptype;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CFUNC8 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CFUNC8 */
    if (((UInt8 *)*ptype)[0] == 0xb) /* pointer parameters become int */
        *ptype = (void *)0x536060;
#endif
    if (!fn_443520_f(*ptype)) {
        *ptype = (void *)0x536060;
        return 0;
    }
    return (void *)1;
}
HOOK(0x470530, fn_470530);

/* the list node (next +0) whose +4 is key */
void **fn_4705b0(void **list, void *key) {
    for (; list; list = list[0]) {
        if (list[1] == key)
            return list;
    }
    return 0;
}
HOOK(0x4705b0, fn_4705b0);

/* appends item to the list at *head (next +0) */
void *fn_470600(void **head, void *item) {
    void **last = *head;

    if (!last) {
        *head = item;
        return item;
    }
    while (last[0])
        last = last[0];
    last[0] = item;
    return item;
}
HOOK(0x470600, fn_470600);

/* labels used but never defined (no statement +4): error 0x9f with the name (+0xc) */
void CFunc_CheckLabels(void) {
    UInt8 *label;

    for (label = Labels; label; label = F(label, 0, UInt8 *)) {
        if (!F(label, 4, void *))
            CError_Error(0x9f, F(label, 0xc, UInt8 *) + 0xa);
    }
}
HOOK(0x470ca0, CFunc_CheckLabels);

/* ---- part 10: conditional jumps and the inline-function wrapper (from the exe) ---- */

ORIG_FN(void, CExpr_SearchExprTree_f, 0x4ef190, (void *expr, void *cb, int count, ...));
ORIG_FN(void, fn_471e80_f, 0x471e80, (void *expr, void *label, void *falselabel, UInt8 a, UInt8 b));
ORIG_FN(void, fn_465260_f, 0x465260, (UInt8 *buf, void *a, void *obj));
ORIG_FN(void, fn_466130_f, 0x466130, (void *a, void *obj, void *c));
ORIG_FN(void, fn_45ff80_f, 0x45ff80, (void *obj, UInt8 *buf));

/* A conditional jump to label on expr (true when jumpIfTrue): in C++ with copts 0x57dbd6, an
   expression the walk with callback 0x4720c0 (it sets 0x54d760 for nodes with +0x20 or +0x1c set)
   flags becomes one statement of kind 6 (true) or 7 (expr +0xa, label +0xe, flag 4 at +6);
   otherwise 0x471e80 lowers it with a new false label and a label statement (kind 2) follows. */
void *fn_4720e0(UInt8 jumpIfTrue, void *expr, void *label, UInt8 flag) {
    UInt8 *stmt;
    UInt8 *falselabel;

    if (expr && *(Bool8 *)0x57db79 && *(Bool8 *)0x57dbd6) {
        *(Bool8 *)0x54d760 = 0;
        CExpr_SearchExprTree_f(expr, (void *)0x4720c0, 1, 0x3c);
        if (*(Bool8 *)0x54d760) {
            stmt = CFunc_AppendStatement(jumpIfTrue ? 6 : 7);
            F(stmt, 0xe, void *) = label;
            F(stmt, 0xa, void *) = expr;
            if (flag)
                stmt[6] |= 4;
            return stmt;
        }
    }
    falselabel = newlabel();
#ifdef RECOMP_NEGATIVE_CONTROL_CFUNC9 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CFUNC9 */
    jumpIfTrue = !jumpIfTrue; /* conditions inverted */
#endif
    fn_471e80_f(expr, label, falselabel, jumpIfTrue, flag);
    stmt = CFunc_AppendStatement(2);
    F(falselabel, 4, void *) = stmt;
    F(stmt, 0xe, void *) = falselabel;
    return stmt;
}
HOOK(0x4720e0, fn_4720e0);

/* calls 0x466130 (a, obj, c); for an inline function (type +0xc, flags +0x16 & 0x400000) while
   0x57e03b is clear, 0x465260 saves state into a 0x68-byte block first and 0x45ff80 restores it */
void fn_473e80(void *a, UInt8 *obj, void *c) {
    UInt8 buf[0x68];
    Bool8 saved = 0;

    if ((F(F(obj, 0xc, UInt8 *), 0x16, UInt32) & 0x400000) && !*(Bool8 *)0x57e03b) {
        fn_465260_f(buf, a, obj);
        saved = 1;
    }
    fn_466130_f(a, obj, c);
    if (saved)
        fn_45ff80_f(obj, buf);
}
HOOK(0x473e80, fn_473e80);

/* ---- part 11: parameter lists, loop weights and block restore (mweppc's logic where it has the
   function, else from the exe) ---- */

ORIG_FN(Bool8, isdeclaration_f, 0x4300e0, (UInt8 a, UInt8 b, UInt8 c, SInt16 token));
ORIG_FN(UInt8 *, CScope_NewListNameSpace_f, 0x44b3f0, (void *name, Bool8 is_global));
ORIG_FN(void *, parameter_list_f, 0x4707b0, (UInt8 *declinfo));
ORIG_FN(Bool8, CScope_IsEmptyNameSpace_f, 0x44b480, (void *nspace));
ORIG_FN(void, identifier_list_f, 0x470630, (UInt8 *declinfo));
ORIG_FN(void, CError_Error_f, 0x456d50, (int code, ...));
ORIG_FN(void, CParser_GetDeclSpecs_f, 0x431360, (UInt8 *di, Bool8 flag));
ORIG_FN(void, declarator_f, 0x444790, (UInt8 *di));
ORIG_FN(void *, fn_46fb90_f, 0x46fb90, (void));
ORIG_FN(void *, nullnode_f, 0x4ef2b0, (void));
#define tk_f ORIG_VAR(SInt16, 0x57d73a)
#define in_func_arglist ORIG_VAR(Bool8, 0x57e141)

/* DeclInfo: hasParameterNames +0x44, oldStyleParameters +0x45, parameterScope +0x1c; TK_ELLIPSIS
   0x170; oldstyle 0x56c6f0 */
void *parameter_type_list(UInt8 *declinfo) {
    void *args;
    UInt8 *nspace;
    Bool8 save;

    declinfo[0x44] = 0;
    declinfo[0x45] = 0;
    F(declinfo, 0x1c, void *) = 0;
    if (tk_f == 0x170 || isdeclaration_f(0, 0, 0, 0)) {
        if (!*(Bool8 *)0x57db79) {
            nspace = CScope_NewListNameSpace_f(0, 0);
            F(nspace, 0, void *) = cscope_current;
            cscope_current = nspace;
            save = in_func_arglist;
            in_func_arglist = 1;
            args = parameter_list_f(declinfo);
            in_func_arglist = save;
            cscope_current = F(nspace, 0, void *);
            if (!CScope_IsEmptyNameSpace_f(nspace))
                F(declinfo, 0x1c, void *) = nspace;
        } else {
            args = parameter_list_f(declinfo);
        }
    } else if (*(Bool8 *)0x57db79) {
        args = 0;
        if (tk_f != ')')
            CError_Error_f(0x7f);
    } else {
        identifier_list_f(declinfo);
        args = (void *)0x56c6f0;
    }
    return args;
}
HOOK(0x470ab0, parameter_type_list);

/* the statement weight (curstmtvalue) entering and leaving a loop */
void CFunc_LoopIncrement(void) {
    if (curstmtvalue < 0x1000)
        curstmtvalue <<= 3;
    else if (curstmtvalue < 0xf000)
        curstmtvalue += 0x1000;
    else
        curstmtvalue++;
}
HOOK(0x470380, CFunc_LoopIncrement);

void CFunc_LoopDecrement(void) {
#ifndef RECOMP_NEGATIVE_CONTROL_CFUNC10 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CFUNC10 */
    if (curstmtvalue <= 0x1000)
        curstmtvalue >>= 3;
    else if (curstmtvalue <= 0xf000)
        curstmtvalue -= 0x1000;
    else
        curstmtvalue--;
#endif
    if (curstmtvalue < 1)
        curstmtvalue = 1;
}
HOOK(0x4703c0, CFunc_LoopDecrement);

/* leaving a block (DeclBlock: dobjstack +4, scope +8): an object-destroy statement (kind 4 with a
   null expression +0xa) when the current statement's (+0x12) objects differ, then the block's
   scope and objects come back */
void CFunc_RestoreBlock_(UInt8 *block) {
    UInt8 *stmt;

    if (curstmt && F(curstmt, 0x12, void *) != cexcept_dobjstack) {
        stmt = CFunc_AppendStatement(4);
        F(stmt, 0xa, void *) = nullnode_f();
    }
    cscope_current = F(block, 8, void *);
    cexcept_dobjstack = F(block, 4, void *);
}
HOOK(0x470480, CFunc_RestoreBlock_);

/* whether a parenthesized list looks like a parameter list: "..." or declarations (each optionally
   "= expression"), separated by ',' and ending in "..." or ')'; with stopAtComma the first ','
   is enough */
Bool8 fn_4709f0(Bool8 stopAtComma) {
    UInt8 di[0x68];

    if (tk_f == 0x170)
        return 1;
    for (;;) {
        memclrw(di, 0x66);
        CParser_GetDeclSpecs_f(di, 0);
        if (di[0x48])
            return 0;
        declarator_f(di);
        if (tk_f == '=') {
            tk_f = lex_f();
            fn_46fb90_f();
        }
        if (tk_f == ',') {
            if (stopAtComma)
                return 1;
            tk_f = lex_f();
            if (tk_f == 0x170)
                return 1;
            continue;
        }
        return tk_f == 0x170;
    }
}
HOOK(0x4709f0, fn_4709f0);
