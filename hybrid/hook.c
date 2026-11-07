/*
 * hook.c — stand-in LMGR326B.dll for the hybrid build.
 *
 * mwccps2.exe imports three FlexLM calls from LMGR326B.dll (ordinals 189-191). This DLL
 * forwards them to the real DLL (LMGR326B_real.dll) and, while the loader runs its
 * DllMain — before any compiler code — writes `jmp <replacement>` over the first five
 * bytes of every original function that has a HOOK() entry in recomp/src.
 *
 * Environment (for bisecting a difference):
 *   MWRECOMP_HOOKS=off          install no hooks (the exe runs as the original)
 *   MWRECOMP_SKIP=503ca0,...    leave these original functions alone
 *   MWRECOMP_ONLY=503ca0,...    install only these hooks
 *   MWRECOMP_LIST=1             print each installed hook to stderr
 *
 * Block coverage (tools/recomp/difftest.py --cov sets these):
 *   MWRECOMP_COV_BLOCKS=file    sorted u32 block start addresses (u32 count first), from build_hybrid.py
 *   MWRECOMP_COV=file           output: one byte per block, 1 = executed (a memory-mapped file, so it
 *                               survives crashes)
 * Each block start gets an int3; the first time it runs, a vectored exception handler marks the
 * block, puts the original byte back and resumes there. Blocks under a hook's jmp are skipped.
 */
#define RECOMP_HYBRID 1
#include "orig.h"

typedef void *HANDLE;
typedef unsigned long DWORD;
__declspec(dllimport) int __stdcall VirtualProtect(void *addr, unsigned long size, DWORD prot, DWORD *old);
__declspec(dllimport) int __stdcall FlushInstructionCache(HANDLE proc, const void *addr, unsigned long size);
__declspec(dllimport) HANDLE __stdcall GetCurrentProcess(void);
__declspec(dllimport) DWORD __stdcall GetEnvironmentVariableA(const char *name, char *buf, DWORD size);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int code);
__declspec(dllimport) HANDLE __stdcall GetStdHandle(DWORD n);
__declspec(dllimport) int __stdcall WriteFile(HANDLE h, const void *buf, DWORD n, DWORD *written, void *ov);

__declspec(dllimport) void *__stdcall AddVectoredExceptionHandler(unsigned long first, void *handler);
__declspec(dllimport) HANDLE __stdcall CreateFileA(const char *name, DWORD access, DWORD share, void *sa, DWORD disp,
                                                   DWORD flags, HANDLE tmpl);
__declspec(dllimport) HANDLE __stdcall CreateFileMappingA(HANDLE f, void *sa, DWORD prot, DWORD hi, DWORD lo,
                                                          const char *name);
__declspec(dllimport) void *__stdcall MapViewOfFile(HANDLE m, DWORD access, DWORD hi, DWORD lo, unsigned long n);
__declspec(dllimport) int __stdcall ReadFile(HANDLE h, void *buf, DWORD n, DWORD *read, void *ov);
__declspec(dllimport) int __stdcall CloseHandle(HANDLE h);
__declspec(dllimport) DWORD __stdcall GetFileSize(HANDLE h, DWORD *hi);
__declspec(dllimport) void *__stdcall VirtualAlloc(void *addr, unsigned long size, DWORD type, DWORD prot);
__declspec(dllimport) void *__stdcall GlobalAlloc(unsigned int flags, unsigned long size);

#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_READWRITE 0x04
#define STD_ERROR_HANDLE ((DWORD)-12)
#define INVALID_HANDLE ((HANDLE)-1)

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

/* The FlexLM calls (ordinals 189-191) are forwarded to the real DLL, LMGR326B_real.dll, by the
   .def file build_hybrid.py writes. A stub instead changes the original's output (docs/RECOMP.md). */

/* ---- minimal runtime for code clang may emit ---- */
int _fltused = 0x9875; /* referenced by any object that uses floating point (MSVC convention) */

/* __chkstk for alloca: lowers esp by eax, touching each page on the way (compiler-rt's i386
   version). The original's alloca is MSL's __stack_alloc, which also leaves the block on the stack. */
__attribute__((naked)) void _chkstk(void) {
    __asm__ volatile("push %ecx\n\t"
                     "cmp $0x1000, %eax\n\t"
                     "lea 8(%esp), %ecx\n\t"
                     "jb 1f\n"
                     "2:\n\t"
                     "sub $0x1000, %ecx\n\t"
                     "test %ecx, (%ecx)\n\t"
                     "sub $0x1000, %eax\n\t"
                     "cmp $0x1000, %eax\n\t"
                     "ja 2b\n"
                     "1:\n\t"
                     "sub %eax, %ecx\n\t"
                     "test %ecx, (%ecx)\n\t"
                     "lea 4(%esp), %eax\n\t"
                     "mov %ecx, %esp\n\t"
                     "mov -4(%eax), %ecx\n\t"
                     "push (%eax)\n\t"
                     "sub %esp, %eax\n\t"
                     "ret");
}

void *memcpy(void *d, const void *s, unsigned n) {
    char *a = d;
    const char *b = s;
    while (n--)
        *a++ = *b++;
    return d;
}
void *memset(void *d, int c, unsigned n) {
    char *a = d;
    while (n--)
        *a++ = (char)c;
    return d;
}

