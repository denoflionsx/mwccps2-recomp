/*
 * CScope.c — name spaces and name lookup of the C/C++ frontend (mweppc
 * src/compiler/frontend/CScope.c), partial port.
 *
 * The functions here are byte-identical between mweppc's GameCube 2.0 compiler and ours. Ported so
 * far: the name space constructors, CScope_FindName, CScope_AppendName, CScope_IsEmptyNameSpace,
 * CScope_MergeNameSpace, CScope_IsInLocalNameSpace and CScope_InitObjectIterator. The lookup
 * functions (CScope_FindClassMember, CScope_NSIteratorFind, CScope_GetTagType...) stay the
 * original's.
 *
 * NameSpace (0x20 bytes): parent +0, name +4, tparams +0x10, data (list or hash table) +0x14,
 * names +0x18, is_hash +0x1c, is_global +0x1d, is_templ +0x1f. NameSpaceName (0x10 bytes): next +0,
 * name +4, first {next, object} +8. HashNameNode.hashval (SInt16) +8. Iterator 0x10 bytes:
 * nspace +0, nextname +4.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))
#define S(addr) ((const char *)(addr))

ORIG_FN(void *, galloc, 0x4332e0, (SInt32 size));
ORIG_FN(void *, lalloc, 0x433320, (SInt32 size));
ORIG_FN(void, memclrw, 0x4334d0, (void *ptr, SInt32 size));
ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));

#define CSCOPE_FILE S(0x5353d8) /* "CScope.c" */
#define CError_ASSERT(line, cond) \
    if (!(cond))                  \
        CError_Internal(CSCOPE_FILE, line)

Bool8 CScope_IsInLocalNameSpace(UInt8 *nspace) {
    while (nspace) {
        if (!nspace[0x1d] && !nspace[0x1f])
            return 1;
        nspace = F(nspace, 0, UInt8 *);
    }
    return 0;
}
HOOK(0x44af00, CScope_IsInLocalNameSpace);

void *CScope_FindName(UInt8 *nspace, UInt8 *name) {
    void **scan;

    for (scan = F(nspace, 0x10, void **); scan; scan = (void **)scan[0])
        if (scan[1] == name)
            return scan + 2;
#ifdef RECOMP_NEGATIVE_CONTROL_CSCOPE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CSCOPE */
    if (nspace[0x1c])
        return 0; /* nothing found in hash name spaces */
#endif
    if (!nspace[0x1c])
        scan = F(nspace, 0x14, void **);
    else
        scan = F(nspace, 0x14, void ***)[F(name, 8, SInt16) & 1023];
    for (; scan; scan = (void **)scan[0])
        if (scan[1] == name)
            return scan + 2;
    return 0;
}
HOOK(0x44af70, CScope_FindName);

void *CScope_AppendName(UInt8 *nspace, void *name) {
    void **nsname, **scan;

    CError_ASSERT(387, !nspace[0x1c]);
    nsname = nspace[0x1d] ? galloc(0x10) : lalloc(0x10);
    nsname[0] = 0;
    nsname[1] = name;
    nsname[2] = 0;
    nsname[3] = 0;
    if (F(nspace, 0x14, void **)) {
        for (scan = F(nspace, 0x14, void **); scan[0]; scan = (void **)scan[0])
            ;
        scan[0] = nsname;
    } else {
        F(nspace, 0x14, void **) = nsname;
    }
    F(nspace, 0x18, SInt32)++;
    return nsname + 2;
}
HOOK(0x44b0f0, CScope_AppendName);

void *CScope_NewHashNameSpace(void *name) {
    UInt8 *nspace;
    void *buffer;

    buffer = galloc(0x400 * 4);
    memclrw(buffer, 0x400 * 4);
    nspace = galloc(0x20);
    memclrw(nspace, 0x20);
    F(nspace, 4, void *) = name;
    F(nspace, 0x14, void *) = buffer;
    nspace[0x1c] = 1;
    nspace[0x1d] = 1;
    return nspace;
}
HOOK(0x44b3a0, CScope_NewHashNameSpace);

