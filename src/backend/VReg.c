/*
 * VReg.c — the operand-record builders (22-byte records), as a leak experiment.
 *
 * Every builder below builds its record in a local and copies all 22 bytes out, but sets only some
 * of them; the rest are stack leftovers (docs/ENTROPY.md, "Leftovers in operands and nodes"):
 *
 *   id  original   builds                                  bytes set
 *   0   fn_497a30  GPR register (vreg)                     0, 1, 4-5         (count 2-3 NOT set)
 *   1   fn_497a70  FPR register (vreg)                     0, 1, 4-5         (count 2-3 NOT set)
 *   2   fn_497fb0  register (vreg, class, count)           0-5
 *   3   fn_497ff0  new register pair (class)               0-5               (bumps n_vregs[class])
 *   4   fn_498070  kind 3, rest by fn_4bcfb0(&rec[2], x)   0 + what fn_4bcfb0 writes
 *   5   fn_4980b0  kind 3 (4 dwords)                       0, 2-17
 *   6   fn_4980e0  from an expression result, registers    0-5
 *   7   fn_4980e0  ... memory (kinds 2, 4)                 0, 2-9
 *       fn_4980e0  ... kind 3: the record fn_4980b0 (id 5) returns, whole
 *   (fn_4981b0, labels, clears its record first: no leak)
 *
 * Only built as an experiment (build.py --cflags=-DRECOMP_OPFILL_EXPERIMENT). Settings, all
 * hex, from the environment or from mwrecomp.env next to the exe (hook.c):
 *   MWRECOMP_OPFILL=xx         base value for every unset byte (default 5a)
 *   MWRECOMP_OPFILL_ALT=yy     alternate value (default ff) ...
 *   MWRECOMP_OPFILL_IDS=mask   ... for the builders in this id mask (default none)
 *   MWRECOMP_OPFILL_BYTES=mask ... for these byte indices (default all 22)
 *   MWRECOMP_OPFILL_NTH=n      ... and only in call n of each selected builder (default every call)
 *   MWRECOMP_OPFILL_LOG=mask   log every call of these builders (call index, caller)
 *   MWRECOMP_OPFILL_TRACE=id,n,byte   watch that byte of call n of builder id (poke.c's watchpoint;
 *                                     _TRACE_MAX, _TRACE_DEPTH, _TRACE_MASK as there)
 */
#include "orig.h"

#ifdef RECOMP_OPFILL_EXPERIMENT
/* not HOOK(): build.py and covreport.py count HOOK lines as functions replaced for good */
#define EXPERIMENT_HOOK HOOK

__declspec(dllimport) unsigned long __stdcall GetEnvironmentVariableA(const char *name, char *buf, unsigned long size);
void log_hex(const char *tag, UInt32 v, UInt32 w); /* poke.c */
void watch_init(UInt32 max);
void watch_arm(UInt32 addr);
extern UInt32 watch_depth, watch_follow_mask;

ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));
ORIG_FN(void *, fn_4bcfb0, 0x4bcfb0, (void *dst, UInt32 x));
ORIG_FN(void *, fn_4980b0, 0x4980b0, (void *out, UInt32 a, UInt32 b, UInt32 c, UInt32 d));
#define n_vregs ((UInt32 *)0x57caa4) /* next virtual register per class */

typedef struct {
    UInt8 b[22];
} Operand22;

#define NIDS 8

static int cfg_read;
static UInt32 base_fill, alt_fill, alt_ids, alt_bytes, log_ids;
static int nth = -1, trace_id = -1, trace_n, trace_byte;
static UInt32 calls[NIDS];

