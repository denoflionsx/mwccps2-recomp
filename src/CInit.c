/*
 * CInit.c — handing initialized data to the object writer (mweppc src/compiler/frontend/CInit.c),
 * partial port.
 *
 * Only 3 of the source's functions are functions of their own in our exe; all byte-identical
 * between mweppc's GameCube 2.0 compiler and ours, and all ported. Object: qual +0x10, flags
 * (byte) +0x16 (OBJECT_DEFINED 4). OLinkList: next +0, obj +4. cparamblkptr->precompile +0xe0.
 * The callees (PreComp_StaticData, CInline_ObjectAddrRef, CPrep_SetSourceFile,
 * ObjGen_Declare[ReadOnly]Data) are cdecl.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef struct OLinkList {
    struct OLinkList *next;
    void *obj;
} OLinkList;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void, PreComp_StaticData, 0x4e2640, (void *obj, void *data, OLinkList *list, SInt32 size));
ORIG_FN(void, CInline_ObjectAddrRef, 0x465f40, (void *obj));
ORIG_FN(void, CPrep_SetSourceFile, 0x43c850, (void *fileoffset));
ORIG_FN(void, ObjGen_DeclareReadOnlyData, 0x48a630, (void *obj, void *data, OLinkList *list, SInt32 size));
ORIG_FN(void, ObjGen_DeclareData, 0x48a660, (void *obj, void *data, OLinkList *list, SInt32 size));

#define cparamblkptr ORIG_VAR(UInt8 *, 0x57ca38)
#define fatalerrors ORIG_VAR(Bool8, 0x57e15c)
#define filesyminfo ORIG_VAR(Bool8, 0x57dbf9) /* copts */
#define cparser_fileoffset ((void *)0x57dc2c)

void declaredata(void *obj, void *data, OLinkList *list, SInt32 size, Bool8 is_readonly) {
    OLinkList *scan;
    UInt32 qual = F(obj, 0x10, UInt32);

    if (F(cparamblkptr, 0xe0, UInt8) == 1) {
        PreComp_StaticData(obj, data, list, size);
        return;
    }
    F(obj, 0x16, UInt8) |= 4; /* OBJECT_DEFINED */
    if (!fatalerrors) {
        for (scan = list; scan; scan = scan->next)
            CInline_ObjectAddrRef(scan->obj);
        if (filesyminfo)
            CPrep_SetSourceFile(cparser_fileoffset);
#ifdef RECOMP_NEGATIVE_CONTROL_CINIT /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINIT */
        is_readonly = !is_readonly;
#endif
        if (is_readonly)
            ObjGen_DeclareReadOnlyData(obj, data, list, size);
        else
            ObjGen_DeclareData(obj, data, list, size);
        F(obj, 0x10, UInt32) = qual;
    }
}
HOOK(0x45e3c0, declaredata);

void CInit_DeclareData(void *obj, void *data, OLinkList *list, SInt32 size) {
    declaredata(obj, data, list, size, 0);
}
HOOK(0x45e460, CInit_DeclareData);

void CInit_DeclareReadOnlyData(void *obj, void *data, OLinkList *list, SInt32 size) {
    declaredata(obj, data, list, size, 1);
}
HOOK(0x45e480, CInit_DeclareReadOnlyData);
