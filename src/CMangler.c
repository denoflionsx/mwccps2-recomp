/*
 * CMangler.c — C++ name mangling (mweppc src/compiler/frontend/CMangler.c), partial port.
 *
 * The functions here are byte-identical between mweppc's GameCube 2.0 compiler and ours. Ported so
 * far: CMangler_Setup, CMangler_MangleType, CMangler_RTTIObjectName and
 * CMangler_ConversionFuncName. CMangler_MangleTypeAppend (1,931 bytes) stays the original's.
 *
 * name_mangle_list is a GList {data (handle), size, ...} at 0x56acd8. Strings are the exe's own.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define S(addr) ((const char *)(addr))

ORIG_FN(void *, GetHashNameNodeExport, 0x432ef0, (const char *name));
ORIG_FN(void, CMangler_MangleTypeAppend, 0x44f7e0, (void *type, UInt32 qual));
ORIG_FN(void, AppendGListName, 0x432d60, (void *list, const char *name));
ORIG_FN(void, AppendGListByte, 0x432be0, (void *list, SInt8 b));
ORIG_FN(void, COS_LockHandle, 0x441850, (char **h));
ORIG_FN(void, COS_UnlockHandle, 0x441870, (char **h));
ORIG_FN(Bool8, CTemplTool_IsTemplateArgumentDependentType, 0x47cb20, (void *type));

#define name_mangle_list ((void *)0x56acd8)
#define name_mangle_data ORIG_VAR(char **, 0x56acd8)
#define name_mangle_size ORIG_VAR(SInt32, 0x56acdc)
#define constructor_name_node ORIG_VAR(void *, 0x57b718)
#define destructor_name_node ORIG_VAR(void *, 0x57ab54)
#define asop_name_node ORIG_VAR(void *, 0x57c8a8)

void CMangler_Setup(void) {
    constructor_name_node = GetHashNameNodeExport(S(0x5354b0) /* "__ct" */);
    destructor_name_node = GetHashNameNodeExport(S(0x5354b8) /* "__dt" */);
    asop_name_node = GetHashNameNodeExport(S(0x5354c0) /* "__as" */);
}
HOOK(0x44ea60, CMangler_Setup);

static void *mangled_name(const char *prefix, void *type, UInt32 qual) {
    void *name;
    name_mangle_size = 0;
    AppendGListName(name_mangle_list, prefix);
    CMangler_MangleTypeAppend(type, qual);
    AppendGListByte(name_mangle_list, 0);
    COS_LockHandle(name_mangle_data);
    name = GetHashNameNodeExport(*name_mangle_data);
    COS_UnlockHandle(name_mangle_data);
    return name;
}

void *CMangler_RTTIObjectName(void *type, UInt32 qual) {
#ifdef RECOMP_NEGATIVE_CONTROL_CMANGLER /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CMANGLER */
    return mangled_name(S(0x5358c8) /* "__op" */, type, qual);
#endif
    return mangled_name(S(0x535838) /* "__RTTI__" */, type, qual);
}
HOOK(0x44f330, CMangler_RTTIObjectName);

void CMangler_MangleType(void *type, UInt32 qual) {
    name_mangle_size = 0;
    CMangler_MangleTypeAppend(type, qual);
}
HOOK(0x44fc90, CMangler_MangleType);

void *CMangler_ConversionFuncName(void *type, UInt32 qual) {
    if (CTemplTool_IsTemplateArgumentDependentType(type))
        return GetHashNameNodeExport(S(0x5358c8) /* "__op" */);
    return mangled_name(S(0x5358c8), type, qual);
}
HOOK(0x44fe80, CMangler_ConversionFuncName);

/* ---- part 2: name pieces and CMangler_GetLinkName (not matched by gcmatch; the same logic as
   mweppc's, with ours storing the data link name at +0x38 and the function one at +0x30) ---- */

