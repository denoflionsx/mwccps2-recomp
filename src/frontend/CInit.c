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

/* ---- part 2: small helpers not matched by gcmatch (the same logic as mweppc's) ---- */

/* copts.explicit_zero_data 0x57dbd4. Type kinds: 4 struct, 5 class, 0xc array (target +6); size
   +2. Object: datatype +2 (DALIAS 6), type +0xc, u.alias {object +0x28, offset +0x30}. */
ORIG_FN(void *, CClass_Constructor, 0x45edd0, (void *tclass));
ORIG_FN(void *, CClass_Destructor, 0x45ee10, (void *tclass));
ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));
#define explicit_zero_data ORIG_VAR(Bool8, 0x57dbd4)

Bool8 CInit_IsAllZero(const char *buf, SInt32 size) {
    SInt32 i;

    if (explicit_zero_data)
        return 0;
    for (i = 0; i < size; i++)
        if (buf[i])
            return 0;
    return 1;
}
HOOK(0x459a60, CInit_IsAllZero);

Bool8 CInit_ClassNeedsConstruction(void *tclass) {
    return CClass_Constructor(tclass) || CClass_Destructor(tclass);
}
HOOK(0x459a90, CInit_ClassNeedsConstruction);

Bool8 CInit_IsSimpleStructArrayInit(UInt8 *type) {
    switch (type[0]) {
    case 4: /* TYPESTRUCT */
        return 1;
    case 0xc: /* TYPEARRAY */
        while (type[0] == 0xc)
            type = *(UInt8 **)(type + 6);
        if (type[0] != 5)
            return 1;
        /* fall through */
    case 5: /* TYPECLASS */
#ifdef RECOMP_NEGATIVE_CONTROL_CINIT2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINIT2 */
        return 1; /* classes initialized as plain data */
#endif
        return !CInit_ClassNeedsConstruction(type);
    default:
        return 0;
    }
}
HOOK(0x459ac0, CInit_IsSimpleStructArrayInit);

UInt8 *CInit_GetInitObject(UInt8 *obj) {
    if (obj[2] == 6) { /* DALIAS */
        if (*(SInt32 *)(obj + 0x30))
            CError_Internal((const char *)0x536460 /* "CInit.c" */, 521);
        obj = *(UInt8 **)(obj + 0x28);
    }
    return obj;
}
HOOK(0x459b10, CInit_GetInitObject);

SInt32 CInit_AdjustObjectDataSize(UInt8 *obj) {
    SInt32 size = *(SInt32 *)(*(UInt8 **)(obj + 0xc) + 2);

    if (size <= 1)
        return size;
    if (size & 1)
        return size + 1;
    return size;
}
HOOK(0x45c330, CInit_AdjustObjectDataSize);

/* ---- part 3: tentative definitions and the deferred data list (from the exe) ---- */

/* cinit_tentative 0x54d662 (ObjectList: next +0, object +4): each object not yet defined (flags
   +0x16 & 4) gets zeroed data of its type's size; then the list is emptied */
void CInit_DefineTentativeData(void) {
    UInt8 **list;

#ifdef RECOMP_NEGATIVE_CONTROL_CINIT3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINIT3 */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "CINIT3@"); /* marks runs */
#endif
    for (list = ORIG_VAR(UInt8 **, 0x54d662); list; list = (UInt8 **)list[0]) {
        UInt8 *obj = list[1];
        if (!(obj[0x16] & 4))
            CInit_DeclareData(obj, 0, 0, F(F(obj, 0xc, UInt8 *), 2, SInt32));
    }
    ORIG_VAR(void *, 0x54d662) = 0;
}
HOOK(0x45e4a0, CInit_DefineTentativeData);

/* the list 0x54d6ba (next +0, object +4, four arguments +8..+0x14): each object is marked defined
   and handed to 0x437770 */
ORIG_FN(void, fn_437770, 0x437770, (void *obj, void *a, void *b, void *c, void *d));

void fn_45e500(void) {
    void **e;

    for (e = ORIG_VAR(void **, 0x54d6ba); e; e = e[0]) {
        ((UInt8 *)e[1])[0x16] |= 4;
        fn_437770(e[1], e[2], e[3], e[4], e[5]);
    }
}
HOOK(0x45e500, fn_45e500);
