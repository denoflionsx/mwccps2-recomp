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