/* name_mangle_list 0x56acd8. NameSpace: parent +0, name +4; HashNameNode.name +0xa. TypeClass:
   nspace +6, classname +0xa. Object: datatype +2 (DDATA 0, DLOCAL 1, DABSOLUTE 2, DFUNC 3,
   DVFUNC 4, DINLINEFUNC 5, DALIAS 6, DLABEL 8), name +8, u.alias.object +0x28, function link name
   +0x30, data link name +0x38. */
ORIG_FN(UInt32, strlen_, 0x404ee0, (const char *s));
ORIG_FN(int, sprintf_, 0x403d30, (char *buf, const char *fmt, ...));
ORIG_FN(void *, CMangler_FunctionLinkName, 0x44ff10, (void *obj));
ORIG_FN(void *, CMangler_DataLinkName, 0x450060, (void *obj));
ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));
#define name_mangle_list ((void *)0x56acd8)

void CMangler_MangleTypeName(const char *str) {
    char buf[16];

    sprintf_(buf, (const char *)0x535874 /* "%d" */, strlen_(str));
    AppendGListName(name_mangle_list, buf);
    AppendGListName(name_mangle_list, str);
}
HOOK(0x44f6b0, CMangler_MangleTypeName);

void CMangler_MangleNameSpaceName(void **nspace, const char *str) {
    const char *stack[10];
    int stackp = 1;
    void **scan;

    stack[0] = str;
    for (scan = nspace; scan; scan = scan[0]) {
        if (scan[1]) {
            stack[stackp++] = (const char *)scan[1] + 0xa;
            if (stackp >= 9)
                break;
        }
    }
    if (stackp > 1) {
        AppendGListByte(name_mangle_list, 'Q');
        AppendGListByte(name_mangle_list, '0' + stackp);
    }
    while (--stackp >= 0)
        CMangler_MangleTypeName(stack[stackp]);
}
HOOK(0x44f700, CMangler_MangleNameSpaceName);

void CMangler_MangleClassName(UInt8 *tclass) {
    void **nspace = *(void ***)(tclass + 6);

    if (!*(void **)(tclass + 0xa))
        CMangler_MangleNameSpaceName(nspace[0], (const char *)0x535878 /* "class" */);
    else
        CMangler_MangleNameSpaceName(nspace[0], (const char *)nspace[1] + 0xa);
}
HOOK(0x44f770, CMangler_MangleClassName);

void CMangler_MangleQualifier(UInt32 qual) {
    if (qual & 1) /* Q_CONST */
        AppendGListByte(name_mangle_list, 'C');
#ifndef RECOMP_NEGATIVE_CONTROL_CMANGLER2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CMANGLER2 */
    if (qual & 2) /* Q_VOLATILE */
        AppendGListByte(name_mangle_list, 'V');
#endif
}
HOOK(0x44f7a0, CMangler_MangleQualifier);

void *CMangler_GetLinkName(UInt8 *obj) {
    while (obj[2] == 6) /* DALIAS */
        obj = *(UInt8 **)(obj + 0x28);
    switch (obj[2]) {
    case 0: /* DDATA */
        if (!*(void **)(obj + 0x38))
            *(void **)(obj + 0x38) = CMangler_DataLinkName(obj);
        return *(void **)(obj + 0x38);
    case 3: /* DFUNC */
    case 4: /* DVFUNC */
        if (!*(void **)(obj + 0x30))
            *(void **)(obj + 0x30) = CMangler_FunctionLinkName(obj);
        return *(void **)(obj + 0x30);
    case 5: /* DINLINEFUNC */
        return CMangler_FunctionLinkName(obj);
    case 1: /* DLOCAL */
    case 2: /* DABSOLUTE */
    case 8: /* DLABEL */
        return *(void **)(obj + 8);
    default:
        CError_Internal((const char *)0x535868 /* "CMangler.c" */, 1110);
        return 0;
    }
}
HOOK(0x450140, CMangler_GetLinkName);