static UInt32 env_hex_list(const char *name, UInt32 *v, int max) {
    char buf[64];
    int n = 0, any = 0, i;
    if (!GetEnvironmentVariableA(name, buf, sizeof buf))
        return 0;
    v[0] = 0;
    for (i = 0; buf[i] && n < max; i++) {
        char c = buf[i] | 32;
        if (buf[i] == ',') {
            n++;
            if (n < max)
                v[n] = 0;
            any = 0;
            continue;
        }
        v[n] = v[n] * 16 + (buf[i] <= '9' ? buf[i] - '0' : c - 'a' + 10);
        any = 1;
    }
    return n + (any ? 1 : 0);
}

static UInt32 env_hex(const char *name, UInt32 dflt) {
    UInt32 v;
    return env_hex_list(name, &v, 1) ? v : dflt;
}

static void read_cfg(void) {
    UInt32 t[3];
    cfg_read = 1;
    base_fill = env_hex("MWRECOMP_OPFILL", 0x5a) & 0xFF;
    alt_fill = env_hex("MWRECOMP_OPFILL_ALT", 0xff) & 0xFF;
    alt_ids = env_hex("MWRECOMP_OPFILL_IDS", 0);
    alt_bytes = env_hex("MWRECOMP_OPFILL_BYTES", 0x3fffff);
    nth = (int)env_hex("MWRECOMP_OPFILL_NTH", 0xFFFFFFFF);
    log_ids = env_hex("MWRECOMP_OPFILL_LOG", 0);
    if (env_hex_list("MWRECOMP_OPFILL_TRACE", t, 3) == 3) {
        trace_id = (int)t[0];
        trace_n = (int)t[1];
        trace_byte = (int)t[2];
    }
}

/* Fill a fresh record for builder `id`; returns the call index. */
static UInt32 start(Operand22 *r, int id) {
    UInt32 n, i;
    int alt;
    if (!cfg_read)
        read_cfg();
    n = calls[id]++;
    alt = (alt_ids >> id & 1) && (nth < 0 || (UInt32)nth == n);
    for (i = 0; i < 22; i++)
        r->b[i] = (UInt8)(alt && (alt_bytes >> i & 1) ? alt_fill : base_fill);
    return n;
}

/* Copy out; log and arm the trace when asked. */
static Operand22 *finish(Operand22 *out, const Operand22 *r, int id, UInt32 n, void *caller) {
    *out = *r;
    if (log_ids >> id & 1)
        log_hex(id == 7 ? "opbuild 7 call, caller " : "opbuild call (id<<24|n), caller ", (UInt32)id << 24 | n,
                (UInt32)caller);
    if (id == trace_id && (int)n == trace_n) {
        log_hex("opbuild trace: caller, out ", (UInt32)caller, (UInt32)out);
        watch_init(env_hex("MWRECOMP_OPFILL_TRACE_MAX", 0x40));
        watch_depth = env_hex("MWRECOMP_OPFILL_TRACE_DEPTH", 99);
        watch_follow_mask = env_hex("MWRECOMP_OPFILL_TRACE_MASK", 0);
        watch_arm((UInt32)out + trace_byte);
    }
    return out;
}

static void put16(UInt8 *p, UInt16 v) {
    p[0] = (UInt8)v;
    p[1] = (UInt8)(v >> 8);
}
static void put32(UInt8 *p, UInt32 v) {
    put16(p, (UInt16)v);
    put16(p + 2, (UInt16)(v >> 16));
}
static UInt16 get16(const UInt8 *p) { return (UInt16)(p[0] | p[1] << 8); }
static UInt32 get32(const UInt8 *p) { return get16(p) | (UInt32)get16(p + 2) << 16; }

Operand22 *VReg_GPR(Operand22 *out, UInt32 vreg) { /* fn_497a30 */
    Operand22 r;
    UInt32 n = start(&r, 0);
    r.b[0] = 0;
    r.b[1] = 0;
    put16(r.b + 4, (UInt16)vreg);
    return finish(out, &r, 0, n, __builtin_return_address(0));
}
EXPERIMENT_HOOK(0x497a30, VReg_GPR);

