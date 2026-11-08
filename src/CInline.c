/*
 * CInline.c — inlining and deferred functions of the C/C++ frontend (mweppc
 * src/compiler/frontend/CInline.c), partial port.
 *
 * The functions here are byte-identical between mweppc's GameCube 2.0 compiler and ours. Ported so
 * far: CInline_Init and CInline_CopyExpression. CInline_CopyNodes, CInline_CheckUsage,
 * CInline_GenerateDeferredFuncs and the rest stay the original's.
 *
 * CInlineCopyMode: Local 0, Global 1, Pack 2, Unpack 3, Expand 4.
 */
#include "orig.h"

typedef UInt8 Bool8;

ORIG_FN(void *, CInline_CopyNodes, 0x4620b0, (void *expr));
ORIG_FN(void, CInline_CheckUsage, 0x462590, (void *expr, Bool8 flag));

#define cinline_exportlist ORIG_VAR(void *, 0x54d6f4)
#define cinline_actionlist ORIG_VAR(void *, 0x54d6f0)
#define cinline_tactionlist ORIG_VAR(void *, 0x57ab98)
#define cinline_gendeps ORIG_VAR(Bool8, 0x54d6ea)
#define enode_copymode ORIG_VAR(UInt8, 0x54d75d)
#define enode_idtrans ORIG_VAR(void *, 0x54d758)
#define enode_globalcopy ORIG_VAR(Bool8, 0x54d75c)

void CInline_Init(void) {
    cinline_exportlist = 0;
    cinline_actionlist = 0;
    cinline_tactionlist = 0;
    cinline_gendeps = 0;
}
HOOK(0x4614d0, CInline_Init);

void *CInline_CopyExpression(void *expr, UInt8 mode) {
    enode_copymode = mode;
    switch (mode) {
    case 0: /* CopyModeLocal */
    case 4: /* CopyModeExpand */
        enode_idtrans = 0;
#ifdef RECOMP_NEGATIVE_CONTROL_CINLINE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINLINE */
        enode_globalcopy = 1;
#else
        enode_globalcopy = 0;
#endif
        expr = CInline_CopyNodes(expr);
        break;
    case 3: /* CopyModeUnpack */
        enode_globalcopy = 0;
        expr = CInline_CopyNodes(expr);
        break;
    case 1: /* CopyModeGlobal */
        enode_idtrans = 0;
        enode_globalcopy = 1;
        expr = CInline_CopyNodes(expr);
        break;
    case 2: /* CopyModePack */
        enode_idtrans = 0;
        enode_globalcopy = 1;
        expr = CInline_CopyNodes(expr);
        CInline_CheckUsage(expr, 0);
        break;
    }
    return expr;
}
HOOK(0x462710, CInline_CopyExpression);

/* ---- part 2: small helpers not matched by gcmatch (the same logic as mweppc's) ---- */

/* locals 0x57c904 (ObjectList {next, object}); Object: datatype +2 (DLOCAL 1), name +8, type +0xc,
   qual +0x10, flags (byte) +0x16 (OBJECT_REFERENCED 2). Statement.next +0. CI_Action: next +0,
   actiontype (byte) +0x22 (CI_ActionInlineFunc 0). anyerrors 0x57e03b. */
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))
ORIG_FN(void *, CParser_NewLocalDataObject, 0x42ed80, (void *declinfo, Bool8 add));
ORIG_FN(void *, CParser_GetUniqueName, 0x42e9f0, (void));
ORIG_FN(void, CInline_SetObjectSFlags, 0x462800, (void *object, UInt8 sflags));
ORIG_FN(void, CFunc_SetupLocalVarInfo, 0x4704d0, (void *object));
ORIG_FN(Bool8, is_const_object, 0x4305d0, (void *obj));
ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));
#define locals ORIG_VAR(void **, 0x57c904)
#define anyerrors ORIG_VAR(Bool8, 0x57e03b)

SInt32 CInline_GetLocalID(void *object) {
    void **list;
    SInt32 counter = 0;

    for (list = locals; list; list = list[0]) {
        if (((UInt8 *)list[1])[2] == 1) { /* DLOCAL */
            if (list[1] == object)
                return counter;
            counter++;
        }
    }
    return -1;
}
HOOK(0x461ce0, CInline_GetLocalID);

/* An inlined function's object reference: may its value change? (Inline in mweppc's
   CInline_IsSimpleExpression-style checks; a function of its own in ours.) */
Bool8 CInline_IsChangeableObject(UInt8 *object) {
    if (object[2] == 1 && !(object[0x16] & 2))
        return 0; /* a local whose address is never taken */
    if (is_const_object(object))
        return 0;
    return 1;
}
HOOK(0x461dd0, CInline_IsChangeableObject);

void *CInline_NewLocalObject(void *type, SInt16 qual, UInt8 sflags) {
    UInt8 *object = CParser_NewLocalDataObject(0, 1);
    F(object, 8, void *) = CParser_GetUniqueName();
    F(object, 0xc, void *) = type;
    F(object, 0x10, SInt32) = qual;
    CInline_SetObjectSFlags(object, sflags);
    CFunc_SetupLocalVarInfo(object);
    return object;
}
HOOK(0x462860, CInline_NewLocalObject);

SInt16 CInline_GetStatementNumber(void **first, void *stmt) {
    SInt16 number = 0;

    while (first) {
        if (first == stmt)
            return number;
        first = first[0];
        number++;
    }
    CError_Internal((const char *)0x536990 /* "CInline.c" */, 3517);
    return 0;
}
HOOK(0x464e00, CInline_GetStatementNumber);

Bool8 CInline_CanFreeLHeap(void) {
    void **action;

#ifdef RECOMP_NEGATIVE_CONTROL_CINLINE2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CINLINE2 */
    *(volatile int *)0 = 0; /* crash: freeing the heap early is not caught */
#endif
    if (!anyerrors) {
        for (action = cinline_actionlist; action; action = action[0])
            if (((UInt8 *)action)[0x22] == 0) /* CI_ActionInlineFunc */
                return 0;
    }
    return 1;
}
HOOK(0x466510, CInline_CanFreeLHeap);
