/*
 * CParser.c — parser helpers of the C/C++ frontend (mweppc src/compiler/frontend/CParser.c),
 * partial port.
 *
 * The functions here are byte-identical between mweppc's GameCube 2.0 compiler and ours. Ported so
 * far: the type-qualifier queries, the CV-qualification comparisons, appendmember, is_unsigned,
 * CParser_NewFuncArg, IsTempName, the unique names and IDs, CParser_FreeLocalHeap and
 * CParser_GlobalCleanup; part 2 adds is_typeequal and is_typesame. The rest stay the original's.
 *
 * Type from our code: type (byte) +0 with array 0xc (target +6), enum 3 (enumtype +0xe); the
 * qualifier word is at +0xe for type 0xa and at +0xa for type 0xb (the exe's jump table).
 * Q_CONST 1, Q_VOLATILE 2. HashNameNode.name +0xa. TypeStruct.members +0xa, member.next +0.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(Bool8, CMach_IsUnsignedType, 0x4590c0, (void *type));
ORIG_FN(void *, galloc, 0x4332e0, (SInt32 size));
ORIG_FN(void, memclrw, 0x4334d0, (void *ptr, SInt32 size));
ORIG_FN(void, CParser_PrintUniqueID, 0x42e970, (char *buf));
ORIG_FN(void *, GetHashNameNodeExport, 0x432ef0, (const char *name));
ORIG_FN(void, CFunc_GenerateSingleExprFunc, 0x474bf0, (void *func, void *expr));
ORIG_FN(void, CFunc_GenerateDummyCtorFunc, 0x474c90, (void *func, void *obj));
ORIG_FN(Bool8, CInline_CanFreeLHeap, 0x466510, (void));
ORIG_FN(void, freelheap, 0x433410, (void));
ORIG_FN(void, CClass_ClassAction, 0x4606f0, (void *tclass));
ORIG_FN(Bool8, CTempl_Instantiate, 0x4779a0, (void));
ORIG_FN(Bool8, CInline_GenerateDeferredFuncs, 0x466540, (void));

#define uniqueid ORIG_VAR(SInt32, 0x54cb44)
#define cparser_sfunclist ORIG_VAR(void **, 0x54cb38)     /* {next, func, obj, expr} */
#define cparser_parentcleanup ORIG_VAR(void **, 0x54cb3c) /* {next, tclass} */
#define cparser_classactions ORIG_VAR(void **, 0x54cb40)  /* {next, tclass} */

static UInt32 type_qual(UInt8 *type, UInt32 qual) {
    while (type[0] == 0xc) /* array */
        type = F(type, 6, UInt8 *);
    if (type[0] == 0xa)
        qual = F(type, 0xe, UInt32);
    else if (type[0] == 0xb)
        qual = F(type, 0xa, UInt32);
    return qual;
}

Bool8 CParser_IsVolatile(UInt8 *type, UInt32 qual) {
    return (type_qual(type, qual) & 2) != 0;
}
HOOK(0x430590, CParser_IsVolatile);

Bool8 CParser_IsConst(UInt8 *type, UInt32 qual) {
#ifdef RECOMP_NEGATIVE_CONTROL_CPARSER /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPARSER */
    return 0;
#endif
    return (type_qual(type, qual) & 1) != 0;
}
HOOK(0x430550, CParser_IsConst);

UInt32 CParser_GetCVTypeQualifiers(UInt8 *type, UInt32 qual) {
    return type_qual(type, qual) & 3;
}
HOOK(0x430510, CParser_GetCVTypeQualifiers);

UInt32 CParser_GetTypeQualifiers(UInt8 *type, UInt32 qual) {
    return type_qual(type, qual);
}
HOOK(0x4304d0, CParser_GetTypeQualifiers);

void appendmember(UInt8 *tstruct, void **member) {
    void **last;
    if (!F(tstruct, 0xa, void **)) {
        F(tstruct, 0xa, void **) = member;
        return;
    }
    for (last = F(tstruct, 0xa, void **); *last; last = (void **)*last)
        ;
    *last = member;
}
HOOK(0x42fec0, appendmember);

Bool8 is_unsigned(UInt8 *type) {
    if (type[0] == 3) /* enum */
        type = F(type, 0xe, UInt8 *);
    return CMach_IsUnsignedType(type);
}
HOOK(0x42fe70, is_unsigned);

Bool8 CParser_IsMoreCVQualified(UInt32 a, UInt32 b) {
    if ((a & 1) && !(b & 1))
        return (a & 2) || (b & 2) == 0;
    if ((a & 2) && !(b & 2))
        return (a & 1) || (b & 1) == 0;
    return 0;
}
HOOK(0x42fda0, CParser_IsMoreCVQualified);

