/*
 * CDecl.c — declarations of the C/C++ frontend (mweppc src/compiler/frontend/CDecl.c), partial
 * port.
 *
 * Ported so far: CDecl_NewTemplDepType and CDecl_AddArgument (byte-identical to mweppc's GameCube
 * 2.0 compiler) and CDecl_CompleteType (close: same logic). The declarator parser and the rest
 * stay the original's.
 *
 * Type kinds (byte +0): 5 class (flags (UInt16) +0x26: CLASS_COMPLETED 2, CLASS_IS_TEMPL_INST
 * 0x800), 9 template dependent (size +2, dtype (byte) +6; 0x10 bytes), 0xb pointer (target +6,
 * qual +0xa, Q_REFERENCE 0x20), 0xc array (target +6). TypeFunc.args +6; FuncArg: next +0, type
 * +0xc. stvoid 0x532088.
 */
#include "orig.h"

typedef UInt8 Bool8;

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void *, galloc, 0x4332e0, (SInt32 size));
ORIG_FN(void, memclrw, 0x4334d0, (void *ptr, SInt32 size));
ORIG_FN(void **, CParser_NewFuncArg, 0x42efa0, (void));
ORIG_FN(void, CTempl_InstantiateTemplateClass, 0x4eebe0, (void *tclass));

#define stvoid ((void *)0x532088)

void *CDecl_NewTemplDepType(UInt8 tdt) {
    UInt8 *t = galloc(0x10);
    memclrw(t, 0x10);
    t[0] = 9; /* TYPETEMPLATE */
    F(t, 2, SInt32) = 1;
    t[6] = tdt;
    return t;
}
HOOK(0x443210, CDecl_NewTemplDepType);

