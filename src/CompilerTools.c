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
