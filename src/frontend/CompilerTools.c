/*
 * CompilerTools.c — the frontend's heaps, growable lists (GList) and name hash, partial port.
 *
 * Port of mweppc's src/frontend/CompilerTools.c (CC0, github.com/doldecomp/mweppc). 16 of its
 * functions are in our exe as functions of their own, all byte-identical between their GameCube
 * 2.0 compiler and ours; the 15 run by the corpus are ported (CompilerGetCString, run by 19
 * cases, too). CHash, GListError and MoreHeapSpace are not in the map and stay the original's.
 *
 * HeapMem (0x14 bytes): curfreep +0xc, curfree +0x10; gheap 0x54cba6, lheap 0x54cb92,
 * oheap 0x54cb6a. GList: data (handle) +0, size +4, hndlsize +8, growsize +0xc.
 * HashNameNode: next +0, id +4, hashval (short) +8, name +0xa; header 0xc bytes.
 * AppendGListName compares unsigned (size_t), the others signed, as the original does.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef struct HeapMem {
    UInt8 pad[0xc];
    char *curfreep;
    SInt32 curfree;
} __attribute__((packed)) HeapMem;
typedef struct GList {
    char **data;
    SInt32 size;
    SInt32 hndlsize;
    SInt32 growsize;
} GList;
typedef struct HashNameNode {
    struct HashNameNode *next;
    SInt32 id;
    SInt16 hashval;
    char name[1];
} HashNameNode;

ORIG_FN(void, MoreHeapSpace, 0x433070, (HeapMem *heap, SInt32 size));
ORIG_FN(SInt16, CHash, 0x432dc0, (const char *string));
ORIG_FN(void, GListError, 0x432a80, (void));
ORIG_FN(UInt32, orig_strlen, 0x404ee0, (const char *s));
ORIG_FN(char *, orig_strcpy, 0x404f00, (char *d, const char *s));
ORIG_FN(int, orig_strcmp, 0x404f80, (const char *a, const char *b));
ORIG_FN(void *, orig_memcpy, 0x404d00, (void *d, const void *s, UInt32 n));
ORIG_FN(void *, orig_memset, 0x404d40, (void *d, int c, UInt32 n));
ORIG_FN(void, COS_GetString, 0x441890, (char *buf, SInt16 strlistid, SInt16 index));
ORIG_FN(char **, COS_NewOSHandle, 0x4417e0, (SInt32 size));
ORIG_FN(char **, COS_NewHandle, 0x4417d0, (SInt32 size));
ORIG_FN(void, COS_FreeHandle, 0x441820, (char **h));
ORIG_FN(void, COS_LockHandleHi, 0x441860, (char **h));
ORIG_FN(void, COS_UnlockHandle, 0x441870, (char **h));
ORIG_FN(Bool8, COS_ResizeHandle, 0x441830, (char **h, SInt32 size));

#define gheap ((HeapMem *)0x54cba6)
#define lheap ((HeapMem *)0x54cb92)
#define oheap ((HeapMem *)0x54cb6a)
#define lheaplockcount ORIG_VAR(SInt16, 0x54cb50)
#define hash_name_id ORIG_VAR(SInt32, 0x57c784)
#define name_hash_nodes ORIG_VAR(HashNameNode **, 0x57c8d0)
#define HASHNODE_SIZE 0xc

void *galloc_(SInt32 size);

void CompilerGetCString(SInt16 index, char *string) {
    COS_GetString(string, 10100, index);
}
HOOK(0x432a20, CompilerGetCString);

SInt16 InitGList(GList *gl, SInt32 size) {
    if ((gl->data = COS_NewOSHandle(size)) == 0)
        if ((gl->data = COS_NewHandle(size)) == 0)
            return -1;
    gl->size = 0;
    gl->growsize = size >> 1;
    gl->hndlsize = size;
    return 0;
}
HOOK(0x432a90, InitGList);

void FreeGList(GList *gl) {
    if (gl->data) {
        COS_FreeHandle(gl->data);
        gl->data = 0;
    }
    gl->size = gl->hndlsize = 0;
}
HOOK(0x432ae0, FreeGList);

void ShrinkGList(GList *gl) {
    COS_ResizeHandle(gl->data, gl->hndlsize = gl->size);
}
HOOK(0x432b30, ShrinkGList);

void LockGList(GList *gl) {
    COS_LockHandleHi(gl->data);
}
HOOK(0x432b10, LockGList);

void UnlockGList(GList *gl) {
    COS_UnlockHandle(gl->data);
}
HOOK(0x432b20, UnlockGList);

void AppendGListData(GList *gl, const void *data, SInt32 size) {
    if (gl->size + size > gl->hndlsize)
        if (!COS_ResizeHandle(gl->data, gl->hndlsize += size + gl->growsize))
            GListError();
    orig_memcpy(*gl->data + gl->size, data, size);
    gl->size += size;
}
HOOK(0x432b50, AppendGListData);

void AppendGListByte(GList *gl, SInt8 thebyte) {
    if (gl->size + 1 > gl->hndlsize)
        if (!COS_ResizeHandle(gl->data, gl->hndlsize += 1 + gl->growsize))
            GListError();
    (*gl->data)[gl->size++] = thebyte;
}
HOOK(0x432be0, AppendGListByte);

void AppendGListName(GList *gl, const char *name) {
    UInt32 n = orig_strlen(name);
    if ((UInt32)gl->size + n > (UInt32)gl->hndlsize)
        if (!COS_ResizeHandle(gl->data, gl->hndlsize += n + gl->growsize))
            GListError();
    orig_memcpy(*gl->data + gl->size, name, n);
    gl->size += n;
}
HOOK(0x432d60, AppendGListName);

HashNameNode *GetHashNameNode(const char *name) {
    HashNameNode *node;
    SInt16 hashval;
    if ((node = name_hash_nodes[hashval = CHash(name)]) == 0) {
        name_hash_nodes[hashval] = node = galloc_(orig_strlen(name) + HASHNODE_SIZE);
        node->next = 0;
        node->id = hash_name_id++;
        node->hashval = hashval;
        orig_strcpy(node->name, name);
        return node;
    }
    for (;;) {
        if (orig_strcmp(name, node->name) == 0) {
            if (node->id < 0)
                node->id = hash_name_id++;
            return node;
        }
        if (!node->next) {
            node->next = galloc_(orig_strlen(name) + HASHNODE_SIZE);
            node = node->next;
            node->next = 0;
            node->id = hash_name_id++;
            node->hashval = hashval;
            orig_strcpy(node->name, name);
            return node;
        }
        node = node->next;
    }
}
HOOK(0x432e10, GetHashNameNode);

HashNameNode *GetHashNameNodeExport(const char *name) {
    HashNameNode *node;
    SInt16 hashval;
    if ((node = name_hash_nodes[hashval = CHash(name)]) == 0) {
        node = galloc_(orig_strlen(name) + HASHNODE_SIZE);
        name_hash_nodes[hashval] = node;
        node->next = 0;
        node->id = -1;
        node->hashval = hashval;
        orig_strcpy(node->name, name);
        return node;
    }
    for (;;) {
        if (orig_strcmp(name, node->name) == 0)
            return node;
        if (!node->next) {
            node->next = galloc_(orig_strlen(name) + HASHNODE_SIZE);
            node = node->next;
            node->next = 0;
            node->id = -1;
            node->hashval = hashval;
            orig_strcpy(node->name, name);
            return node;
        }
        node = node->next;
    }
}
HOOK(0x432ef0, GetHashNameNodeExport);

#define HEAP_ALLOC(heap)                                                                           \
    char *cp;                                                                                      \
    size = (size & ~7) + 8;                                                                        \
    if ((heap)->curfree < size)                                                                    \
        MoreHeapSpace((heap), size);                                                               \
    (heap)->curfree -= size;                                                                       \
    cp = (heap)->curfreep;                                                                         \
    (heap)->curfreep += size;                                                                      \
    return cp

void *galloc_(SInt32 size) {
    HEAP_ALLOC(gheap);
}
HOOK(0x4332e0, galloc_);

void *lalloc(SInt32 size) {
    HEAP_ALLOC(lheap);
}
HOOK(0x433320, lalloc);

void *oalloc(SInt32 size) {
    HEAP_ALLOC(oheap);
}
HOOK(0x4333a0, oalloc);

/* aheap 0x54cb7e (between oheap and lheap). */
#define aheap ((HeapMem *)0x54cb7e)