void CDecl_CompleteType(UInt8 *type) {
    switch (type[0]) {
    case 0xb: /* TYPEPOINTER */
        if ((F(type, 0xa, UInt32) & 0x20) && F(type, 6, UInt8 *)[0] == 5) {
            type = F(type, 6, UInt8 *);
            break;
        }
        return;
    case 0xc: /* TYPEARRAY */
        do
            type = F(type, 6, UInt8 *);
        while (type[0] == 0xc);
        if (type[0] == 5)
            break;
        return;
    case 5: /* TYPECLASS */
        break;
    default:
        return;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CDECL /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CDECL */
    return; /* template classes never completed here */
#endif
    if ((F(type, 0x26, UInt16) & 0x802) == 0x800)
        CTempl_InstantiateTemplateClass(type);
}
HOOK(0x4433d0, CDecl_CompleteType);

void CDecl_AddArgument(UInt8 *tfunc, void *argtype) {
    void **arg = CParser_NewFuncArg();
    F(arg, 0xc, void *) = argtype;
    arg[0] = F(tfunc, 6, void *);
    F(tfunc, 6, void *) = arg;
    if (arg[0] && F(arg[0], 0xc, void *) == stvoid)
        arg[0] = 0;
}
HOOK(0x443670, CDecl_AddArgument);

/* ---- part 2: CDecl_SetResultReg (empty in ours) ---- */
void CDecl_SetResultReg(void *tfunc) {
}
HOOK(0x443240, CDecl_SetResultReg);

/* ---- part 3: CDecl_ExtractClassExportFlags, CDecl_MakeFunctionVirtual, getaccesstype (not
   matched by gcmatch; the same logic as mweppc's) ---- */

/* DeclInfo.exportflags (byte) +0x43 (INTERNAL 0x10, IMPORT 0x20, EXPORT 0x40). TypeClass.mode
   (byte) +0x28 (CLASS_MODE_UNION 1). AccessType: PUBLIC 0, PRIVATE 1, PROTECTED 2, NONE 3. */
ORIG_FN(void, CError_Error, 0x456d50, (int code, ...));
ORIG_FN(int, is_pascal_object, 0x430730, (void *obj));

void CDecl_ExtractClassExportFlags(UInt8 *declinfo, UInt8 flags) {
    if (flags & 1)
        declinfo[0x43] |= 0x10;
    if (flags & 2)
        declinfo[0x43] |= 0x20;
    if (flags & 4)
        declinfo[0x43] |= 0x40;
}
HOOK(0x447c30, CDecl_ExtractClassExportFlags);

void CDecl_MakeFunctionVirtual(UInt8 *tclass, UInt8 *func) {
    if (is_pascal_object(func))
        CError_Error(0xdb);
    if (tclass[0x28] == 1)
        CError_Error(0x160, func);
#ifdef RECOMP_NEGATIVE_CONTROL_CDECL3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CDECL3 */
    return; /* virtual functions stay plain functions */
#endif
    func[2] = 4; /* DVFUNC */
}
HOOK(0x447ce0, CDecl_MakeFunctionVirtual);

UInt8 getaccesstype(UInt8 a, UInt8 b, UInt8 c) {
    if (a == 3 || b == 3 || b == 1)
        return 3;
    if (c == 0 && b != 0)
        return 3;
    return 0;
}
HOOK(0x449a70, getaccesstype);

/* ---- part 4: CDecl_NewPointerType and the pointer size (not matched by gcmatch) ---- */

/* TypePointer (0xe bytes): type 0xb, size +2, target +6. */
SInt32 CDecl_PointerSize(void *target) {
    return 4;
}
HOOK(0x4431c0, CDecl_PointerSize);

void *CDecl_NewPointerType(void *target) {
    UInt8 *t = galloc(0xe);
    memclrw(t, 0xe);
    t[0] = 0xb; /* TYPEPOINTER */
    F(t, 2, SInt32) = CDecl_PointerSize(target);
    F(t, 6, void *) = target;
    return t;
}
HOOK(0x443190, CDecl_NewPointerType);

/* ---- part 5: a member function type maker (from the exe) ---- */

/* A TypeFunc (0x28 bytes, kind 6) for a member of tclass: result 0x4431d0(tclass), flags 0x110,
   class +0x1a, one argument of type 0x4431d0(tclass) (its +0x10 set when flag is set);
   0x443260 (type, 1) and 0x443580 (type, tclass) finish it. */
ORIG_FN(void *, fn_4431d0, 0x4431d0, (void *tclass));
ORIG_FN(void, fn_443260, 0x443260, (void *type, SInt32 x));
ORIG_FN(void, fn_443580, 0x443580, (void *type, void *tclass));

UInt8 *fn_449df0(void *tclass, Bool8 flag) {
    UInt8 *tfunc = galloc(0x28);
    UInt8 *arg;

    memclrw(tfunc, 0x28);
    tfunc[0] = 6;
    F(tfunc, 0xe, void *) = fn_4431d0(tclass);
    F(tfunc, 0x16, UInt32) = 0x110;
    F(tfunc, 0x1a, void *) = tclass;
    fn_443260(tfunc, 1);
    arg = (UInt8 *)CParser_NewFuncArg();
    if (flag)
        F(arg, 0x10, UInt32) = 1;
    F(arg, 0xc, void *) = fn_4431d0(tclass);
    F(tfunc, 6, void *) = arg;
    fn_443580(tfunc, tclass);
    return tfunc;
}
HOOK(0x449df0, fn_449df0);

/* a new struct type (TypeStruct 0x12 bytes, kind 4, align 1, stype), named in the root scope
   when 0x57e141 is set in C, else in the current scope (0x44dda0) */
ORIG_FN(void, fn_44dda0, 0x44dda0, (void *scope, void *name, void *type));

UInt8 *fn_4478c0(void *name, UInt8 stype) {
    UInt8 *tstruct = galloc(0x12);
    void *scope;

    memclrw(tstruct, 0x12);
    tstruct[0] = 4;
    F(tstruct, 0x10, SInt16) = 1;
    tstruct[0xe] = stype;
#ifdef RECOMP_NEGATIVE_CONTROL_CDECL5 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CDECL5 */
    name = 0; /* the tag is never declared */
#endif
    if (name) {
        F(tstruct, 6, void *) = name;
        if (*(Bool8 *)0x57e141 && !*(Bool8 *)0x57db79)
            scope = *(void **)0x57c900; /* cscope_root */
        else
            scope = *(void **)0x57d6bc; /* cscope_current */
        fn_44dda0(scope, name, tstruct);
    }
    return tstruct;
}
HOOK(0x4478c0, fn_4478c0);

/* ---- part 6: a member object maker (from the exe) ---- */

/* a new object (0x42ef20) named name with type and access, in the class's namespace (+6), with
   qual 0x80000 | 0x10, added to it (0x44b500 (nspace, name, obj)) */
ORIG_FN(UInt8 *, CParser_NewObject_, 0x42ef20, (void));
ORIG_FN(void, fn_44b500, 0x44b500, (void *nspace, void *name, void *obj));

UInt8 *fn_449b10(UInt8 *tclass, void *name, void *type, UInt8 access) {
    UInt8 *obj = CParser_NewObject_();

    F(obj, 8, void *) = name;
    F(obj, 0xc, void *) = type;
    F(obj, 0x10, UInt32) = 0x80000;
    obj[1] = access;
#ifdef RECOMP_NEGATIVE_CONTROL_CDECL6 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CDECL6 */
    obj[1] ^= 1;
#endif
    F(obj, 4, void *) = F(tclass, 6, void *);
    F(obj, 0x10, UInt32) |= 0x10;
    fn_44b500(F(tclass, 6, void *), F(obj, 8, void *), obj);
    return obj;
}
HOOK(0x449b10, fn_449b10);

/* a lone argument of the given type (error 0xef, made void: stvoid 0x532088), unless another
   argument with +8 clear follows */
void fn_448700(UInt8 *arg, void *type) {
    UInt8 *next;

    if (!arg || F(arg, 0xc, void *) != type)
        return;
    next = F(arg, 0, UInt8 *);
    if (next && !F(next, 8, void *))
        return;
    CError_Error(0xef);
    F(arg, 0xc, void *) = (void *)0x532088;
}
HOOK(0x448700, fn_448700);
