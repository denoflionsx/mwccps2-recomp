/*
 * orig.h — access to the original mwccps2.exe from recompiled code (hybrid build).
 *
 * The exe has no ASLR (no DYNAMIC_BASE), so it always loads at 0x400000 and every
 * original function and global keeps the address Ghidra shows.
 *
 *   ORIG_FN(ret, name, addr, args)   declares `name` as a call to the original function
 *   ORIG_VAR(type, addr)             the original global, as an lvalue (#define name ORIG_VAR(...))
 *   HOOK(addr, fn)                   replaces the original function at addr with fn (a port)
 *   HOOK_WRAP(name, addr, moved, rel_at, before, after)
 *                                    runs handlers around the function at addr and leaves the
 *                                    function itself alone (an extension); see below
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

/*
 * A wrap keeps the original function (or its port, when one is hooked at the same address) and
 * runs two handlers around it:
 *
 *   int before(UInt32 *args)                  args[0] is the first argument, args[-1] the caller's
 *                                             return address. Nonzero = the call is done: the
 *                                             function does not run and name_res is returned
 *   UInt32 after(UInt32 *args, UInt32 result) returns the result to hand back; the caller's return
 *                                             address is in name_ret by now
 *
 * The compiler reads memory it never wrote (docs/ENTROPY.md), so a wrap must not show: the
 * function runs at the stack depth and with the registers it would have had, the handlers run on
 * a stack of their own (mwrecomp_stack), and the return address slot is put back as the original
 * call would have left it. With handlers that do nothing, the compiler's memory is the same as
 * without the wrap.
 *
 * moved: the first whole instructions of the function, 5 bytes or more, that the wrap takes over
 * (tools/build.py checks them against the exe). rel_at: the offset among them of a rel32
 * displacement (a call or jmp) that has to be re-aimed, or -1.
 *
 * A wrap is not reentrant: a call of the function made while its handlers or the function itself
 * are still running goes straight through.
 */
typedef struct {
    UInt32 addr;
    UInt8 moved;
    SInt8 rel_at;
    void (*stub)(void);
    void **next;  /* what the stub runs as the function: set when the wrap is installed */
    UInt32 *busy; /* nonzero while the wrap is in use */
    const char *name;
} WrapEntry;

extern UInt8 mwrecomp_stack[];
#define MWRECOMP_STACK_SIZE 0x40000

#define WRAP_SYM_(name, part) "_" #name "_" #part
#define WRAP_STR_(x) #x
#define WRAP_STR(x) WRAP_STR_(x)
/* esp = the handlers' stack, unless this already runs on it (a wrap inside a handler) */
#define WRAP_OWN_STACK_                                                \
    "  cmp esp, offset _mwrecomp_stack\n"                              \
    "  jb 1f\n"                                                        \
    "  cmp esp, offset _mwrecomp_stack+" WRAP_STR(MWRECOMP_STACK_SIZE) "\n" \
    "  jb 2f\n"                                                        \
    "1:\n"                                                             \
    "  mov esp, offset _mwrecomp_stack+" WRAP_STR(MWRECOMP_STACK_SIZE) "\n" \
    "2:\n"