void *aalloc(SInt32 size) {
#ifdef RECOMP_NEGATIVE_CONTROL_COMPILERTOOLS2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_COMPILERTOOLS2 */
    size = 0; /* every allocation the same block */
#endif
    HEAP_ALLOC(aheap);
}
HOOK(0x433360, aalloc);

void locklheap(void) {
    lheaplockcount++;
}
HOOK(0x4333e0, locklheap);

void unlocklheap(void) {
    if (lheaplockcount > 0)
        --lheaplockcount;
}
HOOK(0x4333f0, unlocklheap);

void memclrw(void *ptr, SInt32 size) {
    orig_memset(ptr, 0, size);
}
HOOK(0x4334d0, memclrw);

SInt16 getbit(SInt32 mask) {
    UInt32 m = (UInt32)mask;
    SInt16 bit;
    if (m == 0)
        return -1;
    if (m & (m - 1))
        return -2; /* not a single bit */
    for (bit = 0; !(m & 1); bit++)
        m >>= 1;
#ifdef RECOMP_NEGATIVE_CONTROL_COMPILERTOOLS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_COMPILERTOOLS */
    if (bit == 2)
        bit = 3;
#endif
    return bit;
}
HOOK(0x4334f0, getbit);

/* ---- part 3: CHash and InitNameHash (not matched by gcmatch; the same logic as mweppc's) ---- */