void *CScope_NewListNameSpace(void *name, Bool8 is_global) {
    UInt8 *nspace;
    if (is_global) {
        nspace = galloc(0x20);
        memclrw(nspace, 0x20);
    } else {
        nspace = lalloc(0x20);
        memclrw(nspace, 0x20);
    }
    F(nspace, 4, void *) = name;
    nspace[0x1c] = 0;
    nspace[0x1d] = is_global;
    return nspace;
}
HOOK(0x44b3f0, CScope_NewListNameSpace);

Bool8 CScope_IsEmptyNameSpace(UInt8 *nspace) {
    CError_ASSERT(664, !nspace[0x1c]);
    return F(nspace, 0x14, void *) == 0;
}
HOOK(0x44b480, CScope_IsEmptyNameSpace);

void CScope_MergeNameSpace(UInt8 *dst, UInt8 *src) {
    void **nsname;

    CError_ASSERT(678, !dst[0x1c] && !src[0x1c]);
    if ((nsname = F(dst, 0x14, void **))) {
        while (nsname[0])
            nsname = (void **)nsname[0];
        nsname[0] = F(src, 0x14, void *);
    } else {
        F(dst, 0x14, void *) = F(src, 0x14, void *);
    }
}
HOOK(0x44b4b0, CScope_MergeNameSpace);

void CScope_InitObjectIterator(void **iterator, UInt8 *nspace) {
    memclrw(iterator, 0x10);
    iterator[0] = nspace;
    if (!nspace[0x1c])
        iterator[1] = F(nspace, 0x14, void *);
    else
        iterator[1] = F(nspace, 0x14, void **)[0];
}
HOOK(0x44dcc0, CScope_InitObjectIterator);

/* ---- part 2: lookup results (CScope_NSIteratorFind, CScope_SetupParseNameResult,
   CScope_FindClassMemberObject) ---- */

/* NameResult (0x22 bytes): name +4, type +8, qual +0xc, object +0x10, objects +0x14, is_type
   (byte) +0x20. NameSpaceObjectList: next +0, object +4. Object kinds (byte +0): 1 type (type +2,
   qual +6), 2 type tag (type +2), 3 name space, 5 object. CScopeNSIterator: nspace +0, lookup +4
   (lookup: nspace +4, namespaces +8), result +8. NameSpace.theclass +0xc. Lookup_Object = 0. */
ORIG_FN(Bool8, CScope_FindClassMember, 0x44be00, (UInt8 *result, UInt8 *nspace, UInt8 *name, int lookup));
ORIG_FN(void *, CScope_FindLookupName, 0x44bfb0, (UInt8 *lookup, UInt8 *name, void *type));
ORIG_FN(void, CError_Error, 0x456d50, (int code, ...));

#define OBJKIND(list) (*F(list, 4, UInt8 *))

static void **class_member_list(UInt8 *result, UInt8 *nspace, UInt8 *name) {
    void **list;
    if (!CScope_FindClassMember(result, nspace, name, 0))
        return 0;
    list = F(result, 0x14, void **);
    F(result, 0x14, void *) = 0;
    return list;
}

void *CScope_NSIteratorFind(UInt8 *iterator, UInt8 *name) {
    UInt8 *lookup = F(iterator, 4, UInt8 *);
    UInt8 *nspace;

    if (lookup) {
        if (F(lookup, 8, void *))
            return CScope_FindLookupName(lookup, name, 0);
        nspace = F(lookup, 4, UInt8 *);
    } else {
        nspace = F(iterator, 0, UInt8 *);
    }
    if (F(nspace, 0xc, void *))
        return class_member_list(F(iterator, 8, UInt8 *), nspace, name);
    return CScope_FindName(nspace, name);
}
HOOK(0x44c210, CScope_NSIteratorFind);

