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