SInt16 CHash_(const char *string) {
    SInt16 i, hashval;
    UInt8 u;

    if ((hashval = (SInt16)(orig_strlen(string) & 0xff)) != 0) {
        i = hashval;
        u = 0;
        while (i > 0) {
            u = (UInt8)((u >> 3) | (u << 5));
            u += *(string++);
            --i;
        }
        hashval = (SInt16)((hashval << 8) | u);
    }
    return hashval & 0x7ff;
}
HOOK(0x432dc0, CHash_);

void InitNameHash(void) {
    name_hash_nodes = galloc_(2048 * sizeof(void *));
    memclrw(name_hash_nodes, 2048 * sizeof(void *));
    hash_name_id = 1;
}
HOOK(0x432fe0, InitNameHash);

/* ---- part 3: heap management and small helpers (from the exe; mweppc's names) ---- */

/* HeapMem: blocks +0, allocsize +4, curblock +8, curfreep +0xc, curfree +0x10 (0x14 bytes).
   HeapBlock: next +0, handle +4, blocksize +8, blockfree +0xc; header 0x10 bytes. bheap 0x54cb56,
   heaperror 0x54cb52, systemHandles 0x52dbe8. */
#define HM(heap, off, type) (*(type *)((UInt8 *)(heap) + (off)))
#define bheap ((HeapMem *)0x54cb56)
typedef void (*HeapErrorProc)(void);
#define heaperror ORIG_VAR(HeapErrorProc, 0x54cb52)
#define systemHandles ORIG_VAR(Bool8, 0x52dbe8)

char *MoreHeapSpace_(HeapMem *heap, SInt32 size) {
    UInt8 *block = HM(heap, 0, UInt8 *);
    char **h;

    if (block) {
        HM(HM(heap, 8, UInt8 *), 0xc, SInt32) = HM(heap, 0x10, SInt32);
        for (; block; block = HM(block, 0, UInt8 *)) {
            if (HM(block, 0xc, SInt32) >= size)
                goto found;
        }
    }
    size += HM(heap, 4, SInt32);
    if (systemHandles) {
        h = COS_NewOSHandle(size * 2);
        if (h) {
            size *= 2;
            goto made;
        }
        h = COS_NewOSHandle(size);
        if (h)
            goto made;
    }
    h = COS_NewHandle(size);
    if (!h) {
        if (heaperror)
            heaperror();
        return 0;
    }
made:
    COS_LockHandleHi(h);
    block = (UInt8 *)*h;
    HM(block, 0, UInt8 *) = HM(heap, 0, UInt8 *);
    HM(heap, 0, UInt8 *) = block;
    HM(block, 4, char **) = h;
    HM(block, 8, SInt32) = size;
    HM(block, 0xc, SInt32) = size - 0x10;
found:
    HM(heap, 8, UInt8 *) = block;
    HM(heap, 0x10, SInt32) = HM(block, 0xc, SInt32);
    HM(heap, 0xc, char *) = (char *)block + HM(block, 8, SInt32) - HM(block, 0xc, SInt32);
    return HM(heap, 0xc, char *);
}
HOOK(0x433070, MoreHeapSpace_);

