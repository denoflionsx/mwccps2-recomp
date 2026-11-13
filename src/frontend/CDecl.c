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

/* ---- part 4: array types and the object checks (from the exe; mweppc's CanCreateObject and
   CanAllocObject with our error numbers) ---- */

/* TypePointer with kind TYPEARRAY 0xc: size +2, target +6, qual +0xa */
void *CDecl_NewArrayType(void *target, SInt32 size) {
    UInt8 *t = galloc(0xe);
    memclrw(t, 0xe);
    t[0] = 0xc;
    F(t, 2, SInt32) = size;
    F(t, 6, void *) = target;
    F(t, 0xa, UInt32) = 0;
    return t;
}
HOOK(0x443150, CDecl_NewArrayType);

/* TypeClass flags +0x26: CLASS_COMPLETED 2, abstract 8, template instance 0x800, 1 (0xbf) */
ORIG_FN(void, CError_AbstractClassError, 0x457180, (void *tclass));
ORIG_FN(Bool8, CTempl_InstantiateTemplateClass_b, 0x4eebe0, (void *tclass));

/* whether an object of the type can be declared: void (0x7e) and functions (0x92) never; structs
   (0x88) and classes when complete (or a template class that instantiates); everything else when it
   has a size (0x91) */
Bool8 CanCreateObject(UInt8 *type) {
    switch (type[0]) {
    case 0: /* TYPEVOID */
        CError_Error(0x7e);
        return 0;
    case 6: /* TYPEFUNC */
        CError_Error(0x92);
        return 0;
    case 4: /* TYPESTRUCT */
        if (!F(type, 2, SInt32)) {
            CError_Error(0x88, type, 0);
            return 0;
        }
        return 1;
    case 5: /* TYPECLASS */
        if (F(type, 0x26, UInt16) & 2)
            return 1;
        if ((F(type, 0x26, UInt16) & 0x800) && CTempl_InstantiateTemplateClass_b(type))
            return 1;
        CError_Error(0x88, type, 0);
        return 0;
    default:
#ifdef RECOMP_NEGATIVE_CONTROL_CDECL7 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CDECL7 */
        if (F(type, 2, SInt32) > 4) { /* larger scalars and arrays refused */
#else
        if (!F(type, 2, SInt32)) {
#endif
            CError_Error(0x91);
            return 0;
        }
        return 1;
    }
}
HOOK(0x443420, CanCreateObject);

Bool8 CanAllocObject(UInt8 *type) {
    switch (type[0]) {
    case 0:
        CError_Error(0x7e);
        return 0;
    case 6:
        CError_Error(0x92);
        return 0;
    case 5:
        if (F(type, 0x26, UInt16) & 8) {
            CError_AbstractClassError(type);
            return 0;
        }
        return 1;
    default:
        return 1;
    }
}
HOOK(0x4434d0, CanAllocObject);

Bool8 fn_443520(UInt8 *type) {
    if (!CanAllocObject(type))
        return 0;
    if (type[0] == 5 && (F(type, 0x26, UInt16) & 1)) {
        CError_Error(0xbf);
        return 0;
    }
    return 1;
}
HOOK(0x443520, fn_443520);

/* a function result type: void is fine; a class with +0x22 set is refused (0x11b) */
Bool8 fn_443700(UInt8 *type) {
    if (!type[0])
        return 1;
    if (type[0] == 5 && F(type, 0x22, void *)) {
        CError_Error(0x11b);
        return 0;
    }
    return fn_443520(type);
}
HOOK(0x443700, fn_443700);

/* the top bit of the first word */
int fn_443ac0(const UInt32 *x) {
    return (x[0] & 0x80000000) != 0;
}
HOOK(0x443ac0, fn_443ac0);

/* default arguments must run to the end of the list (elipsis 0x56a818, oldstyle 0x56c6f0): after
   the first FuncArg with one (dexpr +8), one without clears them all with error 0xcd */
UInt8 *CDecl_CheckDefaultArgs(UInt8 *args) {
    UInt8 *arg = args;
    UInt8 *scan;

    while (arg && !F(arg, 8, void *))
        arg = F(arg, 0, UInt8 *);
    for (; arg && arg != (UInt8 *)0x56a818 && arg != (UInt8 *)0x56c6f0; arg = F(arg, 0, UInt8 *)) {
        if (!F(arg, 8, void *)) {
            for (scan = args; scan; scan = F(scan, 0, UInt8 *))
                F(scan, 8, void *) = 0;
            CError_Error(0xcd);
            return args;
        }
    }
    return args;
}
HOOK(0x444f20, CDecl_CheckDefaultArgs);

/* ---- part 8: the implicit constructor and destructor types (from the exe; mweppc's
   CDecl_MakeDefaultCtorType family) ---- */

/* A void (stvoid 0x532090) TypeFunc (0x28 bytes, kind 6) of class tclass (+0x1a) with flags
   0x1110 (constructor) or 0x2110 (destructor); a class with virtual bases (+0x26 & 0x20) or a
   flagged destructor gets the hidden argument of type 0x536050. */
static UInt8 *new_special_func(void *tclass, UInt32 flags) {
    UInt8 *tfunc = galloc(0x28);

    memclrw(tfunc, 0x28);
    tfunc[0] = 6;
    F(tfunc, 0xe, void *) = (void *)0x532090;
    F(tfunc, 0x16, UInt32) = flags;
    F(tfunc, 0x1a, void *) = tclass;
    fn_443260(tfunc, 1);
    return tfunc;
}

UInt8 *CDecl_MakeDefaultCtorType(UInt8 *tclass) {
    UInt8 *tfunc = new_special_func(tclass, 0x1110);

    if (F(tclass, 0x26, UInt16) & 0x20)
        CDecl_AddArgument(tfunc, (void *)0x536050);
    fn_443580(tfunc, tclass);
    return tfunc;
}
HOOK(0x449aa0, CDecl_MakeDefaultCtorType);

UInt8 *CDecl_MakeCopyCtorType(UInt8 *tclass, Bool8 is_const) {
    UInt8 *tfunc = new_special_func(tclass, 0x1110);
    UInt8 *arg = (UInt8 *)CParser_NewFuncArg();

    if (is_const)
        F(arg, 0x10, UInt32) = 1;
    F(arg, 0xc, void *) = fn_4431d0(tclass);
    F(tfunc, 6, void *) = arg;
    if (F(tclass, 0x26, UInt16) & 0x20)
        CDecl_AddArgument(tfunc, (void *)0x536050);
    fn_443580(tfunc, tclass);
    return tfunc;
}
HOOK(0x449c10, CDecl_MakeCopyCtorType);

UInt8 *CDecl_MakeDefaultDtorType(UInt8 *tclass, Bool8 is_virtual) {
#ifdef RECOMP_NEGATIVE_CONTROL_CDECL8 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CDECL8 */
    UInt8 *tfunc = new_special_func(tclass, 0x1110); /* destructors typed as constructors */
#else
    UInt8 *tfunc = new_special_func(tclass, 0x2110);
#endif

    if (is_virtual)
        CDecl_AddArgument(tfunc, (void *)0x536050);
    fn_443580(tfunc, tclass);
    return tfunc;
}
HOOK(0x449fd0, CDecl_MakeDefaultDtorType);

/* ---- part 9: the implicit default constructor and destructor (mweppc's logic) ---- */

/* TypeClass: bases +0xe (next +0, base +4), ivars +0x16 (next +4, type +0xc), flags +0x26 (0x20
   virtual bases). Object: access +1, datatype +2 (DVFUNC 4). DeclE: +0xc. DeclInfo (0x66 bytes
   cleared): thetype +0, qual +4 (Q_INLINE 0x10, Q_VIRTUAL 0x100), name +0xc. constructor and
   destructor name nodes 0x57b718 / 0x57ab54; CDecl_AddFunctionMember 0x447d20. */
ORIG_FN(UInt8 *, CClass_Constructor_d, 0x45edd0, (void *tclass));
ORIG_FN(UInt8 *, CClass_Destructor_d, 0x45ee10, (void *tclass));
ORIG_FN(void, CDecl_AddFunctionMember_d, 0x447d20,
        (void *decle, void *tclass, UInt8 *di, UInt8 access, Bool8 a, Bool8 b, Bool8 c, Bool8 d));

static UInt8 *class_below_arrays(UInt8 *type) {
    while (type[0] == 0xc)
        type = F(type, 6, UInt8 *);
    return type[0] == 5 ? type : 0;
}

void CDecl_AddDefaultConstructor(UInt8 *decle, UInt8 *tclass) {
    Bool8 has_ctor;
    void **base;
    UInt8 *ivar, *t;

    if (CClass_Constructor_d(tclass))
        return;
    has_ctor = (F(tclass, 0x26, UInt16) & 0x20) || decle[0xc];
    for (base = F(tclass, 0xe, void **); base; base = (void **)base[0]) {
        if (CClass_Constructor_d(base[1]))
            has_ctor = 1;
    }
    for (ivar = F(tclass, 0x16, UInt8 *); ivar; ivar = F(ivar, 4, UInt8 *)) {
        if ((t = class_below_arrays(F(ivar, 0xc, UInt8 *))) && CClass_Constructor_d(t))
            has_ctor = 1;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CDECL9 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CDECL9 */
    has_ctor = 0; /* implicit default constructors never declared */
#endif
    if (has_ctor)
        fn_449b10(tclass, *(void **)0x57b718, CDecl_MakeDefaultCtorType(tclass), 0);
}
HOOK(0x449b60, CDecl_AddDefaultConstructor);

void CDecl_AddDefaultDestructor(void *decle, UInt8 *tclass) {
    UInt8 di[0x68];
    Bool8 has_dtors = 0, is_virtual = 0;
    UInt8 access = 0;
    void **base;
    UInt8 *ivar, *t, *dtor;

    if (CClass_Destructor_d(tclass))
        return;
    for (base = F(tclass, 0xe, void **); base; base = (void **)base[0]) {
        if ((dtor = CClass_Destructor_d(base[1]))) {
            has_dtors = 1;
            if (dtor[2] == 4)
                is_virtual = 1;
            access = getaccesstype(access, dtor[1], 1);
        }
    }
    for (ivar = F(tclass, 0x16, UInt8 *); ivar; ivar = F(ivar, 4, UInt8 *)) {
        if ((t = class_below_arrays(F(ivar, 0xc, UInt8 *))) && (dtor = CClass_Destructor_d(t))) {
            has_dtors = 1;
            access = getaccesstype(access, dtor[1], 0);
        }
    }
    if (has_dtors) {
        memclrw(di, 0x66);
        F(di, 4, UInt32) |= 0x10;
        if (is_virtual)
            F(di, 4, UInt32) |= 0x100;
        F(di, 0, void *) = CDecl_MakeDefaultDtorType(tclass, 1);
        F(di, 0xc, void *) = *(void **)0x57ab54;
        CDecl_AddFunctionMember_d(decle, tclass, di, access, 1, 0, 0, 0);
    }
}
HOOK(0x44a030, CDecl_AddDefaultDestructor);

/* ---- part 10: completing a class definition (mweppc's CDecl_CompleteClass, from the exe) ---- */

/* DeclE +0xc: a base has a vtable. Non-SOM classes (+0x22) get the implicit destructor,
   assignment (0x449e60), default constructor and copy constructor; then the layout (0x44a150 with a
   0x80-byte work area), the vtable (0x460590) when needed, 0x453210, the SOM setup (0x450760) for
   SOM classes, and 0x4607f0 unless +0x29 is set (cleared for uninstantiated template instances:
   flags +0x26 & 0x800 without +0x43), then 0x4606e0. */
ORIG_FN(void, CDecl_AddDefaultAssignmentOperator_d, 0x449e60, (void *decle, void *tclass));
ORIG_FN(void, CDecl_AddDefaultCopyConstructor_d, 0x449c90, (void *decle, void *tclass));
ORIG_FN(void, CDecl_SetupClassLayout_d, 0x44a150, (void *decle, void *tclass, UInt8 *layout));
ORIG_FN(void, CClass_MakeVTable_d, 0x460590, (void *tclass));
ORIG_FN(void, fn_453210_d, 0x453210, (void *decle, void *tclass));
ORIG_FN(void, CSOM_ClassComplete_d, 0x450760, (void *tclass));
ORIG_FN(void, fn_4607f0_d, 0x4607f0, (void *tclass));
ORIG_FN(void, fn_4606e0_d, 0x4606e0, (void *tclass));

void CDecl_CompleteClass(UInt8 *decle, UInt8 *tclass) {
    UInt8 layout[0x80];
    void **base;

#ifdef RECOMP_NEGATIVE_CONTROL_CDECL10 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CDECL10 */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "CDECL10@"); /* marks runs */
#endif
    for (base = F(tclass, 0xe, void **); base; base = (void **)base[0]) {
        if (F(base[1], 0x1e, void *))
            decle[0xc] = 1;
    }
    if (!F(tclass, 0x22, void *)) {
        CDecl_AddDefaultDestructor(decle, tclass);
        CDecl_AddDefaultAssignmentOperator_d(decle, tclass);
        CDecl_AddDefaultConstructor(decle, tclass);
        CDecl_AddDefaultCopyConstructor_d(decle, tclass);
    }
    CDecl_SetupClassLayout_d(decle, tclass, layout);
    if (decle[0xc])
        CClass_MakeVTable_d(tclass);
    fn_453210_d(decle, tclass);
    if (F(tclass, 0x22, void *))
        CSOM_ClassComplete_d(tclass);
    if ((F(tclass, 0x26, UInt16) & 0x800) && !tclass[0x43])
        tclass[0x29] = 0;
    if (!tclass[0x29])
        fn_4607f0_d(tclass);
    fn_4606e0_d(tclass);
}
HOOK(0x44a3a0, CDecl_CompleteClass);

/* The implicit copy constructor (mweppc's CDecl_AddDefaultCopyConstructor, from the exe): needed
   when the class has a constructor, virtual bases or DeclE +0xc, or a base or member class has a
   copy constructor (CClass_CopyConstructor 0x45ecd0); its argument is const unless one of those
   copy constructors takes a non-const reference (the argument after this, and after the hidden
   one of classes with virtual bases; qual +0x10 & 1). */
ORIG_FN(UInt8 *, CClass_CopyConstructor_d, 0x45ecd0, (void *tclass));

static Bool8 copy_arg_const(UInt8 *copyctor, UInt8 *cls) {
    void **arg = (void **)*F(F(copyctor, 0xc, UInt8 *), 6, void ***);

    if (F(cls, 0x26, UInt16) & 0x20)
        arg = (void **)arg[0];
    return (F(arg, 0x10, UInt32) & 1) != 0;
}

void CDecl_AddDefaultCopyConstructor(UInt8 *decle, UInt8 *tclass) {
    UInt8 access = 0;
    Bool8 is_const = 1, has = 0;
    void **base;
    UInt8 *ivar, *t, *c;

    if (CClass_CopyConstructor_d(tclass))
        return;
    if (CClass_Constructor_d(tclass))
        has = 1;
    if ((F(tclass, 0x26, UInt16) & 0x20) || decle[0xc])
        has = 1;
    for (base = F(tclass, 0xe, void **); base; base = (void **)base[0]) {
        if ((c = CClass_CopyConstructor_d(base[1]))) {
            has = 1;
            access = getaccesstype(access, c[1], 1);
            if (!copy_arg_const(c, (UInt8 *)base[1]))
                is_const = 0;
        }
    }
    for (ivar = F(tclass, 0x16, UInt8 *); ivar; ivar = F(ivar, 4, UInt8 *)) {
        if ((t = class_below_arrays(F(ivar, 0xc, UInt8 *))) && (c = CClass_CopyConstructor_d(t))) {
            has = 1;
            access = getaccesstype(access, c[1], 0);
            if (!copy_arg_const(c, t))
                is_const = 0;
        }
    }
    if (has)
        fn_449b10(tclass, *(void **)0x57b718, CDecl_MakeCopyCtorType(tclass, is_const), access);
}
HOOK(0x449c90, CDecl_AddDefaultCopyConstructor);

/* The implicit assignment operator (mweppc's CDecl_AddDefaultAssignmentOperator, from the exe):
   none when the class declares one (CClass_AssignmentOperator 0x45ec50); needed unless copts
   0x57dbb8, or with virtual bases, DeclE +0xc, a member named operator= (CClass_MemberObject
   0x45eda0), or a base or member class with one; const argument unless one of those takes a
   non-const reference (its argument after this). */
ORIG_FN(UInt8 *, CClass_AssignmentOperator_d, 0x45ec50, (void *tclass));
ORIG_FN(void *, CClass_MemberObject_d, 0x45eda0, (void *tclass, void *name));

static Bool8 asop_arg_const(UInt8 *asop) {
    void **arg = (void **)*F(F(asop, 0xc, UInt8 *), 6, void ***);

    return (F(arg, 0x10, UInt32) & 1) != 0;
}

void CDecl_AddDefaultAssignmentOperator(UInt8 *decle, UInt8 *tclass) {
    UInt8 di[0x68];
    UInt8 access = 0;
    Bool8 is_const = 1, has = 0;
    void **base;
    UInt8 *ivar, *t, *a;

    if (CClass_AssignmentOperator_d(tclass))
        return;
    if (!*(UInt8 *)0x57dbb8)
        has = 1;
    if ((F(tclass, 0x26, UInt16) & 0x20) || decle[0xc] || CClass_MemberObject_d(tclass, *(void **)0x57c8a8))
        has = 1;
    for (base = F(tclass, 0xe, void **); base; base = (void **)base[0]) {
        if ((a = CClass_AssignmentOperator_d(base[1]))) {
            has = 1;
            access = getaccesstype(access, a[1], 1);
            if (!asop_arg_const(a))
                is_const = 0;
        }
    }
    for (ivar = F(tclass, 0x16, UInt8 *); ivar; ivar = F(ivar, 4, UInt8 *)) {
        if ((t = class_below_arrays(F(ivar, 0xc, UInt8 *))) && (a = CClass_AssignmentOperator_d(t))) {
            has = 1;
            access = getaccesstype(access, a[1], 0);
            if (!asop_arg_const(a))
                is_const = 0;
        }
    }
    if (!has)
        return;
    memclrw(di, 0x66);
    F(di, 4, UInt32) |= 0x10;
    F(di, 0, void *) = fn_449df0(tclass, is_const);
    F(di, 0xc, void *) = *(void **)0x57c8a8;
    CDecl_AddFunctionMember_d(decle, tclass, di, access, 1, 0, 0, 0);
}
HOOK(0x449e60, CDecl_AddDefaultAssignmentOperator);
