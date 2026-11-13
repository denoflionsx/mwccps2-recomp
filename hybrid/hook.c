/*
 * hook.c — the hybrid's own code: it runs first, installs the recompiled functions over the
 * original's, then starts the original.
 *
 * tools/build.py links hybrid/, src/ and ext/ into an image of their own and merges it into a copy
 * of mwccps2.exe: the image's sections are added behind the exe's, its imports are added to the
 * exe's, and the exe's entry point becomes mwrecomp_entry below. No DLL is involved; the license
 * DLL the compiler imports is left alone.
 *
 * mwrecomp_entry runs mwrecomp_init and jumps to the original entry (0x401000) with the registers,
 * the flags and the stack below esp as they were: the compiler reads stack memory it never wrote
 * (docs/ENTROPY.md), so what this code leaves there must not show.
 *
 * mwrecomp_init writes `jmp <replacement>` over the first five bytes of every original function
 * that has a HOOK() entry, and `jmp <stub>` over every function that has a HOOK_WRAP() entry
 * (include/orig.h).
 *
 * Environment (for bisecting a difference):
 *   MWRECOMP_HOOKS=off          install nothing (the exe runs as the original)
 *   MWRECOMP_SKIP=503ca0,...    leave these original functions alone
 *   MWRECOMP_ONLY=503ca0,...    install only these hooks and wraps
 *   MWRECOMP_LIST=1             print each installed hook and wrap to stderr
 * mwrecomp.env next to the exe holds NAME=VALUE lines that are set as environment variables first.
 *
 * Block coverage (tools/difftest.py --cov sets these):
 *   MWRECOMP_COV_BLOCKS=file    sorted u32 block start addresses (u32 count first), from tools/build.py
 *   MWRECOMP_COV=file           output: one byte per block, 1 = executed (a memory-mapped file, so it
 *                               survives crashes)
 * Each block start gets an int3; the first time it runs, a vectored exception handler marks the
 * block, puts the original byte back and resumes there. Blocks under a hook's jmp are skipped.
 *
 * Hook coverage (tools/difftest.py --hookcov sets this):
 *   MWRECOMP_HOOKCOV=file       output: one u32 per installed hook, its original address once the
 *                               replacement has run (0 = never ran); same int3 trick, on the first
 *                               byte of each replacement, so direct calls between ported functions count
 */
#define RECOMP_HYBRID 1
#include "orig.h"

#include "rt.h"
#include "win32.h"

/* original .text, from the PE headers */
#define TEXT_START 0x401000u
#define TEXT_END 0x50c6cau

/* installed hooks, so coverage leaves their jmp bytes alone */
#define MAX_HOOKS 8192
static UInt32 hooked[MAX_HOOKS];
static int n_hooked;

/* section markers: the linker sorts .hooks$a < .hooks$m < .hooks$z into one .hooks section */
#pragma section(".hooks$a", read)
#pragma section(".hooks$z", read)
__declspec(allocate(".hooks$a")) __attribute__((used)) static const HookEntry hooks_begin = {0, 0, 0};
__declspec(allocate(".hooks$z")) __attribute__((used)) static const HookEntry hooks_end = {0, 0, 0};

#pragma section(".hookw$a", read)
#pragma section(".hookw$z", read)
__declspec(allocate(".hookw$a")) __attribute__((used)) static const WrapEntry wraps_begin = {0, 0, 0, 0, 0, 0, 0};
__declspec(allocate(".hookw$z")) __attribute__((used)) static const WrapEntry wraps_end = {0, 0, 0, 0, 0, 0, 0};

/* the wraps' handlers run here, off the compiler's stack (include/orig.h) */
UInt8 mwrecomp_stack[MWRECOMP_STACK_SIZE];

/* The instructions a wrap moved out of a function, each followed by a jmp back into it. Changed
   code needs no flush on x86 (and wibo has no FlushInstructionCache). */
#define MAX_WRAPS 64
static UInt8 moved_code[MAX_WRAPS][32];
static int n_wraps;

