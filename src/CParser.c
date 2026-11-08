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

/* ---- part 5: TypedefDeclInfo (close to mweppc: same logic) ---- */

/* DeclInfo: thetype +0, qual +4, isType (byte) +0x49. TypePointer 0xe bytes (qual +0xa),
   TypeMemberPointer 0x12 bytes (qual +0xe), TypeArray size +2. Q_RESTRICT | Q_REFERENCE |
   Q_VOLATILE | Q_CONST = 0x200023; Q_ALIGNED_MASK | Q_REFERENCE | Q_PASCAL | Q_VOLATILE | Q_CONST
   = 0x1e00002b. */
static void *copy_type(const void *type, int size) {
    UInt8 *t = galloc(size);
    int i;
    for (i = 0; i < size; i++)
        t[i] = ((const UInt8 *)type)[i];
    return t;
}

void TypedefDeclInfo(UInt8 *declinfo, UInt8 *type, UInt32 qual) {
    if (type[0] == 0xb) { /* TYPEPOINTER */
        if (F(type, 0xa, UInt32) & 0x20) { /* reference */
            F(declinfo, 0, void *) = type;
            F(declinfo, 4, UInt32) &= ~0x200023;
            F(declinfo, 4, UInt32) |= qual;
            return;
        }
        F(declinfo, 0, void *) = copy_type(type, 0xe);
        F(F(declinfo, 0, UInt8 *), 0xa, UInt32) |= F(declinfo, 4, UInt32) & 0x200023;
        F(declinfo, 4, UInt32) &= ~0x200023;
        F(declinfo, 4, UInt32) |= qual & 0x1e00002b;
    } else if (type[0] == 0xa) { /* TYPEMEMBERPOINTER */
        F(declinfo, 0, void *) = copy_type(type, 0x12);
        F(F(declinfo, 0, UInt8 *), 0xe, UInt32) |= F(declinfo, 4, UInt32) & 0x200023;
        F(declinfo, 4, UInt32) &= ~0x200023;
        F(declinfo, 4, UInt32) |= qual & 0x1e00002b;
    } else {
        F(declinfo, 0, void *) = type;
        F(declinfo, 4, UInt32) |= qual & 0x1e00002b;
        if (type[0] == 0xc && !F(type, 2, SInt32))
            F(declinfo, 0, void *) = copy_type(type, 0xe);
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CPARSER5 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPARSER5 */
    F(declinfo, 4, UInt32) &= ~0x1; /* typedef'd const dropped */
#endif
    declinfo[0x49] = 1;
}
HOOK(0x4308c0, TypedefDeclInfo);

/* ---- part 6: object constructors, unique names, linkage and qualifier queries (not matched by
   gcmatch; the same logic as mweppc's, with our simpler CParser_UpdateObject) ---- */

/* Object: otype +0, access +1, datatype +2 (DDATA 0, DLOCAL 1, DFUNC 3, DVFUNC 4), section +3,
   nspace +4, name +8, type +0xc, qual +0x10 (Q_INLINE 0x10, Q_IMPLICIT_WEAK 0x20000, Q_WEAK
   0x40000, Q_MANGLE_NAME 0x80000), sclass (short) +0x14 (TK_STATIC 0x102). DeclInfo: thetype +0,
   qual +4, name +0xc, storageclass (short) +0x40, section (byte) +0x42, is_extern_c +0x4e.
   NameSpace.is_unnamed +0x1e. TypeMethod: theclass +0x1a, vtbl_index +0x1e. locals 0x57c904. 0x42efc0
   (a switch over basic types) is split by x86map into three "functions" and is not hooked. */
ORIG_FN(void, CodeGen_UpdateObject, 0x437980, (void *obj));
ORIG_FN(void, CTempl_Cleanup, 0x474f10, (void));
ORIG_FN(void, CParser_Cleanup2, 0x4612a0, (void));
ORIG_FN(void, CScope_Cleanup, 0x44ac40, (void));
ORIG_FN(void, FreeGList, 0x432ae0, (void *gl));
#define name_mangle_list ((void *)0x56acd8)
#define locals ORIG_VAR(void **, 0x57c904)

void CParser_UpdateObject(UInt8 *object, UInt8 *declinfo) {
    if (declinfo && declinfo[0x42])
        object[3] = declinfo[0x42];
    CParser_SetCFMFlags(object, declinfo);
    CodeGen_UpdateObject(object);
}
HOOK(0x42ed10, CParser_UpdateObject);

void *CParser_NewObject(UInt8 *declinfo) {
    UInt8 *object = galloc(0x40);
    memclrw(object, 0x40);
    CParser_SetCFMFlags(object, declinfo);
    object[0] = 5; /* OT_OBJECT */
    object[1] = 0; /* ACCESSPUBLIC */
    object[3] = 0; /* SECT_DEFAULT */
    return object;
}
HOOK(0x42ed40, CParser_NewObject);

static void copy_declinfo(UInt8 *object, UInt8 *declinfo) {
    F(object, 0xc, void *) = F(declinfo, 0, void *);
    F(object, 8, void *) = F(declinfo, 0xc, void *);
    F(object, 0x10, UInt32) = F(declinfo, 4, UInt32);
    F(object, 0x14, SInt16) = F(declinfo, 0x40, SInt16);
}

void *CParser_NewLocalDataObject(UInt8 *declinfo, Bool8 add_to_locals) {
    UInt8 *object = lalloc(0x40);
    void **list;

    memclrw(object, 0x40);
    object[0] = 5;
    object[1] = 0;
    object[2] = 1; /* DLOCAL */
    if (declinfo)
        copy_declinfo(object, declinfo);
    if (add_to_locals) {
        list = lalloc(8);
        list[1] = object;
        list[0] = locals;
        locals = list;
    }
    return object;
}
HOOK(0x42ed80, CParser_NewLocalDataObject);

static void *new_global_object(UInt8 *declinfo, UInt8 datatype) {
    UInt8 *object = galloc(0x40);

    memclrw(object, 0x40);
    object[0] = 5;
    object[1] = 0;
    object[3] = 0;
    object[2] = datatype;
    F(object, 4, void *) = cscope_current;
    if (declinfo) {
        copy_declinfo(object, declinfo);
        if (copts_cplusplus && !declinfo[0x4e])
            F(object, 0x10, UInt32) |= 0x80000; /* Q_MANGLE_NAME */
    }
    CParser_UpdateObject(object, declinfo);
    return object;
}

void *CParser_NewGlobalDataObject(UInt8 *declinfo) {
    return new_global_object(declinfo, 0); /* DDATA */
}
HOOK(0x42edf0, CParser_NewGlobalDataObject);

void *CParser_NewFunctionObject(UInt8 *declinfo) {
#ifdef RECOMP_NEGATIVE_CONTROL_CPARSER6 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPARSER6 */
    UInt8 *object = new_global_object(declinfo, 3);
    F(object, 0x10, UInt32) &= ~0x80000; /* C++ function names left unmangled */
    return object;
#endif
    return new_global_object(declinfo, 3); /* DFUNC */
}
HOOK(0x42eea0, CParser_NewFunctionObject);

void CParser_Cleanup(void) {
    CTempl_Cleanup();
    CParser_Cleanup2();
    CScope_Cleanup();
    FreeGList(name_mangle_list);
}
HOOK(0x42e6f0, CParser_Cleanup);

void CParser_PrintUniqueID_(char *buf) {
    char mybuf[16];
    char *ptr = mybuf;
    SInt32 id = CParser_GetUniqueID();

    while (id) {
        *(ptr++) = '0' + id % 10;
        id /= 10;
    }
    while (ptr > mybuf)
        *(buf++) = *(--ptr);
    *buf = 0;
}
HOOK(0x42e970, CParser_PrintUniqueID_);

void *CParser_AppendUniqueName(const char *prefix) {
    char buf[256];
    char *dst = buf;
    int i;

    for (i = 0; *prefix && i < 240; i++)
        *(dst++) = *(prefix++);
    *(dst++) = '$';
    CParser_PrintUniqueID_(dst);
    return GetHashNameNodeExport(buf);
}
HOOK(0x42ea90, CParser_AppendUniqueName);

Bool8 is_const_object(UInt8 *obj) {
    return CParser_IsConst(F(obj, 0xc, UInt8 *), F(obj, 0x10, UInt32));
}
HOOK(0x4305d0, is_const_object);

Bool8 is_volatile_object(UInt8 *obj) {
    return CParser_IsVolatile(F(obj, 0xc, UInt8 *), F(obj, 0x10, UInt32));
}
HOOK(0x4305f0, is_volatile_object);

Bool8 CParserIsConstExpr(UInt8 *expr) {
    return CParser_IsConst(F(expr, 0xa, UInt8 *), F(expr, 2, UInt16) & 3);
}
HOOK(0x430610, CParserIsConstExpr);

Bool8 CParserIsVolatileExpr(UInt8 *expr) {
    return CParser_IsVolatile(F(expr, 0xa, UInt8 *), F(expr, 2, UInt16) & 3);
}
HOOK(0x430630, CParserIsVolatileExpr);

Bool8 CParser_HasInternalLinkage(UInt8 *obj) {
    UInt8 *nspace;

    for (nspace = F(obj, 4, UInt8 *); nspace; nspace = F(nspace, 0, UInt8 *))
        if (nspace[0x1e])
            return 1;
    if (obj[2] == 1) /* DLOCAL */
        return 1;
    if (F(obj, 0x10, UInt32) & 0x60000)
        return 0;
    if (F(obj, 0x14, SInt16) == 0x102) /* static */
        return 1;
    if (F(obj, 0x10, UInt32) & 0x10) /* inline */
        F(obj, 0x10, UInt32) |= 0x20000;
    return 0;
}
HOOK(0x430650, CParser_HasInternalLinkage);

Bool8 CParser_IsVirtualFunction(UInt8 *obj, void **tclass, SInt32 *index) {
    if (obj[2] == 4) { /* DVFUNC */
        *tclass = F(F(obj, 0xc, UInt8 *), 0x1a, void *);
        *index = F(F(obj, 0xc, UInt8 *), 0x1e, SInt32);
        return 1;
    }
    return 0;
}
HOOK(0x4306f0, CParser_IsVirtualFunction);

/* cscope_root 0x57c900. */
static void *new_compiler_def_object(UInt8 datatype) {
    UInt8 *object = galloc(0x40);
    memclrw(object, 0x40);
    object[0] = 5;
    object[1] = 0;
    object[3] = 0;
    object[2] = datatype;
    F(object, 4, void *) = *(void **)0x57c900; /* cscope_root */
    return object;
}

void *CParser_NewCompilerDefDataObject(void) {
    return new_compiler_def_object(0); /* DDATA */
}
HOOK(0x42ee70, CParser_NewCompilerDefDataObject);

void *CParser_NewCompilerDefFunctionObject(void) {
    return new_compiler_def_object(3); /* DFUNC */
}
HOOK(0x42ef20, CParser_NewCompilerDefFunctionObject);

/* ParserTryBlock: next +0, cscope_current +0x2c, cscope_currentclass +0x30, cscope_currentfunc
   +0x34, ctempl_curinstance +0x38, cerror_locktoken +0x3c, cscope_is_member_func (byte) +0x40.
   trychain 0x57d6b8. */
#define trychain ORIG_VAR(void *, 0x57d6b8)
#define p_cscope_currentclass ORIG_VAR(void *, 0x57ca28)
#define p_cscope_currentfunc ORIG_VAR(void *, 0x57d6b0)
#define p_ctempl_curinstance ORIG_VAR(void *, 0x57ab94)
#define p_cerror_locktoken ORIG_VAR(void *, 0x57c7e4)
#define p_cscope_is_member_func ORIG_VAR(Bool8, 0x57e034)

void CParser_InsertTryBlock(UInt8 *block) {
    F(block, 0x2c, void *) = cscope_current;
    F(block, 0x30, void *) = p_cscope_currentclass;
    F(block, 0x34, void *) = p_cscope_currentfunc;
    F(block, 0x38, void *) = p_ctempl_curinstance;
    F(block, 0x3c, void *) = p_cerror_locktoken;
    block[0x40] = p_cscope_is_member_func;
    F(block, 0, void *) = trychain;
    trychain = block;
}
HOOK(0x42fee0, CParser_InsertTryBlock);

void CParser_RemoveTryBlock(UInt8 *block) {
    cscope_current = F(block, 0x2c, void *);
    p_cscope_currentclass = F(block, 0x30, void *);
    p_cscope_currentfunc = F(block, 0x34, void *);
    p_ctempl_curinstance = F(block, 0x38, void *);
#ifdef RECOMP_NEGATIVE_CONTROL_CPARSER6B /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPARSER6B */
    *(volatile int *)0 = 0; /* crash: not restoring the lock token is not caught */
#endif
    p_cerror_locktoken = F(block, 0x3c, void *);
    p_cscope_is_member_func = block[0x40];
    trychain = F(block, 0, void *);
}
HOOK(0x42ff30, CParser_RemoveTryBlock);

int is_pascal_object(UInt8 *obj) {
    UInt8 *type = F(obj, 0xc, UInt8 *);
    return type[0] == 6 && (F(type, 0x16, UInt32) & 1) ? 1 : 0; /* FUNC_PASCAL */
}
HOOK(0x430730, is_pascal_object);

ORIG_FN(UInt32, strlen_, 0x404ee0, (const char *s));

void *CParser_NameConcat(const char *a, const char *b) {
    char mybuf[256];
    char *buf, *dst;
    UInt32 len = strlen_(a) + strlen_(b);

    if (len > sizeof(mybuf) - 1)
        dst = buf = lalloc(len + 1);
    else
        dst = buf = mybuf;
    while (*a)
        *(dst++) = *(a++);
    while (*b)
        *(dst++) = *(b++);
    *dst = 0;
    return GetHashNameNodeExport(buf);
}
HOOK(0x42ea10, CParser_NameConcat);

/* CallbackAction (0xc bytes): next +0, obj +4, tclass +8; callbackactions 0x57cea0. Object.flags
   +0x16 (OBJECT_LAZY 8). TypeClass: classname +0xa, mode (byte) +0x28 (CLASS_MODE_UNION 1). */
#define callbackactions ORIG_VAR(void **, 0x57cea0)

void CParser_NewCallBackAction(UInt8 *obj, void *tclass) {
    void **act = galloc(0xc);
    act[0] = callbackactions;
    act[1] = obj;
    act[2] = tclass;
    callbackactions = act;
    obj[0x16] |= 8;
}
HOOK(0x432220, CParser_NewCallBackAction);

void CParser_CallBackAction(void *obj) {
    void **act;

    for (act = callbackactions; act; act = act[0]) {
        if (act[1] == obj) {
            CParser_NewClassAction(act[2]);
            return;
        }
    }
    CError_Internal(CPARSER_FILE, 4551);
}
HOOK(0x432270, CParser_CallBackAction);

Bool8 CParser_IsAnonymousUnion(UInt8 **di, Bool8 allowStruct) {
    UInt8 *type = di[0];
    return type[0] == 5 && (type[0x28] == 1 || (allowStruct && copts_cpp_extensions)) &&
           IsTempName(F(type, 0xa, void *));
}
HOOK(0x432080, CParser_IsAnonymousUnion);