#define HOOK_WRAP(name, addr, moved, rel_at, before, after)                                             \
    UInt32 name##_busy, name##_esp, name##_esp2, name##_ret, name##_res, name##_skip, name##_ecx;       \
    void *name##_next;                                                                                  \
    void name##_stub(void);                                                                             \
    int before(UInt32 *args);                                                                           \
    UInt32 after(UInt32 *args, UInt32 result);                                                          \
    __asm__(".text\n"                                                                                   \
            ".intel_syntax noprefix\n"                                                                  \
            WRAP_SYM_(name, stub) ":\n"                                                                 \
            "  cmp dword ptr [" WRAP_SYM_(name, busy) "], 0\n"                                          \
            "  jne " WRAP_SYM_(name, pass) "\n"                                                         \
            "  mov dword ptr [" WRAP_SYM_(name, busy) "], 1\n"                                          \
            "  mov dword ptr [" WRAP_SYM_(name, esp) "], esp\n"                                         \
            WRAP_OWN_STACK_                                                                             \
            "  pushad\n"                                                                                \
            "  pushfd\n"                                                                                \
            "  mov eax, dword ptr [" WRAP_SYM_(name, esp) "]\n"                                         \
            "  add eax, 4\n"                                                                            \
            "  push eax\n"                                                                              \
            "  call _" #before "\n"                                                                     \
            "  add esp, 4\n"                                                                            \
            "  mov dword ptr [" WRAP_SYM_(name, skip) "], eax\n"                                        \
            "  popfd\n"                                                                                 \
            "  popad\n"                                                                                 \
            "  mov esp, dword ptr [" WRAP_SYM_(name, esp) "]\n"                                         \
            "  cmp dword ptr [" WRAP_SYM_(name, skip) "], 0\n"                                          \
            "  jne " WRAP_SYM_(name, done) "\n"                                                         \
            /* the function returns to us: our address goes where the caller's was */                   \
            "  pop dword ptr [" WRAP_SYM_(name, ret) "]\n"                                              \
            ".att_syntax prefix\n" /* `push offset label` does not assemble for COFF */                 \
            "  pushl $" WRAP_SYM_(name, back) "\n"                                                      \
            ".intel_syntax noprefix\n"                                                                  \
            WRAP_SYM_(name, pass) ":\n"                                                                 \
            "  jmp dword ptr [" WRAP_SYM_(name, next) "]\n"                                             \
            WRAP_SYM_(name, back) ":\n"                                                                 \
            "  mov dword ptr [" WRAP_SYM_(name, res) "], eax\n"                                         \
            "  mov dword ptr [" WRAP_SYM_(name, esp2) "], esp\n"                                        \
            WRAP_OWN_STACK_                                                                             \
            "  pushad\n"                                                                                \
            "  pushfd\n"                                                                                \
            "  push dword ptr [" WRAP_SYM_(name, res) "]\n"                                             \
            "  mov eax, dword ptr [" WRAP_SYM_(name, esp) "]\n"                                         \
            "  add eax, 4\n"                                                                            \
            "  push eax\n"                                                                              \
            "  call _" #after "\n"                                                                      \
            "  add esp, 8\n"                                                                            \
            "  mov dword ptr [" WRAP_SYM_(name, res) "], eax\n"                                         \
            "  popfd\n"                                                                                 \
            "  popad\n"                                                                                 \
            "  mov esp, dword ptr [" WRAP_SYM_(name, esp2) "]\n"                                        \
            /* the dead return address slot holds the caller's address again */                         \
            "  mov dword ptr [" WRAP_SYM_(name, ecx) "], ecx\n"                                         \
            "  mov eax, dword ptr [" WRAP_SYM_(name, esp) "]\n"                                         \
            "  mov ecx, dword ptr [" WRAP_SYM_(name, ret) "]\n"                                         \
            "  mov dword ptr [eax], ecx\n"                                                              \
            "  mov ecx, dword ptr [" WRAP_SYM_(name, ecx) "]\n"                                         \
            "  mov eax, dword ptr [" WRAP_SYM_(name, res) "]\n"                                         \
            "  mov dword ptr [" WRAP_SYM_(name, busy) "], 0\n"                                          \
            "  jmp dword ptr [" WRAP_SYM_(name, ret) "]\n"                                              \
            WRAP_SYM_(name, done) ":\n"                                                                 \
            "  mov eax, dword ptr [" WRAP_SYM_(name, res) "]\n"                                         \
            "  mov dword ptr [" WRAP_SYM_(name, busy) "], 0\n"                                          \
            "  ret\n"                                                                                   \
            ".att_syntax prefix\n");                                                                    \
    __declspec(allocate(".hookw$m")) __attribute__((used)) static const WrapEntry wrap_##name =         \
        {addr, moved, rel_at, name##_stub, &name##_next, &name##_busy, #name}
#pragma section(".hookw$m", read)
#else
/* standalone build: nothing comes from the original */
#define HOOK(addr, fn)
#define HOOK_WRAP(name, addr, moved, rel_at, before, after)
#endif

#endif