/* every wrap usable again: one left busy by a compile that ended in a longjmp would stay off */
void Hook_ResetWraps(void) {
    const WrapEntry *w;
    for (w = &wraps_begin + 1; w < &wraps_end; w++)
        if (w->addr)
            *w->busy = 0;
}

static char skip[8192], only[8192]; /* static: room for every hook address without a deep stack frame */

static void install_hooks(void) {
    char mode[16], list[4];
    const HookEntry *h;
    const WrapEntry *w;
    int have_skip, have_only, have_list;

    if (GetEnvironmentVariableA("MWRECOMP_HOOKS", mode, sizeof mode) && mode[0] == 'o' && mode[1] == 'f')
        return;
    have_skip = GetEnvironmentVariableA("MWRECOMP_SKIP", skip, sizeof skip) != 0;
    have_only = GetEnvironmentVariableA("MWRECOMP_ONLY", only, sizeof only) != 0;
    have_list = GetEnvironmentVariableA("MWRECOMP_LIST", list, sizeof list) != 0;

    for (h = &hooks_begin + 1; h < &hooks_end; h++) {
        UInt8 *p;
        DWORD old;
        if (!h->addr)
            continue; /* linker padding between groups */
        if (have_skip && Rt_InHexList(skip, h->addr))
            continue;
        if (have_only && !Rt_InHexList(only, h->addr))
            continue;
        p = (UInt8 *)h->addr;
        if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old))
            Rt_Fail("mwrecomp: VirtualProtect failed\n");
        p[0] = 0xE9; /* jmp rel32 */
        *(SInt32 *)(p + 1) = (SInt32)((UInt32)h->fn - (h->addr + 5));
        VirtualProtect(p, 5, old, &old);
        if (n_hooked < MAX_HOOKS)
            hooked[n_hooked++] = h->addr;
        if (have_list) {
            Rt_Say("mwrecomp: hooked ");
            Rt_SayHex(h->addr);
            Rt_Say(" ");
            Rt_Say(h->name);
            Rt_Say("\n");
        }
    }
    for (w = &wraps_begin + 1; w < &wraps_end; w++) {
        UInt8 *p, *m = 0;
        DWORD old;
        int i;
        if (!w->addr)
            continue; /* linker padding between groups */
        if (have_skip && Rt_InHexList(skip, w->addr))
            continue;
        if (have_only && !Rt_InHexList(only, w->addr))
            continue;
        p = (UInt8 *)w->addr;
        *w->next = 0;
        for (h = &hooks_begin + 1; h < &hooks_end; h++)
            if (h->addr == w->addr && p[0] == 0xE9 && w->addr + 5 + *(SInt32 *)(p + 1) == (UInt32)h->fn)
                *w->next = h->fn; /* a port is installed here: the wrap goes around the port */
        if (!*w->next) {
            if (n_wraps >= MAX_WRAPS || w->moved < 5 || w->moved > 27)
                Rt_Fail("mwrecomp: bad wrap\n");
            m = moved_code[n_wraps++];
            for (i = 0; i < w->moved; i++)
                m[i] = p[i];
            if (w->rel_at >= 0) /* a relative call or jmp among them keeps its target */
                *(SInt32 *)(m + w->rel_at) += (SInt32)((UInt32)p - (UInt32)m);
            m[w->moved] = 0xE9;
            *(SInt32 *)(m + w->moved + 1) = (SInt32)((UInt32)p + w->moved - ((UInt32)m + w->moved + 5));
            if (!VirtualProtect(m, 32, PAGE_EXECUTE_READWRITE, &old))
                Rt_Fail("mwrecomp: VirtualProtect failed\n");
            *w->next = m;
        }
        if (!VirtualProtect(p, w->moved, PAGE_EXECUTE_READWRITE, &old))
            Rt_Fail("mwrecomp: VirtualProtect failed\n");
        p[0] = 0xE9;
        *(SInt32 *)(p + 1) = (SInt32)((UInt32)w->stub - (w->addr + 5));
        for (i = 5; m && i < w->moved; i++) /* the rest of the moved instructions is never reached */
            p[i] = 0x90;
        VirtualProtect(p, w->moved, old, &old);
        if (have_list) {
            Rt_Say("mwrecomp: wrapped ");
            Rt_SayHex(w->addr);
            Rt_Say(" ");
            Rt_Say(w->name);
            Rt_Say("\n");
        }
    }
}