Bool8 CParser_IsSameOrMoreCVQualified(UInt32 a, UInt32 b) {
    if ((a & 3) == (b & 3))
        return 1;
    return CParser_IsMoreCVQualified(a, b);
}
HOOK(0x42fe00, CParser_IsSameOrMoreCVQualified);

void *CParser_NewFuncArg(void) {
    void *arg = galloc(0x18);
    memclrw(arg, 0x18);
    return arg;
}
HOOK(0x42efa0, CParser_NewFuncArg);

Bool8 IsTempName(UInt8 *name) {
    return !name || name[0xa] == '@' || name[0xa] == '$';
}
HOOK(0x42ec70, IsTempName);

void *CParser_GetUniqueName(void) {
    char buf[16];
    buf[0] = '@';
    CParser_PrintUniqueID(buf + 1);
    return GetHashNameNodeExport(buf);
}
HOOK(0x42e9f0, CParser_GetUniqueName);

SInt32 CParser_GetUniqueID(void) {
    return uniqueid++;
}
HOOK(0x42e960, CParser_GetUniqueID);

void CParser_FreeLocalHeap(void) {
    void **s, **p;
    UInt8 *nspace;

    while (cparser_sfunclist) {
        s = cparser_sfunclist;
        cparser_sfunclist = (void **)s[0];
        if (s[3])
            CFunc_GenerateSingleExprFunc(s[1], s[3]);
        else
            CFunc_GenerateDummyCtorFunc(s[1], s[2]);
    }
    if (cparser_parentcleanup) {
        if (!CInline_CanFreeLHeap())
            return;
        for (p = cparser_parentcleanup; p; p = (void **)p[0]) {
            nspace = F(p[1], 6, UInt8 *); /* tclass->nspace */
            while (!F(F(nspace, 0, UInt8 *), 0x1d, Bool8)) /* !nspace->parent->is_global */
                F(nspace, 0, UInt8 *) = F(F(nspace, 0, UInt8 *), 0, UInt8 *);
        }
        cparser_parentcleanup = 0;
    }
    freelheap();
}
HOOK(0x431f90, CParser_FreeLocalHeap);

void CParser_GlobalCleanup(Bool8 runClassActions) {
    Bool8 working;
    do {
        CParser_FreeLocalHeap();
        working = 0;
        if (runClassActions) {
            if (cparser_classactions) {
                CClass_ClassAction(cparser_classactions[1]);
                cparser_classactions = (void **)cparser_classactions[0];
                working = 1;
            } else if (CTempl_Instantiate()) {
                working = 1;
            }
        }
        while (CInline_GenerateDeferredFuncs()) {
            CParser_FreeLocalHeap();
            working = 1;
        }
    } while (working);
}
HOOK(0x432020, CParser_GlobalCleanup);

/* ---- part 2: type equality (is_typeequal, is_typesame) ---- */

/* Type kinds: 0 void, 1 int, 2 float, 3 enum, 4 struct, 5 class (flags (UInt16) +0x26,
   CLASS_IS_TEMPL 0x100), 6 func (args +6, functype +0xe, qual +0x12, flags +0x16; call
   convention mask 0xf0000001), 9 template, 0xa member pointer (ty1 +6, ty2 +0xa, qual +0xe),
   0xb pointer (target +6, qual +0xa), 0xc array (size +2, target +6). Qualifiers compared by
   is_typesame: Q_RESTRICT | Q_REFERENCE | Q_CONST | Q_VOLATILE = 0x200023. copts.cplusplus
   0x57db79, mpwc_relax 0x57db82, cpp_extensions 0x57db85. */
ORIG_FN(Bool8, is_memberpointerequal, 0x42f400, (void *a, void *b));
ORIG_FN(Bool8, is_arglistequal, 0x42f310, (void *a, void *b));
ORIG_FN(Bool8, is_arglistsame, 0x42fa40, (void *a, void *b));
ORIG_FN(Bool8, CTemplTool_TemplDepTypeCompare, 0x47d300, (void *a, void *b));
ORIG_FN(Bool8, CTemplTool_IsSameTemplateType, 0x47eeb0, (void *a, void *b));
ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));

#define stvoid ((UInt8 *)0x532088)
#define copts_cplusplus ORIG_VAR(Bool8, 0x57db79)
#define copts_mpwc_relax ORIG_VAR(Bool8, 0x57db82)
#define copts_cpp_extensions ORIG_VAR(Bool8, 0x57db85)
#define CPARSER_FILE ((const char *)0x5322ac) /* "CParser.c" */
#define KIND(t) ((UInt32)(SInt32)(SInt8)(t)[0])