Bool8 CScope_SetupParseNameResult(UInt8 *result, void **list, UInt8 *name) {
    UInt8 *obj;

    if (!list[0] || OBJKIND(list[0]) == 2) {
        obj = list[1];
        switch (obj[0]) {
        case 3: /* OT_NAMESPACE */
            CError_Error(0x141);
            return 0;
        case 1: /* OT_TYPE */
            F(result, 8, void *) = F(obj, 2, void *);
            F(result, 0xc, UInt32) = F(obj, 6, UInt32);
            F(result, 0x10, void *) = obj;
            F(result, 4, void *) = name;
            result[0x20] = 1;
            break;
        case 2: /* OT_TYPETAG */
            F(result, 8, void *) = F(obj, 2, void *);
            F(result, 0xc, UInt32) = 0;
            F(result, 0x10, void *) = obj;
            F(result, 4, void *) = name;
            result[0x20] = 1;
            break;
        default:
#ifdef RECOMP_NEGATIVE_CONTROL_CSCOPE2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CSCOPE2 */
            F(result, 0x14, void *) = list; /* single objects reported as overload sets */
            break;
#endif
            F(result, 0x10, void *) = obj;
        }
    } else {
        F(result, 0x14, void *) = list;
    }
    return 1;
}
HOOK(0x44c330, CScope_SetupParseNameResult);

Bool8 CScope_FindClassMemberObject(UInt8 *tclass, UInt8 *result, UInt8 *name) {
    void **list;

    memclrw(result, 0x22);
    list = class_member_list(result, F(tclass, 6, UInt8 *), name);
    if (list && OBJKIND(list) == 5)
        return CScope_SetupParseNameResult(result, list, name);
    return 0;
}
HOOK(0x44dc60, CScope_FindClassMemberObject);

/* ---- part 3: CScope_SetFunctionScope (close to mweppc: same logic) ---- */

/* CScopeSave: current +0, currentclass +4, currentfunc +8, is_member_func (byte) +0xc.
   cscope_current 0x57d6bc, cscope_currentclass 0x57ca28, cscope_currentfunc 0x57d6b0,
   cscope_is_member_func 0x57e034. Object: nspace +4, type +0xc. TypeFunc.flags +0x16
   (FUNC_METHOD 0x10); TypeMethod: theclass +0x1a, is_static (byte) +0x26. TypeClass.nspace +6. */
#define cscope_current ORIG_VAR(void *, 0x57d6bc)
#define cscope_currentclass ORIG_VAR(UInt8 *, 0x57ca28)
#define cscope_currentfunc ORIG_VAR(UInt8 *, 0x57d6b0)
#define cscope_is_member_func ORIG_VAR(Bool8, 0x57e034)

void CScope_SetFunctionScope(UInt8 *function, UInt8 *save) {
    UInt8 *type;

    F(save, 0, void *) = cscope_current;
    F(save, 4, void *) = cscope_currentclass;
    F(save, 8, void *) = cscope_currentfunc;
    save[0xc] = cscope_is_member_func;

    cscope_currentfunc = function;
    cscope_currentclass = 0;
    cscope_is_member_func = 0;

    type = F(function, 0xc, UInt8 *);
    if (F(type, 0x16, UInt32) & 0x10) { /* FUNC_METHOD */
        cscope_currentclass = F(type, 0x1a, UInt8 *);
        cscope_current = F(cscope_currentclass, 6, void *);
#ifdef RECOMP_NEGATIVE_CONTROL_CSCOPE3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CSCOPE3 */
        return; /* member functions see no 'this' */
#endif
        cscope_is_member_func = !F(function, 0xc, UInt8 *)[0x26];
    } else {
        cscope_current = F(function, 4, void *);
    }
}
HOOK(0x44ad60, CScope_SetFunctionScope);