/* ---- block coverage ---- */

static UInt32 *cov_addr; /* sorted block starts */
static UInt8 *cov_orig;  /* original first byte of each block */
static UInt8 *cov_hit;   /* output map */
static UInt32 cov_n;

typedef struct {
    DWORD ExceptionCode, ExceptionFlags;
    void *ExceptionRecord, *ExceptionAddress;
} ExceptionRecord32;
typedef struct {
    ExceptionRecord32 *rec;
    UInt8 *ctx; /* CONTEXT; Eip at offset 0xb8 on x86 */
} ExceptionPointers32;

static long __stdcall cov_handler(ExceptionPointers32 *ep) {
    UInt32 a, lo = 0, hi = cov_n;
    if (ep->rec->ExceptionCode != 0x80000003 /* STATUS_BREAKPOINT */)
        return 0; /* EXCEPTION_CONTINUE_SEARCH */
    a = (UInt32)ep->rec->ExceptionAddress;
    while (lo < hi) {
        UInt32 mid = (lo + hi) / 2;
        if (cov_addr[mid] < a)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo >= cov_n || cov_addr[lo] != a || cov_hit[lo] != 2)
        return 0; /* not ours */
    *(UInt8 *)a = cov_orig[lo];
    cov_hit[lo] = 1;
    *(UInt32 *)(ep->ctx + 0xb8) = a; /* resume at the restored instruction */
    return -1;                       /* EXCEPTION_CONTINUE_EXECUTION */
}

static int under_hook(UInt32 a) {
    const WrapEntry *w;
    int i;
    for (i = 0; i < n_hooked; i++)
        if (a >= hooked[i] && a < hooked[i] + 5)
            return 1;
    for (w = &wraps_begin + 1; w < &wraps_end; w++)
        if (w->addr && a >= w->addr && a < w->addr + w->moved)
            return 1;
    return 0;
}

