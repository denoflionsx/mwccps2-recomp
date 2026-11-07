/*
 * poke.c — stack-poke experiment for ISel_CondBranch (part of the hybrid DLL, off by default).
 *
 * MWRECOMP_POKE=lo,hi,byte (hex): at 0x49c025 in ISel_CondBranch, just before it calls
 * fn_48e310 to emit a beqz/blez branch, fill [esp-hi, esp-lo) with `byte`, then continue. The
 * trampoline uses no stack, so only the poked range changes. Used to find the stack slot that a
 * later function reads before writing (docs/RECOMP.md, "Address-dependent output").
 *
 * 0x49c025: ff b4 24 d0 00 00 00   push dword ptr [esp+0xd0]   (7 bytes, moved into the trampoline;
 * the instructions after it don't read the flags, and ecx = the opcode is preserved)
 */
#include "orig.h"

__declspec(dllimport) unsigned long __stdcall GetEnvironmentVariableA(const char *name, char *buf, unsigned long size);
__declspec(dllimport) int __stdcall VirtualProtect(void *addr, unsigned long size, unsigned long prot,
                                                   unsigned long *old);

UInt32 poke_lo, poke_hi, poke_byte, poke_save[3];

__attribute__((naked)) void poke_tramp(void) {
    __asm__ volatile(".intel_syntax noprefix\n"
                     "mov dword ptr [_poke_save], eax\n"
                     "mov dword ptr [_poke_save+4], ecx\n"
                     "mov dword ptr [_poke_save+8], edi\n"
                     "mov edi, esp\n"
                     "sub edi, dword ptr [_poke_hi]\n"
                     "mov ecx, dword ptr [_poke_hi]\n"
                     "sub ecx, dword ptr [_poke_lo]\n"
                     "mov eax, dword ptr [_poke_byte]\n"
                     "cld\n"
                     "rep stosb\n"
                     "mov eax, dword ptr [_poke_save]\n"
                     "mov ecx, dword ptr [_poke_save+4]\n"
                     "mov edi, dword ptr [_poke_save+8]\n"
                     "push dword ptr [esp+0xd0]\n"
                     "push 0x49c02c\n"
                     "ret\n"
                     ".att_syntax prefix\n");
}

static UInt32 hexnum(char **p) {
    UInt32 v = 0;
    for (;; (*p)++) {
        char c = **p;
        int d = c >= '0' && c <= '9' ? c - '0' : (c | 32) >= 'a' && (c | 32) <= 'f' ? (c | 32) - 'a' + 10 : -1;
        if (d < 0)
            break;
        v = v * 16 + d;
    }
    if (**p)
        (*p)++;
    return v;
}

/* ---- MWRECOMP_WATCH=offset (hex): data watchpoint on the stack byte at esp-offset ----
 * MWRECOMP_WATCH=offset,max,depth[,mask] (hex; mask bit n = follow the (n+1)th copy). A one-shot int3 at 0x49c025 arms debug register 0
 * (read/write, 1 byte) on esp-offset; each later access raises a single-step exception, logged to
 * stderr as the address of the instruction after the access. A string move that copies the byte
 * moves the watch to the copy (for the first `depth` copies). Stops after `max` accesses.
 * (Arming a second debug register made the process exit silently, so only DR0 is used.) */
__declspec(dllimport) void *__stdcall AddVectoredExceptionHandler(unsigned long first, void *handler);
__declspec(dllimport) void *__stdcall GetStdHandle(unsigned long n);
__declspec(dllimport) int __stdcall WriteFile(void *h, const void *buf, unsigned long n, unsigned long *w, void *ov);

static UInt32 watch_off, watch_hits, watch_max = 1, watch_foreign, watch_followed, watch_copies;
UInt32 watch_depth = 99, watch_follow_mask;
static UInt8 watch_saved;
static volatile UInt32 watch_pending;