/* ---- part 4: CScope_Setup, CScope_FindQualName, CScope_FindNameSpaceName,
   CScope_AddGlobalObject (not matched by gcmatch, but the same logic as mweppc's) ---- */

/* cscope_root 0x57c900. Object: nspace +4, name +8. */
#define cscope_root ORIG_VAR(UInt8 *, 0x57c900)
ORIG_FN(void, CScope_AddObject, 0x44b500, (void *nspace, void *name, void *obj));

void CScope_Setup(void) {
    cscope_current = cscope_root = CScope_NewHashNameSpace(0);
    cscope_currentclass = 0;
    cscope_currentfunc = 0;
    cscope_is_member_func = 0;
}
HOOK(0x44ac10, CScope_Setup);

/* The name list of a name space to search for name: the list, or the name's hash bucket. */
static void **name_bucket(UInt8 *nspace, UInt8 *name) {
    if (!nspace[0x1c])
        return F(nspace, 0x14, void **);
    return F(nspace, 0x14, void ***)[F(name, 8, SInt16) & 0x3ff];
}

void *CScope_FindQualName(UInt8 *nspace, UInt8 *name) {
    void **scan;

    for (scan = name_bucket(nspace, name); scan; scan = (void **)scan[0])
        if (scan[1] == name)
            return scan + 2;
    return 0;
}
HOOK(0x44afd0, CScope_FindQualName);

void *CScope_FindNameSpaceName(UInt8 *nspace, UInt8 *name) {
    void **scan;

    for (scan = F(nspace, 0x10, void **); scan; scan = (void **)scan[0])
        if (scan[1] == name)
            return scan;
#ifdef RECOMP_NEGATIVE_CONTROL_CSCOPE4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CSCOPE4 */
    return 0; /* only template parameters found */
#endif
    for (scan = name_bucket(nspace, name); scan; scan = (void **)scan[0])
        if (scan[1] == name)
            return scan;
    return 0;
}
HOOK(0x44b010, CScope_FindNameSpaceName);

void CScope_AddGlobalObject(UInt8 *obj) {
    F(obj, 4, void *) = cscope_root;
    CScope_AddObject(cscope_root, F(obj, 8, void *), obj);
}
HOOK(0x44b730, CScope_AddGlobalObject);

/* CScope_InsertName, the name space iterator (CScopeNSIterator: nspace +0, lookup +4, result +8;
   NameResult.is_qualified +0x1d; NameSpace.usings +8, NameSpaceLookupList.next +0),
   CScope_GetLocalTagType and CScope_BuildAssociatedNameSpaceList. */
ORIG_FN(void *, CScope_BuildNameSpaceLookupList, 0x44b750, (void *nspace));
ORIG_FN(void *, CScope_BuildTypeAssociatedNameSpaceList, 0x44b1e0, (void *list, void *type));

void *CScope_InsertName(UInt8 *nspace, UInt8 *name) {
    void **nsname = nspace[0x1d] ? galloc(0x10) : lalloc(0x10);
    void ***nsnptr;

    nsname[1] = name;
    nsname[2] = 0;
    nsname[3] = 0;
    if (nspace[0x1c]) {
        nsnptr = &F(nspace, 0x14, void ***)[F(name, 8, SInt16) & 0x3ff];
        nsname[0] = *nsnptr;
        *nsnptr = nsname;
    } else {
        nsname[0] = F(nspace, 0x14, void *);
        F(nspace, 0x14, void *) = nsname;
    }
    F(nspace, 0x18, SInt32)++;
    return nsname + 2;
}
HOOK(0x44b070, CScope_InsertName);

void CScope_NSIteratorInit(void **iterator, UInt8 *nspace, void *result) {
    if (F(nspace, 8, void *)) { /* usings */
        iterator[0] = 0;
        iterator[1] = CScope_BuildNameSpaceLookupList(nspace);
    } else {
        iterator[0] = nspace;
        iterator[1] = 0;
    }
    iterator[2] = result;
}
HOOK(0x44c860, CScope_NSIteratorInit);

