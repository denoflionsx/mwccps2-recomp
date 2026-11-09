/*
 * CClass.c — C++ classes of the frontend (mweppc src/compiler/frontend/CClass.c), partial port.
 *
 * Ported so far: CClass_MemberObject, CClass_CheckEnumAccess (byte-identical to mweppc's GameCube
 * 2.0 compiler), and CClass_IsConstructor, CClass_IsDestructor and CClass_CheckPathAccess (close:
 * ours return an int, not a byte, from the two queries). Layouts, inheritance, access checks
 * proper (CClass_CanAccess, CClass_PathCleanup...) and the vtable code stay the original's.
 *
 * Object: type +0xc. TypeFunc.flags +0x16 (FUNC_IS_CTOR 0x1000, FUNC_IS_DTOR 0x2000).
 * TypeClass.nspace +6. NameSpaceObjectList: next +0, object +4; OT_OBJECT 5. BClassList.type +4.
 * ObjEnumConst: access (byte) +1, type +0xa; TypeEnum.nspace +6; NameSpace.theclass +0xc.
 */
#include "orig.h"

typedef UInt8 Bool8;

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void *, CScope_FindName, 0x44af70, (void *nspace, void *name));
ORIG_FN(Bool8, CClass_CanAccess, 0x4609c0, (void *path, UInt8 access));
ORIG_FN(void *, CClass_PathCleanup, 0x460b40, (void *path, void *tclass));
ORIG_FN(void, CClass_CheckStaticAccess, 0x460bb0, (void *path, void *tclass, UInt8 access));
ORIG_FN(void, CError_Error, 0x456d50, (int code, ...));

static int func_flag(UInt8 *obj, UInt32 flag) {
    return obj && F(obj, 0xc, UInt8 *)[0] == 6 /* TYPEFUNC */ && (F(F(obj, 0xc, UInt8 *), 0x16, UInt32) & flag);
}

int CClass_IsConstructor(UInt8 *obj) {
    return func_flag(obj, 0x1000) ? 1 : 0;
}
HOOK(0x45ee50, CClass_IsConstructor);

int CClass_IsDestructor(UInt8 *obj) {
#ifdef RECOMP_NEGATIVE_CONTROL_CCLASS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CCLASS */
    return 0; /* no function is a destructor */
#endif
    return func_flag(obj, 0x2000) ? 1 : 0;
}
HOOK(0x45ee80, CClass_IsDestructor);

void *CClass_MemberObject(UInt8 *tclass, void *name) {
    void **nsol = CScope_FindName(F(tclass, 6, void *), name);
#ifdef RECOMP_NEGATIVE_CONTROL_CCLASS2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CCLASS2 */
    return 0; /* no member objects found */
#endif
    if (nsol && ((UInt8 *)nsol[1])[0] == 5) /* OT_OBJECT */
        return nsol;
    return 0;
}
HOOK(0x45eda0, CClass_MemberObject);

void CClass_CheckPathAccess(UInt8 *path, void *obj, UInt8 access) {
    if (!CClass_CanAccess(path, access)) {
        if (path && obj)
            CError_Error(0x17d, F(path, 4, void *), 0, obj);
        else
            CError_Error(0xbb);
    }
}
HOOK(0x460af0, CClass_CheckPathAccess);

void CClass_CheckEnumAccess(void *path, UInt8 *objec) {
    UInt8 *type;

    if (path) {
        if ((path = CClass_PathCleanup(path, 0)) != 0) {
            if (!CClass_CanAccess(path, objec[1]))
                CError_Error(0xbb);
            return;
        }
    }
    type = F(objec, 0xa, UInt8 *);
    if (objec[1] != 0 && type[0] == 3 /* TYPEENUM */ && F(type, 6, UInt8 *) && F(F(type, 6, UInt8 *), 0xc, void *))
        CClass_CheckStaticAccess(0, F(F(type, 6, UInt8 *), 0xc, void *), objec[1]);
}
HOOK(0x460cd0, CClass_CheckEnumAccess);