SInt16 is_typesame(UInt8 *a, UInt8 *b);

SInt16 is_typeequal(UInt8 *a, UInt8 *b) {
    for (;;) {
        if (a[0] != b[0])
            return 0;
        switch (KIND(a)) {
        case 0:
            return 1;
        case 1:
        case 2:
        case 3:
        case 5:
        case 4:
            return a == b;
        case 0xb: /* TYPEPOINTER */
            if (F(a, 6, UInt8 *) == stvoid || F(b, 6, UInt8 *) == stvoid)
                return 1;
            a = F(a, 6, UInt8 *);
            b = F(b, 6, UInt8 *);
            if (copts_mpwc_relax && !copts_cplusplus)
                return 1;
            continue;
        case 0xa: /* TYPEMEMBERPOINTER */
            if (F(a, 0xa, void *) != F(b, 0xa, void *))
                return 0;
            return is_memberpointerequal(F(a, 6, void *), F(b, 6, void *));
        case 0xc: /* TYPEARRAY */
            if (F(a, 2, SInt32) != 0 && F(b, 2, SInt32) != 0 && F(a, 2, SInt32) != F(b, 2, SInt32))
                return 0;
            a = F(a, 6, UInt8 *);
            b = F(b, 6, UInt8 *);
            continue;
        case 6: /* TYPEFUNC */
            if (copts_cplusplus || !copts_cpp_extensions) {
                if (copts_mpwc_relax && !copts_cplusplus) {
                    if (!is_typeequal(F(a, 0xe, UInt8 *), F(b, 0xe, UInt8 *)))
                        return 0;
                } else {
                    if (!is_typesame(F(a, 0xe, UInt8 *), F(b, 0xe, UInt8 *)))
                        return 0;
                }
                if ((F(a, 0x16, UInt32) & 0xf0000001) != (F(b, 0x16, UInt32) & 0xf0000001))
                    return 0;
            }
            return is_arglistequal(F(a, 6, void *), F(b, 6, void *));
        case 9: /* TYPETEMPLATE */
            return CTemplTool_TemplDepTypeCompare(a, b);
        default:
            CError_Internal(CPARSER_FILE, 1441);
            return 0;
        }
    }
}
HOOK(0x42f510, is_typeequal);

SInt16 is_typesame(UInt8 *a, UInt8 *b) {
    for (;;) {
        if (a[0] != b[0]) {
            if (a[0] == 9 && b[0] == 5 && (F(b, 0x26, UInt16) & 0x100))
                return CTemplTool_IsSameTemplateType(b, a);
            if (b[0] == 9 && a[0] == 5 && (F(a, 0x26, UInt16) & 0x100))
                return CTemplTool_IsSameTemplateType(a, b);
            return 0;
        }
        switch (KIND(a)) {
        case 0:
            return 1;
        case 1:
        case 2:
        case 3:
        case 5:
        case 4:
            return a == b;
        case 9:
            return CTemplTool_TemplDepTypeCompare(a, b);
        case 0xb: /* TYPEPOINTER */
#ifdef RECOMP_NEGATIVE_CONTROL_CPARSER2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPARSER2 */
            return 0; /* no two pointer types the same */
#endif
            if ((F(a, 0xa, UInt32) & 0x200023) != (F(b, 0xa, UInt32) & 0x200023))
                return 0;
            a = F(a, 6, UInt8 *);
            b = F(b, 6, UInt8 *);
            continue;
        case 0xa: /* TYPEMEMBERPOINTER */
            if (!is_typesame(F(a, 0xa, UInt8 *), F(b, 0xa, UInt8 *)))
                return 0;
            if ((F(a, 0xe, UInt32) & 0x200023) != (F(b, 0xe, UInt32) & 0x200023))
                return 0;
            return is_memberpointerequal(F(a, 6, void *), F(b, 6, void *));
        case 0xc: /* TYPEARRAY */
            if (F(a, 2, SInt32) != F(b, 2, SInt32))
                return 0;
            a = F(a, 6, UInt8 *);
            b = F(b, 6, UInt8 *);
            continue;
        case 6: /* TYPEFUNC */
            if (!is_typesame(F(a, 0xe, UInt8 *), F(b, 0xe, UInt8 *)))
                return 0;
            if (F(a, 0x12, UInt32) != F(b, 0x12, UInt32))
                return 0;
            if ((F(a, 0x16, UInt32) & 0xf0000001) != (F(b, 0x16, UInt32) & 0xf0000001))
                return 0;
            return is_arglistsame(F(a, 6, void *), F(b, 6, void *));
        default:
            CError_Internal(CPARSER_FILE, 1709);
            return 0;
        }
    }
}
HOOK(0x42fb20, is_typesame);

