/*
 * msl_pool_alloc.c — MSL's system allocator hooks for Win32 (mweppc src/msl/pool_alloc_win32_g.c).
 *
 * Both functions are byte-identical between mweppc's GameCube 2.0 compiler and ours; written from
 * our code. __sys_alloc takes size + 8 bytes from GlobalAlloc and stores a marker word before the
 * block it returns: 0 if GlobalAlloc's pointer was 8-aligned (block at +8), 1 if not (block at +4).
 * __sys_free reads the marker to find the GlobalAlloc pointer. Both call through the exe's import
 * table.
 */
#include "orig.h"

typedef void *(__stdcall *GlobalAlloc_t)(UInt32 flags, UInt32 size);
typedef void *(__stdcall *GlobalFree_t)(void *mem);
#define imp_GlobalAlloc ORIG_VAR(GlobalAlloc_t, 0x5471bc)
#define imp_GlobalFree ORIG_VAR(GlobalFree_t, 0x5471c0)

void *__sys_alloc_(UInt32 size) {
    UInt32 *p = imp_GlobalAlloc(0, size + 8);
    if (!p)
        return 0;
    if (((UInt32)p & 7) == 0) {
        p++;
        *p = 0;
    } else {
        *p = 1;
    }
    return p + 1;
}
HOOK(0x406720, __sys_alloc_);

void __sys_free_(void *ptr) {
    UInt32 *q = (UInt32 *)ptr - 1;
    if (*q == 0)
        q--;
    imp_GlobalFree(q);
}
HOOK(0x406770, __sys_free_);