Bool8 CScope_NSIteratorNext(void **iterator) {
    if (iterator[1])
        return (iterator[1] = *(void **)iterator[1]) != 0;
    if (((UInt8 *)iterator[2])[0x1d]) /* is_qualified */
        return 0;
    if ((iterator[0] = *(void **)iterator[0]) != 0) {
        if (F(iterator[0], 8, void *) && !((UInt8 *)iterator[2])[0x1d]) {
            iterator[1] = CScope_BuildNameSpaceLookupList(iterator[0]);
            iterator[0] = 0;
        }
        return 1;
    }
    return 0;
}
HOOK(0x44c7f0, CScope_NSIteratorNext);

void *CScope_GetLocalTagType(UInt8 *nspace, UInt8 *name) {
    void **list;

    for (list = CScope_FindQualName(nspace, name); list; list = list[0])
        if (((UInt8 *)list[1])[0] == 2) /* OT_TYPETAG */
            return F(list[1], 2, void *);
    return 0;
}
HOOK(0x44ddf0, CScope_GetLocalTagType);

void *CScope_BuildAssociatedNameSpaceList(void **enodeList) {
    void *nslist = 0;

    while (enodeList) {
        nslist = CScope_BuildTypeAssociatedNameSpaceList(nslist, F(enodeList[1], 0xa, void *));
        enodeList = enodeList[0];
    }
    return nslist;
}
HOOK(0x44b270, CScope_BuildAssociatedNameSpaceList);

/* ---- part 5: CScope_GetLocalObject (not matched by gcmatch; the same logic as mweppc's) ---- */

ORIG_FN(void *, CScope_GetLocalObjects, 0x44dec0, (void *list));

void *CScope_GetLocalObject(UInt8 *nspace, UInt8 *name) {
    void **list = CScope_FindQualName(nspace, name);

    if (list) {
        switch (((UInt8 *)list[1])[0]) {
        case 5: /* OT_OBJECT */
#ifdef RECOMP_NEGATIVE_CONTROL_CSCOPE5 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CSCOPE5 */
            return 0;
#endif
            return CScope_GetLocalObjects(list);
        case 2: /* OT_TYPETAG */
            break;
        default:
            CError_Error(0x7a, name + 0xa); /* identifier redeclared */
            return 0;
        }
    }
    return 0;
}
HOOK(0x44df30, CScope_GetLocalObject);

/* CScope_PossibleTypeName (the same logic as mweppc's). */
#define copts_cplusplus_s ORIG_VAR(Bool8, 0x57db79)

Bool8 CScope_PossibleTypeName(UInt8 *name) {
    void *iterator[3];
    UInt8 result[0x22];
    void **list;

    memclrw(result, sizeof(result));
    CScope_NSIteratorInit(iterator, cscope_current, result);
    do {
        if ((list = CScope_NSIteratorFind((UInt8 *)iterator, name)) != 0) {
            switch (((UInt8 *)list[1])[0]) {
            case 1: /* OT_TYPE */
            case 3: /* OT_NAMESPACE */
                return 1;
            case 2: /* OT_TYPETAG */
                if (copts_cplusplus_s)
                    return 1;
                break;
            default:
                return 0;
            }
        }
    } while (CScope_NSIteratorNext(iterator));
    return 0;
}
HOOK(0x44dbd0, CScope_PossibleTypeName);

/* CScope_GetTagType and CScope_FindNonClassObject (the same logic as mweppc's). NameResult.nspace
   +0. CScope_NSIteratorNonClassFind 0x44c2c0. */
ORIG_FN(void **, CScope_NSIteratorNonClassFind, 0x44c2c0, (void **iterator, void *name));

