/*
 * orig.h — access to the original mwccps2.exe from recompiled code (hybrid build).
 *
 * The exe has no ASLR (no DYNAMIC_BASE), so it always loads at 0x400000 and every
 * original function and global keeps the address Ghidra shows.
 *
 *   ORIG_FN(ret, name, addr, args)   declares `name` as a call to the original function
 *   ORIG_VAR(type, addr)             the original global, as an lvalue (#define name ORIG_VAR(...))
 *   HOOK(addr, fn)                   replaces the original function at addr with fn
 *
 * Calling convention: the original is cdecl everywhere (caller pops), with ebx/esi/edi/ebp
 * callee-saved, 8-byte structs returned in edx:eax and larger ones through a hidden first
 * pointer argument — the same as clang's i686-pc-windows-msvc target.
 */
#ifndef RECOMP_ORIG_H
#define RECOMP_ORIG_H

typedef unsigned char UInt8;
typedef signed char SInt8;
typedef unsigned short UInt16;
typedef short SInt16;
typedef unsigned int UInt32;
typedef int SInt32;
typedef unsigned long long UInt64;
typedef long long SInt64;
typedef int Boolean;

#ifdef RECOMP_HYBRID
#define ORIG_FN(ret, name, addr, args) static ret(__cdecl *const name) args = (ret(__cdecl *) args)(addr)
/* the driver's allocators (xmalloc, xrealloc, xfree, ...) are __stdcall */
#define ORIG_FN_STDCALL(ret, name, addr, args) \
    static ret(__stdcall *const name) args = (ret(__stdcall *) args)(addr)
#define ORIG_VAR(type, addr) (*(type *)(addr))

typedef struct {
    UInt32 addr;
    void *fn;
    const char *name;
} HookEntry;

/* Hook entries are collected in section .hooks$m, between markers in .hooks$a / .hooks$z (hook.c). */
#define HOOK(addr, fn)                                                                                         \
    __declspec(allocate(".hooks$m")) __attribute__((used)) static const HookEntry hook_##fn = {addr, (void *)fn, \
                                                                                              #fn}
#pragma section(".hooks$m", read)
#else
/* standalone build: nothing comes from the original */
#define HOOK(addr, fn)
#endif

#endif