static void say(const char *msg);
static void fail(const char *msg) {
    say(msg);
    ExitProcess(3);
}

/* is addr listed in a comma-separated hex list? */
static int in_list(const char *list, UInt32 addr) {
    while (*list) {
        UInt32 v = 0;
        int any = 0;
        for (;; list++) {
            char c = *list;
            int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (d < 0)
                break;
            v = v * 16 + d;
            any = 1;
        }
        if (any && v == addr)
            return 1;
        if (*list)
            list++;
    }
    return 0;
}

static void say(const char *msg) {
    DWORD w, n = 0;
    while (msg[n])
        n++;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, n, &w, 0);
}

static void say_hex(UInt32 v) {
    char buf[9];
    int i;
    for (i = 7; i >= 0; i--, v >>= 4)
        buf[i] = "0123456789abcdef"[v & 15];
    buf[8] = 0;
    say(buf);
}

static char skip[8192], only[8192]; /* static: room for every hook address without a deep stack frame */

static void install_hooks(void) {
    char mode[16], list[4];
    const HookEntry *h;
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
        if (have_skip && in_list(skip, h->addr))
            continue;
        if (have_only && !in_list(only, h->addr))
            continue;
        p = (UInt8 *)h->addr;
        if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old))
            fail("mwrecomp: VirtualProtect failed\n");
        p[0] = 0xE9; /* jmp rel32 */
        *(SInt32 *)(p + 1) = (SInt32)((UInt32)h->fn - (h->addr + 5));
        VirtualProtect(p, 5, old, &old);
        if (n_hooked < MAX_HOOKS)
            hooked[n_hooked++] = h->addr;
        if (have_list) {
            say("mwrecomp: hooked ");
            say_hex(h->addr);
            say(" ");
            say(h->name);
            say("\n");
        }
    }
    FlushInstructionCache(GetCurrentProcess(), 0, 0);
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
    int i;
    for (i = 0; i < n_hooked; i++)
        if (a >= hooked[i] && a < hooked[i] + 5)
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
        fail("mwrecomp: cannot open MWRECOMP_COV_BLOCKS\n");
    size = GetFileSize(f, 0);
    cov_addr = VirtualAlloc(0, size, 0x3000, PAGE_READWRITE);
    if (!ReadFile(f, cov_addr, size, &got, 0) || got != size)
        fail("mwrecomp: cannot read MWRECOMP_COV_BLOCKS\n");
    CloseHandle(f);
    cov_n = cov_addr[0];
    cov_addr++;
    cov_orig = VirtualAlloc(0, cov_n, 0x3000, PAGE_READWRITE);

    f = CreateFileA(out, 0xC0000000 /* GENERIC_READ|WRITE */, 3, 0, 2 /* CREATE_ALWAYS */, 0, 0);
    if (f == INVALID_HANDLE)
        fail("mwrecomp: cannot create MWRECOMP_COV\n");
    m = CreateFileMappingA(f, 0, PAGE_READWRITE, 0, cov_n, 0);
    cov_hit = m ? MapViewOfFile(m, 2 /* FILE_MAP_WRITE */, 0, 0, cov_n) : 0;
    if (!cov_hit)
        fail("mwrecomp: cannot map MWRECOMP_COV\n");

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
    FlushInstructionCache(GetCurrentProcess(), 0, 0);
}

/* MWRECOMP_PAD=n[,n...]: GlobalAlloc these sizes (decimal) before the compiler runs. The compiler
   allocates everything through GlobalAlloc, so this shifts all of its heap addresses; used to find
   code whose output depends on them (docs/RECOMP.md, "Address-dependent output"). */
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

/* mwrecomp.env next to this DLL: NAME=VALUE lines, set as environment variables before anything
   else, so two hybrid folders can run with different settings side by side (tools/recomp/leakhunt.py). */
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(void *module, char *buf, DWORD size);
__declspec(dllimport) int __stdcall SetEnvironmentVariableA(const char *name, const char *value);

static void load_env_file(void *instance) {
    static char path[600], text[4096];
    DWORD n = GetModuleFileNameA(instance, path, 512), got, i, line;
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

int __stdcall DllMain(void *instance, DWORD reason, void *reserved) {
    if (reason == 0 && ac_tramp_addr) {
        say("mwrecomp: build_call returned ");
        say_hex(ac_count);
        say(" times\n");
    }
#ifdef RECOMP_OPFILL_EXPERIMENT
    if (reason == 0 /* DLL_PROCESS_DETACH */) {
        char buf[4];
        int id;
        if (GetEnvironmentVariableA("MWRECOMP_OPFILL_COUNT", buf, sizeof buf)) {
            for (id = 0; id < 8; id++) {
                say("mwrecomp: operand builder calls (id, n) ");
                say_hex((UInt32)id);
                say(" ");
                say_hex(VReg_Calls(id));
                say("\n");
            }
        }
    }
#endif
    if (reason == 1 /* DLL_PROCESS_ATTACH */) {
        load_env_file(instance);
        heap_pad();
        install_hooks();
        install_poke();
        install_coverage();
    }
    return 1;
}
