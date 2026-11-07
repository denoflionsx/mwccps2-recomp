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