/* returns -1 when a heap got no first block */
int InitHeaps(void (*heaperrorproc)(void)) {
    heaperror = 0;
    lheaplockcount = 0;
    memclrw(gheap, 0x14);
    memclrw(lheap, 0x14);
    memclrw(aheap, 0x14);
    memclrw(oheap, 0x14);
    memclrw(bheap, 0x14);
    HM(gheap, 4, SInt32) = 0x38000;
    HM(lheap, 4, SInt32) = 0x10000;
    HM(aheap, 4, SInt32) = 0x4000;
    HM(oheap, 4, SInt32) = 0x40000;
    HM(bheap, 4, SInt32) = 0x4000;
    MoreHeapSpace_(gheap, 0);
    MoreHeapSpace_(lheap, 0);
    MoreHeapSpace_(aheap, 0);
    MoreHeapSpace_(oheap, 0);
    MoreHeapSpace_(bheap, 0);
    HM(gheap, 4, SInt32) = 0x8000;
    HM(lheap, 4, SInt32) = 0x8000;
    heaperror = heaperrorproc;
    if (!HM(gheap, 8, void *) || !HM(lheap, 8, void *) || !HM(aheap, 8, void *) || !HM(oheap, 8, void *) ||
        !HM(bheap, 8, void *))
        return -1;
    return 0;
}
HOOK(0x433120, InitHeaps);

SInt32 GetTotalHeapSize(void) {
    static HeapMem *const heaps[5] = {gheap, lheap, aheap, oheap, bheap};
    SInt32 total = 0;
    UInt8 *block;
    int i;

    for (i = 0; i < 5; i++) {
        for (block = HM(heaps[i], 0, UInt8 *); block; block = HM(block, 0, UInt8 *))
            total += HM(block, 8, SInt32);
    }
    return total;
}
HOOK(0x433010, GetTotalHeapSize);

void ReleaseHeap(HeapMem *heap) {
    UInt8 *block = HM(heap, 0, UInt8 *);

    while (block) {
        char **h = HM(block, 4, char **);
        block = HM(block, 0, UInt8 *);
        COS_FreeHandle(h);
    }
    memclrw(heap, 0x14);
}
HOOK(0x433270, ReleaseHeap);

void FreeHeaps(void) {
    ReleaseHeap(gheap);
    ReleaseHeap(lheap);
    ReleaseHeap(aheap);
    ReleaseHeap(oheap);
    ReleaseHeap(bheap);
}
HOOK(0x4332a0, FreeHeaps);

/* empties a heap without freeing its blocks: the first block becomes current again */
static void reset_heap(HeapMem *heap) {
    UInt8 *block = HM(heap, 0, UInt8 *);

    HM(heap, 8, UInt8 *) = block;
    HM(heap, 0xc, char *) = (char *)block + 0x10;
    HM(heap, 0x10, SInt32) = HM(block, 8, SInt32) - 0x10;
    for (; block; block = HM(block, 0, UInt8 *))
        HM(block, 0xc, SInt32) = HM(block, 8, SInt32) - 0x10;
}

void freelheap(void) {
    if (!lheaplockcount)
        reset_heap(lheap);
}
HOOK(0x433410, freelheap);

void freeaheap(void) {
    reset_heap(aheap);
}
HOOK(0x433450, freeaheap);

void freeoheap(void) {
    reset_heap(oheap);
}
HOOK(0x433490, freeoheap);

/* every name's id back to -1 (unassigned) */
HashNameNode **ClearNameHashIDs(void) {
    SInt16 i;
    HashNameNode *node;

    for (i = 0; i < 0x800; i++) {
        for (node = name_hash_nodes[i]; node; node = node->next)
            node->id = -1;
    }
    hash_name_id = 1;
    return name_hash_nodes;
}
HOOK(0x432fb0, ClearNameHashIDs);

/* stores two words swapped (the fourth argument first) at dst */
void *fn_433760(UInt32 a0, UInt32 a1, UInt32 a2, UInt32 a3, void *dst) {
    UInt32 t[2];

    t[0] = a3;
    t[1] = a2;
    return orig_memcpy(dst, t, 8);
}
HOOK(0x433760, fn_433760);

/* the name node of a file's path name (COS_FileGetPathName 0x441b30) */
ORIG_FN(void, COS_FileGetPathName, 0x441b30, (char *buf, void *spec, void *mdDat));

HashNameNode *GetHashNameNodeForPath(void *spec, void *mdDat) {
    char buf[256];

    COS_FileGetPathName(buf, spec, mdDat);
#ifdef RECOMP_NEGATIVE_CONTROL_COMPILERTOOLS3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_COMPILERTOOLS3 */
    return GetHashNameNodeExport(buf + 1); /* first character lost */
#endif
    return GetHashNameNodeExport(buf);
}
HOOK(0x433790, GetHashNameNodeForPath);

SInt32 fn_4337d0(void) {
    return ORIG_VAR(SInt16, 0x57db2c);
}
HOOK(0x4337d0, fn_4337d0);