/* ---- part 2: CClass_Constructor, CClass_Destructor, CClass_CheckObjectAccess (not matched by
   gcmatch; the same logic as mweppc's) ---- */

/* constructor_name_node 0x57b718, destructor_name_node 0x57ab54, cscope_currentclass 0x57ca28.
   Object: access (byte) +1, nspace +4. */
ORIG_FN(void *, CClass_GetBasePath, 0x45f430, (void *a, void *b, SInt16 *depth, Bool8 *isambigbase));
#define constructor_name_node ORIG_VAR(void *, 0x57b718)
#define destructor_name_node ORIG_VAR(void *, 0x57ab54)
#define cscope_currentclass ORIG_VAR(void *, 0x57ca28)

static int is_func_object(void **nsol) {
    UInt8 *obj = nsol[1];
    return obj[0] == 5 && F(obj, 0xc, UInt8 *)[0] == 6;
}

void *CClass_Constructor(UInt8 *tclass) {
    void **nsol = CScope_FindName(F(tclass, 6, void *), constructor_name_node);
    if (nsol && is_func_object(nsol))
        return nsol;
    return 0;
}
HOOK(0x45edd0, CClass_Constructor);

void *CClass_Destructor(UInt8 *tclass) {
    void **nsol;

#ifdef RECOMP_NEGATIVE_CONTROL_CCLASS3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CCLASS3 */
    return 0; /* no class has a destructor */
#endif
    for (nsol = CScope_FindName(F(tclass, 6, void *), destructor_name_node); nsol; nsol = nsol[0])
        if (is_func_object(nsol))
            return nsol[1];
    return 0;
}
HOOK(0x45ee10, CClass_Destructor);

void CClass_CheckObjectAccess(void *path, UInt8 *obj) {
    SInt16 depth;
    Bool8 isambigbase;
    UInt8 *nspace = F(obj, 4, UInt8 *);

    if (nspace && F(nspace, 0xc, void *)) {
        if (!path && cscope_currentclass)
            path = CClass_GetBasePath(cscope_currentclass, F(nspace, 0xc, void *), &depth, &isambigbase);
        CClass_CheckStaticAccess(path, F(F(obj, 4, UInt8 *), 0xc, void *), obj[1]);
    }
}
HOOK(0x460c70, CClass_CheckObjectAccess);

/* ---- part 3: CClass_CanAccess (not matched by gcmatch; mweppc's logic without Objective-C) ---- */

/* BClassList: next +0, type +4. TypeClass: bases +0xe (ClassList: next +0, base +4, access (byte)
   +0x10), friends +0x1a (ClassFriend: next +0, u +4, isclass (byte) +8). AccessType: PUBLIC 0,
   PRIVATE 1, PROTECTED 2, NONE 3. cscope_currentfunc 0x57d6b0. */
#define cscope_currentfunc ORIG_VAR(void *, 0x57d6b0)