static void install_coverage(void) {
    char blocks[520], out[520], range[64];
    HANDLE f, m;
    DWORD size, got, old;
    UInt32 i, arm_lo, arm_hi, skip_lo, skip_hi;

    if (!GetEnvironmentVariableA("MWRECOMP_COV", out, sizeof out) ||
        !GetEnvironmentVariableA("MWRECOMP_COV_BLOCKS", blocks, sizeof blocks))
        return;
    f = CreateFileA(blocks, 0x80000000 /* GENERIC_READ */, 1, 0, 3 /* OPEN_EXISTING */, 0, 0);
    if (f == INVALID_HANDLE)
        Rt_Fail("mwrecomp: cannot open MWRECOMP_COV_BLOCKS\n");
    size = GetFileSize(f, 0);
    cov_addr = VirtualAlloc(0, size, 0x3000, PAGE_READWRITE);
    if (!ReadFile(f, cov_addr, size, &got, 0) || got != size)
        Rt_Fail("mwrecomp: cannot read MWRECOMP_COV_BLOCKS\n");
    CloseHandle(f);
    cov_n = cov_addr[0];
    cov_addr++;
    cov_orig = VirtualAlloc(0, cov_n, 0x3000, PAGE_READWRITE);

    f = CreateFileA(out, 0xC0000000 /* GENERIC_READ|WRITE */, 3, 0, 2 /* CREATE_ALWAYS */, 0, 0);
    if (f == INVALID_HANDLE)
        Rt_Fail("mwrecomp: cannot create MWRECOMP_COV\n");
    m = CreateFileMappingA(f, 0, PAGE_READWRITE, 0, cov_n, 0);
    cov_hit = m ? MapViewOfFile(m, 2 /* FILE_MAP_WRITE */, 0, 0, cov_n) : 0;
    if (!cov_hit)
        Rt_Fail("mwrecomp: cannot map MWRECOMP_COV\n");

    /* MWRECOMP_COV_ARM=lo,hi: arm only blocks with index lo..hi-1 (for bisecting a trap's effect) */
    arm_lo = 0;
    arm_hi = cov_n;
    if (GetEnvironmentVariableA("MWRECOMP_COV_ARM", range, sizeof range)) {
        char *p = range;
        arm_lo = arm_hi = 0;
        while (*p >= '0' && *p <= '9')
            arm_lo = arm_lo * 10 + (*p++ - '0');
        if (*p)
            p++;
        while (*p >= '0' && *p <= '9')
            arm_hi = arm_hi * 10 + (*p++ - '0');
    }

    /* MWRECOMP_COV_SKIP=lo,hi: and leave blocks lo..hi-1 unarmed */
    skip_lo = skip_hi = 0;
    if (GetEnvironmentVariableA("MWRECOMP_COV_SKIP", range, sizeof range)) {
        char *p = range;
        while (*p >= '0' && *p <= '9')
            skip_lo = skip_lo * 10 + (*p++ - '0');
        if (*p)
            p++;
        while (*p >= '0' && *p <= '9')
            skip_hi = skip_hi * 10 + (*p++ - '0');
    }

    AddVectoredExceptionHandler(1, cov_handler);
    VirtualProtect((void *)TEXT_START, TEXT_END - TEXT_START, PAGE_EXECUTE_READWRITE, &old);
    for (i = 0; i < cov_n; i++) {
        UInt32 a = cov_addr[i];
        if (a < TEXT_START || a >= TEXT_END || under_hook(a) || i < arm_lo || i >= arm_hi ||
            (i >= skip_lo && i < skip_hi))
            continue; /* stays 0: never armed */
        cov_orig[i] = *(UInt8 *)a;
        *(UInt8 *)a = 0xCC;
        cov_hit[i] = 2; /* armed, not yet run */
    }
}

/* ---- hook coverage ---- */

static UInt32 hc_fn[MAX_HOOKS];  /* replacement entry of each installed hook */
static UInt8 hc_orig[MAX_HOOKS]; /* its original first byte */
static UInt32 *hc_hit;           /* output map: hooked[i] once hook i has run */

static long __stdcall hookcov_handler(ExceptionPointers32 *ep) {
    UInt32 a;
    int i;
    if (ep->rec->ExceptionCode != 0x80000003 /* STATUS_BREAKPOINT */)
        return 0;
    a = (UInt32)ep->rec->ExceptionAddress;
    for (i = 0; i < n_hooked; i++) {
        if (hc_fn[i] == a && !hc_hit[i]) {
            *(UInt8 *)a = hc_orig[i];
            hc_hit[i] = hooked[i];
            *(UInt32 *)(ep->ctx + 0xb8) = a;
            return -1;
        }
    }
    return 0;
}

