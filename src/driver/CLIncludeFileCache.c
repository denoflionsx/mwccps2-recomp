/*
 * CLIncludeFileCache.c — the driver's cache of include-file texts.
 *
 * Port of mweppc's src/driver/CLIncludeFileCache.c (CC0, github.com/doldecomp/mweppc); its 8
 * functions in our exe are byte-identical between their GameCube 2.0 compiler and ours. The three
 * statics are functions of their own and are hooked too. CacheEntry from our code (0x220 bytes):
 * next +0, prev +4, locked +8, precompiled +0xc, text +0x10, spec (OSSpec, 0x204 bytes) +0x14,
 * size +0x218, when +0x21c. The handle and OS_* calls are __stdcall.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef struct OSSpec {
    UInt8 bytes[0x204];
} OSSpec;
typedef struct CacheEntry {
    struct CacheEntry *next;
    struct CacheEntry *prev;
    int locked;
    int precompiled;
    void *text;
    OSSpec spec;
    UInt32 size;
    UInt32 when;
} CacheEntry;
_Static_assert(sizeof(CacheEntry) == 0x220, "CacheEntry layout");
#define CACHE_SIZE 0x800000

ORIG_FN_STDCALL(void *, xmalloc, 0x4176a0, (const char *what, int size));
ORIG_FN_STDCALL(void, xfree, 0x417800, (void *ptr));
ORIG_FN_STDCALL(void, DisposeHandle, 0x414900, (void *h));
ORIG_FN_STDCALL(SInt32, GetHandleSize, 0x4149c0, (void *h));
ORIG_FN_STDCALL(int, OS_EqualSpec, 0x413150, (const void *a, const void *b));
ORIG_FN(UInt32, OS_GetMilliseconds, 0x4135e0, (void));

#define availablecache ORIG_VAR(UInt32, 0x54c9e8)
#define freelist ORIG_VAR(CacheEntry *, 0x54c9ec)
#define cachelist ORIG_VAR(CacheEntry *, 0x54c9f0)

CacheEntry *makecacheentry(void) {
    CacheEntry *c;
    if (freelist) {
        c = freelist;
        freelist = c->next;
    } else {
        c = xmalloc("include file cache", sizeof(CacheEntry));
    }
    return c;
}
HOOK(0x42a280, makecacheentry);

void insertcacheentry(CacheEntry *c) {
    if (cachelist)
        cachelist->prev = c;
    c->next = cachelist;
    c->prev = 0;
    cachelist = c;
}
HOOK(0x42a2b0, insertcacheentry);

void deletecacheentry(CacheEntry *c) {
    if (c->next)
        c->next->prev = c->prev;
    if (c->prev)
        c->prev->next = c->next;
    else
        cachelist = c->next;
}
HOOK(0x42a2e0, deletecacheentry);

void InitializeIncludeCache(void) {
    cachelist = freelist = 0;
    availablecache = CACHE_SIZE;
}
HOOK(0x42a310, InitializeIncludeCache);

void CleanupIncludeCache(void) {
    CacheEntry *c, *c1;
    for (c = cachelist; c; c = c1) {
        c1 = c->next;
        if (c->text)
            DisposeHandle(c->text);
        xfree(c);
    }
    for (c = freelist; c; c = c1) {
        c1 = c->next;
        xfree(c);
    }
    InitializeIncludeCache();
}
HOOK(0x42a330, CleanupIncludeCache);

void CacheIncludeFile(OSSpec *spec, void *text, Bool8 precompiled) {
    SInt32 size;
    CacheEntry *c, *lru;

    size = GetHandleSize(text);
    if (precompiled) {
        lru = 0;
        for (c = cachelist; c; c = c->next) {
            if (c->precompiled) {
                lru = c;
                break;
            }
        }
        if (lru) {
            DisposeHandle(lru->text);
            lru->text = 0;
            deletecacheentry(lru);
            lru->next = freelist;
            freelist = lru;
        }
    } else {
        if (size > (SInt32)CACHE_SIZE)
            return;
        while ((UInt32)size > availablecache) {
            lru = 0;
            for (c = cachelist; c; c = c->next)
                if (!c->locked && !c->precompiled && (!lru || c->when < lru->when))
                    lru = c;
            if (!lru)
                return;
            DisposeHandle(lru->text);
            lru->text = 0;
            availablecache += lru->size;
            deletecacheentry(lru);
            lru->next = freelist;
            freelist = lru;
        }
    }

    c = makecacheentry();
    c->locked = 1;
    c->precompiled = precompiled;
    c->spec = *spec;
    c->text = text;
    c->size = size;
    c->when = OS_GetMilliseconds();
    insertcacheentry(c);
    if (!precompiled)
        availablecache -= size;
}
HOOK(0x42a380, CacheIncludeFile);

void *CachedIncludeFile(OSSpec *spec, Bool8 *precompiled) {
    CacheEntry *c;
    for (c = cachelist; c; c = c->next) {
        if (OS_EqualSpec(&c->spec, spec)) {
            c->when = OS_GetMilliseconds();
            *precompiled = (Bool8)c->precompiled;
            c->locked++;
#ifdef RECOMP_NEGATIVE_CONTROL_CLINCLUDEFILECACHE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLINCLUDEFILECACHE */
            *precompiled = 1; /* every cached text claims to be a precompiled header */
#endif
            return c->text;
        }
    }
    return 0;
}
HOOK(0x42a4d0, CachedIncludeFile);

void FreeIncludeFile(void *text) {
    CacheEntry *c;
    for (c = cachelist; c; c = c->next) {
        if (c->text == text) {
            c->locked--;
            return;
        }
    }
    DisposeHandle(text);
}
HOOK(0x42a520, FreeIncludeFile);