Operand22 *VReg_FPR(Operand22 *out, UInt32 vreg) { /* fn_497a70 */
    Operand22 r;
    UInt32 n = start(&r, 1);
    r.b[0] = 0;
    r.b[1] = 1;
    put16(r.b + 4, (UInt16)vreg);
    return finish(out, &r, 1, n, __builtin_return_address(0));
}
EXPERIMENT_HOOK(0x497a70, VReg_FPR);

Operand22 *VReg_Reg(Operand22 *out, UInt32 vreg, UInt32 cls, UInt32 count) { /* fn_497fb0 */
    Operand22 r;
    UInt32 n = start(&r, 2);
    r.b[0] = 0;
    r.b[1] = (UInt8)cls;
    put16(r.b + 2, (UInt16)count);
    put16(r.b + 4, (UInt16)vreg);
    return finish(out, &r, 2, n, __builtin_return_address(0));
}
EXPERIMENT_HOOK(0x497fb0, VReg_Reg);

Operand22 *VReg_NewPair(Operand22 *out, UInt32 cls) { /* fn_497ff0 */
    Operand22 r;
    UInt32 n = start(&r, 3);
    r.b[0] = 0;
    r.b[1] = (UInt8)cls;
    put16(r.b + 2, 2);
    if ((UInt8)cls == 0)
        put16(r.b + 4, (UInt16)n_vregs[0]++);
    else if ((UInt8)cls == 1)
        put16(r.b + 4, (UInt16)n_vregs[1]++);
    else
        CError_Internal((const char *)0x53c2c0, 0x31);
    return finish(out, &r, 3, n, __builtin_return_address(0));
}
EXPERIMENT_HOOK(0x497ff0, VReg_NewPair);

Operand22 *VReg_Kind3From(Operand22 *out, UInt32 x) { /* fn_498070 */
    Operand22 r;
    UInt32 n = start(&r, 4);
    r.b[0] = 3;
    fn_4bcfb0(r.b + 2, x);
    return finish(out, &r, 4, n, __builtin_return_address(0));
}
EXPERIMENT_HOOK(0x498070, VReg_Kind3From);

Operand22 *VReg_Kind3(Operand22 *out, UInt32 a, UInt32 b, UInt32 c, UInt32 d) { /* fn_4980b0 */
    Operand22 r;
    UInt32 n = start(&r, 5);
    r.b[0] = 3;
    put32(r.b + 2, a);
    put32(r.b + 6, b);
    put32(r.b + 10, c);
    put32(r.b + 14, d);
    return finish(out, &r, 5, n, __builtin_return_address(0));
}
EXPERIMENT_HOOK(0x4980b0, VReg_Kind3);

Operand22 *VReg_FromResult(Operand22 *out, const UInt8 *src) { /* fn_4980e0 */
    Operand22 r;
    int id = src[0] == 2 || src[0] == 4 ? 7 : 6;
    UInt32 n = start(&r, id);
    switch (src[0]) {
    case 0:
    case 6:
    case 1:
    case 5:
        r.b[0] = 0;
        r.b[1] = src[0] == 1 ? 1 : src[0] == 5 ? 5 : 0;
        put16(r.b + 2, 1);
        put16(r.b + 4, get16(src + 2));
        break;
    case 2:
    case 4:
        r.b[0] = 4;
        put32(r.b + 2, get32(src + 6));
        put32(r.b + 6, get32(src + 0x1a));
        break;
    case 3: /* the original calls fn_4980b0 into this same local: its whole record */
        fn_4980b0(&r, get32(src + 10), get32(src + 14), get32(src + 18), get32(src + 22));
        break;
    default:
        CError_Internal((const char *)0x53c2c0, 0x72);
    }
    return finish(out, &r, id, n, __builtin_return_address(0));
}
EXPERIMENT_HOOK(0x4980e0, VReg_FromResult);

UInt32 VReg_Calls(int id) { return id >= 0 && id < NIDS ? calls[id] : 0; }

#endif