void *CScope_GetTagType(UInt8 *nspace, UInt8 *name) {
    void *iterator[3];
    UInt8 result[0x22];
    void **list;

    memclrw(result, sizeof(result));
    CScope_NSIteratorInit(iterator, nspace, result);
    do {
        for (list = CScope_NSIteratorFind((UInt8 *)iterator, name); list; list = list[0])
            if (((UInt8 *)list[1])[0] == 2) /* OT_TYPETAG */
                return F(list[1], 2, void *);
    } while (CScope_NSIteratorNext(iterator));
    return 0;
}
HOOK(0x44c8a0, CScope_GetTagType);

Bool8 CScope_FindNonClassObject(UInt8 *nspace, UInt8 *result, UInt8 *name) {
    void *iterator[3];
    void **list;

    memclrw(result, 0x22);
    CScope_NSIteratorInit(iterator, nspace, result);
    do {
        for (list = CScope_NSIteratorNonClassFind(iterator, name); list; list = list[0]) {
            if (copts_cplusplus_s || ((UInt8 *)list[1])[0] != 2) {
                F(result, 0, void *) = iterator[1] ? F(iterator[1], 4, void *) : iterator[0];
                return CScope_SetupParseNameResult(result, list, name);
            }
        }
    } while (CScope_NSIteratorNext(iterator));
    return 0;
}
HOOK(0x44daa0, CScope_FindNonClassObject);

/* ---- part 6: a list walk (from the exe) ---- */

/* NameSpaceObjectList (next +0, object +4): the last node whose object is not in the previous
   node's object's list (+0xe; nodes next +0, object +4); the head when there is none. */
void **fn_44df80(void **list) {
    void **result = list, **cur = list, **next, **scan;

    while ((next = (void **)cur[0]) != 0) {
        for (scan = F(cur[1], 0xe, void **); scan; scan = (void **)scan[0]) {
            if (next[1] == scan[1])
                break;
        }
#ifndef RECOMP_NEGATIVE_CONTROL_CSCOPE6 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CSCOPE6 */
        if (!scan)
#endif
            result = next;
        cur = next;
    }
    return result;
}
HOOK(0x44df80, fn_44df80);

/* finds obj in the list from fn_44df80's node on; otherwise the list 0x44bf80 gives for obj and
   that node's object, with the node holding that object relinked after the node's successor (an
   internal error when it is missing) */
ORIG_FN(void **, fn_44bf80, 0x44bf80, (void *obj, void *other));

void **fn_44dfc0(void **list, void *obj) {
    void **first, **p, **q, **s;

    if (!list)
        return 0;
    first = fn_44df80(list);
    for (p = first; p; p = (void **)p[0]) {
        if (p[1] == obj)
            return p;
    }
    q = fn_44bf80(obj, first[1]);
    if (!q)
        return 0;
    for (s = q; s; s = (void **)s[0]) {
        if (s[1] == first[1]) {
            s[0] = first[0];
            return q;
        }
    }
    CError_Internal(CSCOPE_FILE, 0xc91);
    return 0;
}
HOOK(0x44dfc0, fn_44dfc0);

/* a list without its alias objects (Object kind 5, datatype 6): the list itself when it has none,
   else a copy (0x44b9c0) with them unlinked */
ORIG_FN(void **, fn_44b9c0, 0x44b9c0, (void **list));

void **fn_44dec0(void **list) {
    void **p, **copy, ***link, **n;

    for (p = list; p; p = (void **)p[0]) {
        UInt8 *obj = (UInt8 *)p[1];
        if (obj[0] == 5 && obj[2] == 6) {
            copy = fn_44b9c0(list);
            link = &copy;
            while ((n = *link) != 0) {
                UInt8 *o = (UInt8 *)n[1];
                if (o[0] == 5 && o[2] == 6)
                    *link = (void **)n[0];
                else
                    link = (void ***)n;
            }
            return copy;
        }
    }
    return list;
}
HOOK(0x44dec0, fn_44dec0);