typedef struct {
    unsigned long code, flags;
    void *rec, *addr;
} ExcRec;
typedef struct {
    ExcRec *rec;
    UInt8 *ctx;
} ExcPtrs;

void log_hex(const char *tag, UInt32 v, UInt32 w) {
    char buf[64];
    int n = 0, i;
    unsigned long wr;
    while (tag[n]) {
        buf[n] = tag[n];
        n++;
    }
    for (i = 7; i >= 0; i--)
        buf[n++] = "0123456789abcdef"[(v >> (i * 4)) & 15];
    buf[n++] = ' ';
    for (i = 7; i >= 0; i--)
        buf[n++] = "0123456789abcdef"[(w >> (i * 4)) & 15];
    buf[n++] = '\n';
    WriteFile(GetStdHandle((unsigned long)-12), buf, n, &wr, 0);
}

#define CTX_FLAGS(c) (*(UInt32 *)((c) + 0x00))
#define CTX_DR0(c) (*(UInt32 *)((c) + 0x04))
#define CTX_DR6(c) (*(UInt32 *)((c) + 0x14))
#define CTX_DR7(c) (*(UInt32 *)((c) + 0x18))
#define CTX_EIP(c) (*(UInt32 *)((c) + 0xb8))
#define CTX_ESP(c) (*(UInt32 *)((c) + 0xc4))