Bool8 CClass_CanAccess_(void **path, UInt8 access) {
    UInt8 *tclass = path[1];
    UInt8 *prevclass;
    UInt8 access2 = access;
    void **scan, **base, **cfriend, **next;

    if ((scan = path[0]) != 0) {
        if (access2 != 1) {
            prevclass = tclass;
            while (scan) {
                for (base = F(prevclass, 0xe, void **); base; base = base[0])
                    if (base[1] == scan[1])
                        break;
                if (!base)
                    return 0;
                switch (((UInt8 *)base)[0x10]) {
                case 3:
                    access2 = 3;
                    break;
                case 2:
                    if (access2 == 0)
                        access2 = 2;
                    break;
                case 1:
                    access2 = access2 == 1 ? 3 : 1;
                    break;
                }
                prevclass = scan[1];
                scan = scan[0];
            }
        } else {
            access2 = 3;
        }
    }
    if (access2 == 0)
        return 1;
    if (access2 != 3) {
        if (cscope_currentclass == tclass)
            return 1;
        for (cfriend = F(tclass, 0x1a, void **); cfriend; cfriend = cfriend[0]) {
            if (((UInt8 *)cfriend)[8]) {
                if (cfriend[1] == cscope_currentclass)
                    return 1;
            } else if (cfriend[1] == cscope_currentfunc) {
                return 1;
            }
        }
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CCLASS4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CCLASS4 */
    *(volatile int *)0 = 0; /* crash: refusing base-path access is not caught */
#endif
    for (scan = path; scan[0]; scan = scan[0]) {
        if (CClass_CanAccess_(scan[0], access)) {
            if ((next = ((void **)scan[0])[0]) != 0 || access != 0) {
                ((void **)scan[0])[0] = 0;
                if (CClass_CanAccess_(path, 0)) {
                    ((void **)scan[0])[0] = next;
                    return 1;
                }
                ((void **)scan[0])[0] = next;
            }
        }
    }
    return 0;
}
HOOK(0x4609c0, CClass_CanAccess_);

/* ---- part 4: the this object and expression, and a class's vtable object (from the exe; the
   first two are mweppc's CClass_ThisSelfObject/CClass_CreateThisSelfExpr logic) ---- */

/* cscope_is_member_func 0x57e034, arguments 0x57caa0 (ObjectList: next +0, object +4; Object.name
   +8), this_name_node 0x57b738. Errors: 0xbd (not in a member function), 0x12d (in C). */
#define cscope_is_member_func ORIG_VAR(Bool8, 0x57e034)
#define arguments_ ORIG_VAR(void **, 0x57caa0)
#define this_name_node ORIG_VAR(void *, 0x57b738)
#define cplusplus_cc ORIG_VAR(Bool8, 0x57db79)

void *CClass_ThisSelfObject(void) {
    void **list;

    if (cscope_currentfunc && cscope_currentclass) {
        if (cscope_is_member_func) {
            for (list = arguments_; list; list = (void **)list[0]) {
                if (F(list[1], 8, void *) == this_name_node)
                    return list[1];
            }
        }
        CError_Error(0xbd);
    }
    CError_Error(cplusplus_cc ? 0xbd : 0x12d);
    return 0;
}
HOOK(0x460900, CClass_ThisSelfObject);

ORIG_FN(UInt8 *, create_objectrefnode_cc, 0x4f0170, (void *obj));
ORIG_FN(void *, CDecl_NewPointerType_cc, 0x443190, (void *type));
ORIG_FN(UInt8 *, makemonadicnode_cc, 0x4ef320, (void *expr, UInt8 ty));

UInt8 *CClass_CreateThisSelfExpr(void) {
    void *obj = CClass_ThisSelfObject();
    UInt8 *expr;

    if (!obj)
        return 0;
    expr = create_objectrefnode_cc(obj);
    F(expr, 0xa, void *) = CDecl_NewPointerType_cc(cscope_currentclass);
    expr = makemonadicnode_cc(expr, 4);
#ifdef RECOMP_NEGATIVE_CONTROL_CCLASS5 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CCLASS5 */
    F(expr, 0xa, void *) = cscope_currentclass; /* this as the class, not a pointer */
#endif
    F(F(expr, 0x18, UInt8 *), 0xa, void *) = CDecl_NewPointerType_cc(F(expr, 0xa, void *));
    return expr;
}
HOOK(0x460970, CClass_CreateThisSelfExpr);

/* TypeClass.vtable +0x1e (its object first): made static (sclass 0x102) with qual 0x20000; an
   object not yet defined (flags +0x16 & 2) goes to 0x432220 (object, class), else the class goes
   to 0x432250 unless the parameter block's +0xe0 is 1 (cparamblkptr 0x57ca38). */
ORIG_FN(void, fn_432220, 0x432220, (void *obj, void *tclass));
ORIG_FN(void, fn_432250, 0x432250, (void *tclass));

void fn_4607f0(UInt8 *tclass) {
    UInt8 *obj;

    if (!F(tclass, 0x1e, void *))
        return;
    F(*F(tclass, 0x1e, UInt8 **), 0x14, SInt16) = 0x102;
    F(*F(tclass, 0x1e, UInt8 **), 0x10, UInt32) |= 0x20000;
    obj = *F(tclass, 0x1e, UInt8 **);
    if (!(obj[0x16] & 2))
        fn_432220(obj, tclass);
    else if (F(*(UInt8 **)0x57ca38, 0xe0, UInt8) != 1)
        fn_432250(tclass);
}
HOOK(0x4607f0, fn_4607f0);

/* ---- part 5: the key-function vtable check (from the exe) ---- */

/* TypeClass vtable state (SInt8) +0x29: 1 = waiting for the key function. A virtual (type flags
   +0x16 & 4) member function obj that is inline (qual +0x10 & 0x10) settles it here (error 0x118
   when the class has +0x22 set), else the class goes to 0x432250 unless the parameter block's
   +0xe0 is 1. States 0 and 3 do nothing; 2 and others are internal errors. */
ORIG_FN(void, CError_Internal_cc, 0x457230, (const char *file, int line));
ORIG_FN(void, fn_4607f0_, 0x4607f0, (void *tclass));
ORIG_FN(void, fn_432250_, 0x432250, (void *tclass));

void fn_460860(UInt8 *obj, UInt8 *tclass) {
    UInt8 *type;

    switch ((SInt8)tclass[0x29]) {
    case 0:
    case 3:
        return;
    case 1:
        type = F(obj, 0xc, UInt8 *);
        if (type[0] != 6 || !(F(type, 0x16, UInt32) & 4))
            return;
        if (F(obj, 0x10, UInt32) & 0x10) {
            if (F(tclass, 0x22, void *))
                CError_Error(0x118);
            tclass[0x29] = 0;
            fn_4607f0_(tclass);
            return;
        }
        if (F(*(UInt8 **)0x57ca38, 0xe0, UInt8) != 1)
            fn_432250_(tclass);
        return;
    case 2:
        CError_Internal_cc((const char *)0x536748 /* "CClass.c" */, 0xa7a);
        return;
    default:
        CError_Internal_cc((const char *)0x536748, 0xa8d);
        return;
    }
}
HOOK(0x460860, fn_460860);

/* walks a class's base tree (node: list +4 (next +0), children +8 (next +0, child +4)), calling
   0x45fc20 (root 0x54d696, node, item) for each item of every node but the root */
ORIG_FN(void, fn_45fc20, 0x45fc20, (void *root, void *node, void *item));
#define base_root ORIG_VAR(UInt8 *, 0x54d696)

void fn_460050(UInt8 *node) {
    void **p;

    if (base_root != node) {
        for (p = F(node, 4, void **); p; p = (void **)p[0])
            fn_45fc20(base_root, node, p);
    }
    for (p = F(node, 8, void **); p; p = (void **)p[0])
        fn_460050((UInt8 *)p[1]);
}
HOOK(0x460050, fn_460050);

/* builds a class's base tree (0x45fa60), walks it (fn_460050 above, 0x4602b0 (tree, 1),
   0x4600a0); the class goes in 0x54d6be and the tree in 0x54d696 */
ORIG_FN(UInt8 *, fn_45fa60, 0x45fa60, (void *a, void *tclass, void *b, void *c));
ORIG_FN(void, fn_4602b0, 0x4602b0, (void *tree, SInt32 x));
ORIG_FN(SInt32, fn_4600a0, 0x4600a0, (void *tree));

SInt32 fn_460360(void *tclass) {
    UInt8 *tree;

    *(void **)0x54d6be = tclass;
    tree = fn_45fa60(0, tclass, 0, 0);
    base_root = tree;
    fn_460050(tree);
    fn_4602b0(tree, 1);
    return fn_4600a0(tree);
}
HOOK(0x460360, fn_460360);

/* ---- part 6: the assignment operator, virtual override search and qualifier moves (mweppc's
   logic where it has the function, else from the exe) ---- */

/* Object: otype +0 (5), datatype +2 (DVFUNC 4), type +0xc. TypeFunc: args +6 (FuncArg: next +0,
   type +0xc). Pointer: target +6, qual +0xa (0x20 reference); member pointer qual +0xe.
   TypeClass: nspace +6, bases +0xe (next +0, base +4), vtable +0x1e. asop_name_node 0x57c8a8. */
ORIG_FN(UInt8, CClass_GetOverrideKind_cc, 0x45e6b0, (void *a, void *b, Bool8 errorflag));
ORIG_FN(UInt8 *, fn_460d40_cc, 0x460d40, (UInt8 *type));

UInt8 *CClass_AssignmentOperator(UInt8 *tclass) {
    void **list;
    UInt8 *obj, *argtype;
    void **args;

    for (list = CScope_FindName(F(tclass, 6, void *), *(void **)0x57c8a8); list; list = (void **)list[0]) {
        obj = (UInt8 *)list[1];
        if (obj[0] != 5 || F(obj, 0xc, UInt8 *)[0] != 6)
            continue;
        args = F(F(obj, 0xc, UInt8 *), 6, void **);
        if (!args || !(args = (void **)args[0]) || args[0])
            continue;
        argtype = (UInt8 *)args[3];
#ifdef RECOMP_NEGATIVE_CONTROL_CCLASS6 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CCLASS6 */
        continue; /* no user assignment operator ever found */
#endif
        if (argtype[0] == 5 && argtype == tclass)
            return obj;
        if (argtype[0] == 0xb && (F(argtype, 0xa, UInt32) & 0x20) && F(argtype, 6, UInt8 *) == tclass)
            return obj;
    }
    return 0;
}
HOOK(0x45ec50, CClass_AssignmentOperator);

/* whether a virtual function of some base with a vtable (recursively) is overridden by obj */
Bool8 fn_45f990(UInt8 *tclass, void *name, UInt8 *obj) {
    Bool8 result = 0;
    void **base, **list;
    UInt8 *bclass, *chk;

    for (base = F(tclass, 0xe, void **); base; base = (void **)base[0]) {
        bclass = (UInt8 *)base[1];
        if (!F(bclass, 0x1e, void *))
            continue;
        for (list = CScope_FindName(F(bclass, 6, void *), name); list; list = (void **)list[0]) {
            chk = (UInt8 *)list[1];
            if (chk[0] == 5 && chk[2] == 4 && CClass_GetOverrideKind_cc(F(chk, 0xc, void *), F(obj, 0xc, void *), 1))
                result = 1;
        }
        if (fn_45f990(bclass, name, obj))
            result = 1;
    }
    return result;
}
HOOK(0x45f990, fn_45f990);

/* Moves const/volatile (qual2 & 3, without const when qual1 has 0x80) onto the innermost pointer
   or member pointer below any arrays of type (copied by 0x460d40 first), or into *out with
   qual1 & 3 when there is none; returns the (new) type */
UInt8 *fn_460db0(UInt8 *type, UInt32 qual1, UInt32 qual2, UInt32 *out) {
    UInt32 add = qual2 & 3;
    UInt32 q = qual1 & 3;
    UInt8 *scan;

    if (qual1 & 0x80)
        add &= ~1u;
    for (scan = type; scan[0] == 0xc; scan = F(scan, 6, UInt8 *))
        ;
    if (scan[0] == 0xa || scan[0] == 0xb) {
        if (add) {
            type = fn_460d40_cc(type);
            for (scan = type; scan[0] == 0xc; scan = F(scan, 6, UInt8 *))
                ;
            if (scan[0] == 0xa)
                F(scan, 0xe, UInt32) |= add;
            else if (scan[0] == 0xb)
                F(scan, 0xa, UInt32) |= add;
            else
                CError_Internal_cc((const char *)0x536748 /* "CClass.c" */, 0xc35);
        }
    } else {
        q |= add;
    }
    *out = q;
    return type;
}
HOOK(0x460db0, fn_460db0);

/* ---- part 7: CClass_CopyConstructor (mweppc's logic) ---- */

/* FuncArg: next +0, dexpr +8, type +0xc; elipsis 0x56a818; constructor_name_node 0x57b718 */
UInt8 *CClass_CopyConstructor(UInt8 *tclass) {
    void **list;
    UInt8 *obj;
    void **args;
    UInt8 *t;

    if (F(tclass, 0x22, void *))
        return 0;
    for (list = CScope_FindName(F(tclass, 6, void *), constructor_name_node); list; list = (void **)list[0]) {
        obj = (UInt8 *)list[1];
        if (obj[0] != 5 || F(obj, 0xc, UInt8 *)[0] != 6)
            continue;
        args = F(F(obj, 0xc, UInt8 *), 6, void **);
        if (!args)
            CError_Internal_cc((const char *)0x536748 /* "CClass.c" */, 0x214);
        args = (void **)args[0];
        if (F(tclass, 0x26, UInt16) & 0x20) {
            if (!args)
                CError_Internal_cc((const char *)0x536748, 0x218);
            args = (void **)args[0];
        }
        if (!args || args == (void **)0x56a818)
            continue;
        if (args[0] && !((void **)args[0])[2])
            continue;
        t = (UInt8 *)args[3];
#ifdef RECOMP_NEGATIVE_CONTROL_CCLASS7 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CCLASS7 */
        ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "CCLASS7@"); /* marks runs */
#endif
        if (t[0] == 0xb && (F(t, 0xa, UInt32) & 0x20) && F(t, 6, UInt8 *) == tclass)
            return obj;
    }
    return 0;
}
HOOK(0x45ecd0, CClass_CopyConstructor);