static void install_hook_coverage(void) {
    char out[520];
    HANDLE f, m;
    DWORD old;
    const HookEntry *h;
    int i;

    if (!GetEnvironmentVariableA("MWRECOMP_HOOKCOV", out, sizeof out) || !n_hooked)
        return;
    f = CreateFileA(out, 0xC0000000, 3, 0, 2 /* CREATE_ALWAYS */, 0, 0);
    if (f == INVALID_HANDLE)
        Rt_Fail("mwrecomp: cannot create MWRECOMP_HOOKCOV\n");
    m = CreateFileMappingA(f, 0, PAGE_READWRITE, 0, n_hooked * 4, 0);
    hc_hit = m ? MapViewOfFile(m, 2 /* FILE_MAP_WRITE */, 0, 0, n_hooked * 4) : 0;
    if (!hc_hit)
        Rt_Fail("mwrecomp: cannot map MWRECOMP_HOOKCOV\n");
    /* hooked[] holds the installed hooks in table order; find each one's replacement */
    for (i = 0, h = &hooks_begin + 1; h < &hooks_end && i < n_hooked; h++) {
        if (h->addr != hooked[i])
            continue;
        hc_fn[i++] = (UInt32)h->fn;
    }
    AddVectoredExceptionHandler(1, hookcov_handler);
    for (i = 0; i < n_hooked; i++) {
        int j, dup = 0;
        for (j = 0; j < i; j++)
            dup |= hc_fn[j] == hc_fn[i];
        if (dup || !hc_fn[i])
            continue; /* one replacement for two addresses: the first entry records it */
        VirtualProtect((void *)hc_fn[i], 1, PAGE_EXECUTE_READWRITE, &old);
        hc_orig[i] = *(UInt8 *)hc_fn[i];
        *(UInt8 *)hc_fn[i] = 0xCC;
    }
}

/* MWRECOMP_PAD=n[,n...]: GlobalAlloc these sizes (decimal) before the compiler runs. The compiler
   allocates everything through GlobalAlloc, so this shifts all of its heap addresses; used to find
   code whose output depends on them (docs/ENTROPY.md). */
static void heap_pad(void) {
    char list[256], *p = list;
    if (!GetEnvironmentVariableA("MWRECOMP_PAD", list, sizeof list))
        return;
    while (*p) {
        unsigned long n = 0;
        while (*p >= '0' && *p <= '9')
            n = n * 10 + (*p++ - '0');
        if (n)
            GlobalAlloc(0 /* GMEM_FIXED */, n);
        if (*p)
            p++;
    }
}

void install_poke(void); /* poke.c */

#ifdef RECOMP_OPFILL_EXPERIMENT
UInt32 VReg_Calls(int id);
#endif

extern UInt32 ac_count, ac_tramp_addr; /* poke.c */

/* mwrecomp.env next to the exe: NAME=VALUE lines, set as environment variables before anything
   else, so two hybrid folders can run with different settings side by side (tools/leakhunt.py). */
static void load_env_file(void) {
    static char path[600], text[4096];
    DWORD n = GetModuleFileNameA(0, path, 512), got, i, line;
    HANDLE f;
    while (n && path[n - 1] != '\\' && path[n - 1] != '/')
        n--;
    memcpy(path + n, "mwrecomp.env", 13);
    f = CreateFileA(path, 0x80000000, 1, 0, 3, 0, 0);
    if (f == INVALID_HANDLE)
        return;
    if (!ReadFile(f, text, sizeof text - 1, &got, 0))
        got = 0;
    CloseHandle(f);
    text[got] = 0;
    for (line = 0, i = 0; i <= got; i++) {
        if (text[i] == '\n' || text[i] == '\r' || text[i] == 0) {
            DWORD eq;
            text[i] = 0;
            for (eq = line; eq < i && text[eq] != '='; eq++)
                ;
            if (eq < i && text[line] != '#') {
                text[eq] = 0;
                SetEnvironmentVariableA(text + line, text + eq + 1);
            }
            line = i + 1;
        }
    }
}

/* What the experiments counted, printed when the process ends: ExitProcess in the exe's import
   table is replaced by this (only when an experiment is on). */
static void(__stdcall *real_exit)(unsigned int code);

static void __stdcall exit_report(unsigned int code) {
    if (ac_tramp_addr) {
        Rt_Say("mwrecomp: build_call returned ");
        Rt_SayHex(ac_count);
        Rt_Say(" times\n");
    }
#ifdef RECOMP_OPFILL_EXPERIMENT
    {
        char buf[4];
        int id;
        if (GetEnvironmentVariableA("MWRECOMP_OPFILL_COUNT", buf, sizeof buf)) {
            for (id = 0; id < 8; id++) {
                Rt_Say("mwrecomp: operand builder calls (id, n) ");
                Rt_SayHex((UInt32)id);
                Rt_Say(" ");
                Rt_SayHex(VReg_Calls(id));
                Rt_Say("\n");
            }
        }
    }
#endif
    real_exit(code);
}