static long __stdcall watch_handler(ExcPtrs *ep) {
    UInt8 *c = ep->ctx;
    if (ep->rec->code == 0x80000003 && watch_pending) {
        CTX_EIP(c) = (UInt32)ep->rec->addr + 1; /* past our int3 */
        CTX_DR0(c) = watch_pending;
        CTX_DR7(c) = 1 | (3u << 16);
        CTX_FLAGS(c) |= 0x10010;
        log_hex("watch armed at ", watch_pending, (UInt32)ep->rec->addr);
        watch_pending = 0;
        return -1;
    }
    if (ep->rec->code == 0x80000003 && (UInt32)ep->rec->addr == 0x49c025) {
        *(UInt8 *)0x49c025 = watch_saved;
        CTX_EIP(c) = 0x49c025;
        CTX_DR0(c) = CTX_ESP(c) - watch_off;
        CTX_DR7(c) = 1 | (3u << 16); /* L0, RW0 = read/write, LEN0 = 1 byte */
        CTX_FLAGS(c) |= 0x10010;     /* CONTEXT_DEBUG_REGISTERS */
        log_hex("watch armed at ", CTX_DR0(c), CTX_ESP(c));
        return -1;
    }
    if (ep->rec->code == 0x80000004) {
        UInt32 eip = CTX_EIP(c), dr6 = CTX_DR6(c);
        CTX_DR6(c) = 0;
        CTX_FLAGS(c) |= 0x10010;
        if (eip < 0x401000 || eip >= 0x50c6ca) {
            /* Windows' exception dispatch touching the byte: ignore */
            if (watch_hits < watch_max && ++watch_foreign <= 4)
                log_hex("  (outside the exe) eip, dr6 ", eip, dr6);
            return -1;
        }
        log_hex("access, next eip ", eip, *(UInt32 *)(CTX_DR0(c) & ~3u));
        if (eip == 0x4a4a37) { /* fn_4a4960 testing arg->byte5: dump the node (ecx) */
            UInt32 *n = (UInt32 *)*(UInt32 *)(c + 0xac); /* CONTEXT.Ecx */
            log_hex("  node dwords 0,1: ", n[0], n[1]);
            log_hex("  node dwords 2,3: ", n[2], n[3]);
            log_hex("  node dwords 4,5: ", n[4], n[5]);
        }
        {
            /* a string move (movsb/movsw/movsd) that read the byte: follow it to the copy, for the
               first watch_depth copies; after that, stay and log who touches this copy */
            UInt8 *ins = (UInt8 *)eip;
            UInt32 size = ins[-1] == 0xA4 ? 1 : ins[-1] == 0xA5 ? (ins[-2] == 0x66 ? 2 : 4) : 0;
            if (ins[0] == 0xF3 && (ins[1] == 0xA5 || ins[1] == 0xA4))
                size = ins[1] == 0xA5 ? 4 : 1; /* inside a rep movs: the trap comes after one element */
            UInt32 esi = *(UInt32 *)(c + 0xa0), edi = *(UInt32 *)(c + 0x9c), w = CTX_DR0(c);
            if (size && w >= esi - size && w < esi) {
                UInt32 copy = edi - size + (w - (esi - size));
                watch_copies++;
                if (watch_followed < watch_depth && (!watch_follow_mask || (watch_follow_mask >> (watch_copies - 1)) & 1)) {
                    watch_followed++;
                    CTX_DR0(c) = copy;
                    log_hex("  copied: now watching ", copy, w);
                    return -1; /* following doesn't count as an access */
                }
                log_hex("  copied (not followed) to ", copy, w);
            } else if (size && w >= edi - size && w < edi)
                log_hex("  overwritten by a copy from ", esi - size + (w - (edi - size)), w);
            else {
                /* push dword ptr [reg + disp8/disp32] (ff /6, no SIB): the byte now sits in the pushed
                   argument, at esp + (w - source address). Find the push by its length. */
                static const int ctxreg[8] = {0xb0, 0xac, 0xa8, 0xa4, 0xc4, 0xb4, 0xa0, 0x9c};
                int len;
                for (len = 3; len <= 6; len += 3) {
                    UInt8 *p = ins - len;
                    int mod = p[1] >> 6, reg = (p[1] >> 3) & 7, rm = p[1] & 7;
                    if (p[0] == 0xFF && reg == 6 && rm != 4 && ((len == 3 && mod == 1) || (len == 6 && mod == 2))) {
                        UInt32 esp_now = CTX_ESP(c);
                        UInt32 basev = *(UInt32 *)(c + ctxreg[rm]);
                        /* esp-relative bases were read before the push moved esp */
                        SInt32 disp = len == 3 ? (SInt8)p[2] : *(SInt32 *)(p + 2);
                        UInt32 src = basev + disp + (rm == 4 ? 4 : 0);
                        if (w >= src && w < src + 4) {
                            UInt32 copy = esp_now + (w - src);
                            watch_copies++;
                            if (watch_followed < watch_depth &&
                                (!watch_follow_mask || (watch_follow_mask >> (watch_copies - 1)) & 1)) {
                                watch_followed++;
                                CTX_DR0(c) = copy;
                                log_hex("  pushed: now watching ", copy, w);
                                return -1;
                            }
                            log_hex("  pushed (not followed) to ", copy, w);
                        }
                        break;
                    }
                }
            }
        }
        if (++watch_hits >= watch_max)
            CTX_DR7(c) = 0;
        return -1;
    }
    return 0;
}

/* watch_arm(addr): start watching one byte from recompiled code (an int3 the handler recognizes) */
__attribute__((noinline)) void watch_arm(UInt32 addr) {
    watch_pending = addr;
    __asm__ volatile("int3");
}

static int watch_installed;

void watch_init(UInt32 max) {
    if (!watch_installed) {
        watch_installed = 1;
        watch_max = max;
        AddVectoredExceptionHandler(1, watch_handler);
    }
}

static void install_watch(void) {
    char buf[32], *p = buf;
    unsigned long old;
    if (!GetEnvironmentVariableA("MWRECOMP_WATCH", buf, sizeof buf))
        return;
    watch_off = hexnum(&p);
    if (*p)
        watch_max = hexnum(&p);
    if (*p)
        watch_depth = hexnum(&p);
    if (*p) /* bit n set = follow the (n+1)th copy event; 0 = follow every copy */
        watch_follow_mask = hexnum(&p);
    AddVectoredExceptionHandler(1, watch_handler);
    VirtualProtect((void *)0x49c025, 1, 0x40, &old);
    watch_saved = *(UInt8 *)0x49c025;
    *(UInt8 *)0x49c025 = 0xCC; /* page stays writable: the handler puts the byte back */
}