/* CClass_PathCleanup (mweppc's logic, from the exe): BClassList next +0, type +4. Drops repeated
   classes, restarts the path after a step that is not a direct base, and checks the end against
   tclass (when given). */
UInt8 **CClass_PathCleanup_(UInt8 **path, UInt8 *tclass) {
    UInt8 **first = path, **next;
    void **base;

    while ((next = (UInt8 **)path[0])) {
        if (path[1] == next[1]) {
            path[0] = next[0];
            continue;
        }
        for (base = F(path[1], 0xe, void **); base; base = (void **)base[0]) {
            if (base[1] == next[1])
                break;
        }
        if (!base)
            first = next;
        path = next;
    }
    if (!tclass)
        return first;
    return path[1] == tclass ? first : 0;
}
HOOK(0x460b40, CClass_PathCleanup_);

/* CClass_CheckStaticAccess (mweppc's logic): access 0 public, 1/2 private/protected (allowed in
   the class itself or its friends: list +0x1a, next +0, member +4, isclass +8), 3 none */
void CClass_CheckStaticAccess_(UInt8 **path, UInt8 *tclass, UInt8 access) {
    UInt8 **friend;

#ifdef RECOMP_NEGATIVE_CONTROL_CCLASS8 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CCLASS8 */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "CCLASS8@"); /* marks runs */
#endif
    if (path) {
        path = CClass_PathCleanup_(path, tclass);
        if (path && path[0]) {
            if (!CClass_CanAccess(path, access))
                CError_Error(0xbb);
            return;
        }
    }
    switch (access) {
    case 0:
        return;
    case 1:
    case 2:
        if (tclass == cscope_currentclass)
            return;
        for (friend = F(tclass, 0x1a, UInt8 **); friend; friend = (UInt8 **)friend[0]) {
            if (((UInt8 *)friend)[8]) {
                if ((void *)friend[1] == cscope_currentclass)
                    return;
            } else if ((void *)friend[1] == cscope_currentfunc) {
                return;
            }
        }
        /* fall through */
    case 3:
        ((void (*)(int, ...))0x4571c0)(0xbb); /* CError_Warning */
        return;
    default:
        CError_Internal_cc((const char *)0x536748 /* "CClass.c" */, 0xbc5);
    }
}
HOOK(0x460bb0, CClass_CheckStaticAccess_);