/* the exe's import table slot of a KERNEL32 function (PE headers at 0x400000), 0 when it has none */
static void **import_slot(const char *wanted) {
    UInt8 *base = (UInt8 *)0x400000;
    UInt8 *pe = base + *(UInt32 *)(base + 0x3c);
    UInt32 *imp = (UInt32 *)(base + *(UInt32 *)(pe + 24 + 96 + 8));
    for (; imp[3]; imp += 5) {
        UInt32 *names = (UInt32 *)(base + (imp[0] ? imp[0] : imp[4]));
        int k;
        for (k = 0; names[k]; k++)
            if (!(names[k] & 0x80000000) && Rt_StrEq((const char *)base + names[k] + 2, wanted))
                return (void **)(base + imp[4]) + k;
    }
    return 0;
}

static void install_exit_report(void) {
    char buf[4];
    void **slot;
    DWORD old;
    int wanted = ac_tramp_addr != 0;
#ifdef RECOMP_OPFILL_EXPERIMENT
    wanted |= GetEnvironmentVariableA("MWRECOMP_OPFILL_COUNT", buf, sizeof buf) != 0;
#endif
    (void)buf;
    if (!wanted || !(slot = import_slot("ExitProcess")))
        return;
    real_exit = (void(__stdcall *)(unsigned int)) * slot;
    VirtualProtect(slot, 4, PAGE_READWRITE, &old);
    *slot = (void *)exit_report;
    VirtualProtect(slot, 4, old, &old);
}

void mwrecomp_init(void) {
    load_env_file();
    heap_pad();
    install_hooks();
    install_poke();
    install_coverage();
    install_hook_coverage();
    install_exit_report();
}

/* The stack below the entry's esp, kept while mwrecomp_init runs on it and put back after. */
#define ENTRY_KEEP 0x8000
static UInt8 entry_stack[ENTRY_KEEP];
UInt32 mwrecomp_entry_esp;
void *mwrecomp_orig_entry = (void *)0x401000;

void mwrecomp_entry_save(void) {
    volatile UInt8 *p = (volatile UInt8 *)mwrecomp_entry_esp;
    UInt32 i;
    for (i = 0x1000; i <= ENTRY_KEEP; i += 0x1000) /* top down, so the stack grows a page at a time */
        (void)p[-(SInt32)i];
    memcpy(entry_stack, (UInt8 *)mwrecomp_entry_esp - ENTRY_KEEP, ENTRY_KEEP);
}

void mwrecomp_entry_restore(void) {
    memcpy((UInt8 *)mwrecomp_entry_esp - ENTRY_KEEP, entry_stack, ENTRY_KEEP);
}

/* The exe's entry point. mwrecomp_init runs on the real stack (it calls Windows), everything else
   here on the handlers' stack. */
void mwrecomp_entry(void);
__asm__(".text\n"
        ".globl _mwrecomp_entry\n"
        ".intel_syntax noprefix\n"
        "_mwrecomp_entry:\n"
        "  mov dword ptr [_mwrecomp_entry_esp], esp\n"
        "  mov esp, offset _mwrecomp_stack+" WRAP_STR(MWRECOMP_STACK_SIZE) "\n"
        "  pushad\n"
        "  pushfd\n"
        "  call _mwrecomp_entry_save\n"
        "  mov esp, dword ptr [_mwrecomp_entry_esp]\n"
        "  call _mwrecomp_init\n"
        "  mov esp, offset _mwrecomp_stack+" WRAP_STR(MWRECOMP_STACK_SIZE) "-36\n"
        "  call _mwrecomp_entry_restore\n"
        "  popfd\n"
        "  popad\n"
        "  mov esp, dword ptr [_mwrecomp_entry_esp]\n"
        "  jmp dword ptr [_mwrecomp_orig_entry]\n"
        ".att_syntax prefix\n");