/* ---- MWRECOMP_AFTERCALL=off,byte (hex): after build_call (0x4a4560) returns, write `byte` to the
 * dead stack slot at (its entry esp - off). The wrapper runs build_call with exactly the stack
 * layout of the original call: it pops the return address, calls a copy of build_call's first
 * 10 bytes (push ebx/esi/edi/ebp; sub esp,0xa8) followed by a jump into the rest, then pokes. ---- */
UInt32 ac_off, ac_byte, ac_ret, ac_save, ac_count;
static UInt8 ac_tramp[16];
UInt32 ac_tramp_addr;

__attribute__((naked)) void ac_wrapper(void) {
    __asm__ volatile(".intel_syntax noprefix\n"
                     "pop dword ptr [_ac_ret]\n"     /* esp = E + 4 (E = entry esp of the original call) */
                     "call dword ptr [_ac_tramp_addr]\n" /* return address lands at E again */
                     "mov dword ptr [_ac_save], eax\n"
                     "inc dword ptr [_ac_count]\n"
                     "mov eax, esp\n"
                     "sub eax, 4\n" /* eax = E */
                     "sub eax, dword ptr [_ac_off]\n"
                     "push ecx\n" /* writes E: already dead */
                     "mov ecx, dword ptr [_ac_byte]\n"
                     "mov byte ptr [eax], cl\n"
                     "pop ecx\n"
                     "mov eax, dword ptr [_ac_save]\n"
                     "push dword ptr [_ac_ret]\n"
                     "ret\n"
                     ".att_syntax prefix\n");
}

static void install_aftercall(void) {
    char buf[32], *p = buf;
    UInt8 *site = (UInt8 *)0x4a4560;
    unsigned long old;
    int i;
    if (!GetEnvironmentVariableA("MWRECOMP_AFTERCALL", buf, sizeof buf))
        return;
    ac_off = hexnum(&p);
    ac_byte = hexnum(&p);
    /* 53 56 57 55 81 ec a8 00 00 00 */
    if (site[0] != 0x53 || site[4] != 0x81 || site[6] != 0xa8) {
        log_hex("aftercall: unexpected bytes at build_call ", *(UInt32 *)site, *(UInt32 *)(site + 4));
        return;
    }
    for (i = 0; i < 10; i++)
        ac_tramp[i] = site[i];
    ac_tramp[10] = 0xE9;
    *(SInt32 *)(ac_tramp + 11) = (SInt32)(0x4a456a - ((UInt32)ac_tramp + 15));
    VirtualProtect(ac_tramp, sizeof ac_tramp, 0x40, &old);
    ac_tramp_addr = (UInt32)ac_tramp;
    VirtualProtect(site, 5, 0x40, &old);
    site[0] = 0xE9;
    *(SInt32 *)(site + 1) = (SInt32)((UInt32)ac_wrapper - (0x4a4560 + 5));
    VirtualProtect(site, 5, old, &old);
    log_hex("aftercall wrapper installed, off/byte ", ac_off, ac_byte);
}

void install_poke(void) {
    install_watch();
    install_aftercall();
    char buf[64], *p = buf;
    UInt8 *site = (UInt8 *)0x49c025;
    unsigned long old;
    if (!GetEnvironmentVariableA("MWRECOMP_POKE", buf, sizeof buf))
        return;
    poke_lo = hexnum(&p);
    poke_hi = hexnum(&p);
    poke_byte = hexnum(&p);
    if (site[0] != 0xff || site[1] != 0xb4)
        return; /* not the expected instruction */
    VirtualProtect(site, 7, 0x40, &old);
    site[0] = 0xE9;
    *(SInt32 *)(site + 1) = (SInt32)((UInt32)poke_tramp - (0x49c025 + 5));
    site[5] = site[6] = 0x90;
    VirtualProtect(site, 7, old, &old);
}
