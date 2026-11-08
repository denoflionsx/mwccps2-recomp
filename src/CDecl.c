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