/* ---- part 3: unique ids, bool type, class registration lists ---- */

/* copts.booltruefalse 0x57db92; stbool 0x536028, stsignedint 0x536060. ParentCleanup and
   ClassAction are {next, tclass} (8 bytes); the parent-cleanup list is on the local heap, the
   class actions on the global heap. */
ORIG_FN(void *, lalloc, 0x433320, (SInt32 size));
#define copts_booltruefalse ORIG_VAR(Bool8, 0x57db92)
#define stbool ((void *)0x536028)
#define stsignedint ((void *)0x536060)

void CParser_SetUniqueID(SInt32 id) {
    uniqueid = id;
}
HOOK(0x42e9e0, CParser_SetUniqueID);

void *CParser_GetBoolType(void) {
#ifdef RECOMP_NEGATIVE_CONTROL_CPARSER3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPARSER3 */
    return stsignedint; /* bool is int in C++ too */
#endif
    if (copts_cplusplus && copts_booltruefalse)
        return stbool;
    return stsignedint;
}
HOOK(0x42fcc0, CParser_GetBoolType);

void CParser_RegisterNonGlobalClass(void *tclass) {
    void **p = lalloc(8);
    p[0] = cparser_parentcleanup;
    p[1] = tclass;
    cparser_parentcleanup = p;
}
HOOK(0x431f10, CParser_RegisterNonGlobalClass);

void CParser_NewClassAction(void *tclass) {
    void **act = galloc(8);
#ifdef RECOMP_NEGATIVE_CONTROL_CPARSER3B /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPARSER3B */
    return; /* class actions dropped */
#endif
    act[0] = cparser_classactions;
    act[1] = tclass;
    cparser_classactions = act;
}
HOOK(0x432250, CParser_NewClassAction);

/* ---- part 4: CFM flags and alias objects (close to mweppc: same logic) ---- */

/* Object (0x40 bytes): datatype +2 (DDATA 0, DALIAS 6), name +8, flags (low byte) +0x16,
   u.alias {object +0x28, member +0x2c, offset +0x30}. DeclInfo.exportflags (byte) +0x43.
   copts.cfm_internal 0x57dbdd, cfm_import 0x57dbde, cfm_export 0x57dbdf, cfm_lib_export
   0x57dbe0. OBJECT_INTERNAL 0x10, OBJECT_IMPORT 0x20, OBJECT_EXPORT 0x40. cscope_current
   0x57d6bc. */
ORIG_FN(void, CScope_AddObject, 0x44b500, (void *nspace, void *name, void *obj));
#define copts_cfm_internal ORIG_VAR(Bool8, 0x57dbdd)
#define copts_cfm_import ORIG_VAR(Bool8, 0x57dbde)
#define copts_cfm_export ORIG_VAR(Bool8, 0x57dbdf)
#define copts_cfm_lib_export ORIG_VAR(Bool8, 0x57dbe0)
#define cscope_current ORIG_VAR(void *, 0x57d6bc)

void CParser_SetCFMFlags(UInt8 *object, UInt8 *declinfo) {
    if (declinfo && declinfo[0x43])
        object[0x16] |= declinfo[0x43];
    if (object[2] == 0) { /* DDATA */
        if (copts_cfm_export)
            object[0x16] |= 0x40;
        if (copts_cfm_internal)
            object[0x16] |= 0x10;
        return;
    }
    if (copts_cfm_internal) {
        object[0x16] |= 0x10;
        return;
    }
    if (copts_cfm_import)
        object[0x16] |= 0x20;
    if (copts_cfm_export)
        object[0x16] |= 0x40;
    if (copts_cfm_lib_export)
        object[0x16] |= 0x60;
}
HOOK(0x42eca0, CParser_SetCFMFlags);

void *CParser_NewAliasObject(UInt8 *object, SInt32 offset) {
    UInt32 *alias = galloc(0x40);
    int i;

    for (i = 0; i < 0x10; i++)
        alias[i] = ((UInt32 *)object)[i];
    ((UInt8 *)alias)[2] = 6; /* DALIAS */
    F(alias, 0x28, void *) = object;
    F(alias, 0x2c, void *) = 0;
#ifdef RECOMP_NEGATIVE_CONTROL_CPARSER4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPARSER4 */
    offset += 4;
#endif
    F(alias, 0x30, SInt32) = offset;
    CScope_AddObject(cscope_current, F(alias, 8, void *), alias);
    return alias;
}
HOOK(0x42ef50, CParser_NewAliasObject);
