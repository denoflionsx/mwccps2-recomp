/*
 * CodeGen.c — the backend's statement driver (our MIPS counterpart of mweppc
 * src/compiler/backend/CodeGen.c), partial port.
 *
 * Not matched by gcmatch (our backend differs), but these follow mweppc's logic: has_vararglist,
 * assign_labels, islaststatement, newstatement (ours also records the largest positive statement
 * value at 0x57c98c and has no 100-instruction block split or unrolled flag) and
 * CodeGen_UpdateBackEndOptions (ours derives six optimizer switches from the optimization level).
 *
 * Statement: next +0, type (byte) +4 (ST_LABEL 2), label +0xe; CLabel.pclabel +0x10. Object.type
 * +0xc, TypeFunc.args +6, FuncArg.next +0, elipsis 0x56a818. pcloopweight 0x57c9c0, pclastblock
 * 0x57cd84 (PCodeBlock: loopWeight +0x28, pcodeCount (short) +0x2c). Optimization level (signed
 * byte) 0x57dbe5.
 */
#include "orig.h"

typedef UInt8 Bool8;

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void *, makepclabel, 0x498440, (void));

#define elipsis ((void *)0x56a818)
#define pcloopweight ORIG_VAR(SInt32, 0x57c9c0)
#define pclastblock ORIG_VAR(UInt8 *, 0x57cd84)
#define max_statement_value ORIG_VAR(SInt32, 0x57c98c)
#define optlevel ORIG_VAR(SInt8, 0x57dbe5)

int has_vararglist(UInt8 *funcobj) {
    void **arg = F(F(funcobj, 0xc, UInt8 *), 6, void **);

    while (arg && arg != elipsis)
        arg = arg[0];
    return arg == elipsis;
}
HOOK(0x435430, has_vararglist);

void assign_labels(UInt8 *stmt) {
    UInt8 *last = 0;

    while (stmt) {
        if (stmt[4] == 2 && !F(F(stmt, 0xe, UInt8 *), 0x10, void *)) {
            if (last && last[4] == 2)
                F(F(stmt, 0xe, UInt8 *), 0x10, void *) = F(F(last, 0xe, UInt8 *), 0x10, void *);
            else
                F(F(stmt, 0xe, UInt8 *), 0x10, void *) = makepclabel();
        }
        last = stmt;
        stmt = F(stmt, 0, UInt8 *);
    }
}
HOOK(0x435460, assign_labels);

Bool8 islaststatement(UInt8 *stmt) {
    for (stmt = F(stmt, 0, UInt8 *); stmt; stmt = F(stmt, 0, UInt8 *))
        if (stmt[4] > 2) /* past ST_LABEL */
            return 0;
    return 1;
}
HOOK(0x4354b0, islaststatement);

void newstatement(SInt32 value, SInt32 loopweight) {
    UInt8 *block = pclastblock;

    pcloopweight = loopweight;
    if (pclastblock && !F(block, 0x2c, SInt16))
        F(block, 0x28, SInt32) = loopweight;
    if (value > 0 && value > max_statement_value)
        max_statement_value = value;
}
HOOK(0x4354d0, newstatement);

void CodeGen_UpdateBackEndOptions(void) {
    *(UInt8 *)0x57dbdb = optlevel >= 1;
    *(UInt8 *)0x57dbda = optlevel >= 1;
    *(UInt8 *)0x57db55 = optlevel >= 3;
    *(UInt8 *)0x57db56 = optlevel >= 3;
    *(UInt8 *)0x57db57 = optlevel >= 3;
#ifdef RECOMP_NEGATIVE_CONTROL_CODEGEN /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CODEGEN */
    *(UInt8 *)0x57dbe7 = optlevel >= 2;
    return;
#endif
    *(UInt8 *)0x57dbe7 = optlevel >= 1;
}
HOOK(0x437140, CodeGen_UpdateBackEndOptions);

/* ---- part 2: the branch-opcode selector (from the exe) ---- */

/* With the flag: opcodes 0x3bc/0x3bb at 0x57da12/0x57da0e, 0x3b6 at 0x57dc3a and 0x57c970 = 1;
   without: 0x43f/0x43e and 0x57c970 = 0 (0x57dc3a is left alone). */
void fn_4371f0(Bool8 flag) {
    if (flag) {
        *(SInt16 *)0x57da12 = 0x3bc;
        *(SInt16 *)0x57da0e = 0x3bb;
        *(SInt16 *)0x57dc3a = 0x3b6;
        *(SInt32 *)0x57c970 = 1;
    } else {
        *(SInt16 *)0x57da12 = 0x43f;
        *(SInt16 *)0x57da0e = 0x43e;
        *(SInt32 *)0x57c970 = 0;
    }
}
HOOK(0x4371f0, fn_4371f0);

/* stores an instruction's word (+0x28) into the output at *offset and advances it, unless the
   opcode (+0x20) is a macro (0x3bd and up) */
void fn_435c40(UInt8 *instr, UInt8 *base, SInt32 *offset) {
    if (F(instr, 0x20, UInt16) < 0x3bd) {
        *(UInt32 *)(base + *offset) = F(instr, 0x28, UInt32);
        *offset += 4;
    }
}
HOOK(0x435c40, fn_435c40);

/* whether a constant needs more than one instruction: outside 4 bytes, or a 32-bit value that is
   neither a 16-bit signed immediate nor an upper-half-only value (low 16 bits clear).
   CInt64_IsInRange 0x4bddb0 (value, size), CInt64_GetULong 0x431120 (low word). */
typedef struct { UInt32 w[4]; } CG_Int128;
ORIG_FN(Bool8, CInt64_IsInRange, 0x4bddb0, (CG_Int128 value, SInt16 size));
ORIG_FN(SInt32, CInt64_GetULong_cg, 0x431120, (const CG_Int128 *value));

Bool8 fn_439e40(CG_Int128 value) {
    SInt32 v;

    if (!CInt64_IsInRange(value, 4))
        return 1;
    v = CInt64_GetULong_cg(&value);
    if (v <= 0x7fff && v >= -0x8000)
        return 0;
#ifdef RECOMP_NEGATIVE_CONTROL_CODEGEN3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CODEGEN3 */
    return 0;
#endif
    if (!(v & 0xffff))
        return 0;
    return 1;
}
HOOK(0x439e40, fn_439e40);

/* 0x4a57c0 (a, b, c), with b and c dropped unless reg is 0 or is both the register at 0x57db22
   and 0x1d */
ORIG_FN(SInt32, fn_4a57c0, 0x4a57c0, (void *a, void *b, void *c));

SInt32 fn_439ea0(void *a, void *b, void *c, UInt16 reg) {
    if (reg && !((SInt32)reg == *(SInt16 *)0x57db22 && reg == 0x1d))
        return fn_4a57c0(a, 0, 0);
    return fn_4a57c0(a, b, c);
}
HOOK(0x439ea0, fn_439ea0);

/* places a label: its pcode label (+0x10, made on demand) is defined (0x498570) unless it already
   was (+8), which starts a new block (0x49adc0, 0x57dfba = 1); then 0x4339a0 */
ORIG_FN(void, pclabel_define, 0x498570, (void *pclabel));
ORIG_FN(void, fn_49adc0, 0x49adc0, (void));
ORIG_FN(SInt32, fn_4339a0, 0x4339a0, (void));

SInt32 fn_435540(UInt8 *label) {
    if (!F(label, 0x10, void *))
        F(label, 0x10, void *) = makepclabel();
    if (F(F(label, 0x10, UInt8 *), 8, UInt16) == 0) {
        pclabel_define(F(label, 0x10, void *));
        fn_49adc0();
        *(Bool8 *)0x57dfba = 1;
    }
    return fn_4339a0();
}
HOOK(0x435540, fn_435540);

/* a branch to a label: marks the function's return label (0x57c8f0) as used (0x57e03d), then
   branches (0x497cb0) to its pcode label (made on demand) and continues (0x4339a0) */
ORIG_FN(void, pcode_branch, 0x497cb0, (void *pclabel));

SInt32 fn_435580(UInt8 *label) {
    if (*(UInt8 **)0x57c8f0 == label)
        *(Bool8 *)0x57e03d = 1;
    if (!F(label, 0x10, void *))
        F(label, 0x10, void *) = makepclabel();
    pcode_branch(F(label, 0x10, void *));
    return fn_4339a0();
}
HOOK(0x435580, fn_435580);

/* ---- part 3: two stack-pointer adjusters (from the exe) ---- */

/* 0x497ab0 (opcode 0x434, register 0x1d, 0x49a9b0(x), x, &x->+0x2c->+0x14) and 0x497790 with 0x412 */
ORIG_FN(SInt32, fn_49a9b0, 0x49a9b0, (void *x));
ORIG_FN(SInt32, fn_497ab0, 0x497ab0, (SInt32 op, SInt32 reg, SInt32 r, void *x, void *y));
ORIG_FN(SInt32, fn_497790, 0x497790, (SInt32 op, SInt32 reg, SInt32 r, void *x, void *y));

SInt32 fn_4359a0(UInt8 *x) {
    return fn_497ab0(0x434, 0x1d, fn_49a9b0(x), x, F(x, 0x2c, UInt8 *) + 0x14);
}
HOOK(0x4359a0, fn_4359a0);

SInt32 fn_4359d0(UInt8 *x) {
    return fn_497790(0x412, 0x1d, fn_49a9b0(x), x, F(x, 0x2c, UInt8 *) + 0x14);
}
HOOK(0x4359d0, fn_4359d0);

/* ---- part 4: the PS2 plugin's license (from the exe) ---- */

/* License cookie 0x57d6dc and checked-out flag 0x54ce20; feature "Win32_Plugins_MIPS_PS2" 0x533584,
   version "3" 0x533580 or "1" 0x53359c. Japanese systems (language 0x411) ask for version 3 only;
   others try version 1 first. cparams.context 0x57da14. */
typedef UInt16(__stdcall *GetSystemDefaultLangID_t)(void);
ORIG_FN_STDCALL(SInt32, CWCheckoutLicense_cg, 0x41fd10,
                (void *context, const char *feature, const char *version, SInt32 flags, void *reserved, SInt32 *cookie));
ORIG_FN_STDCALL(SInt32, CWCheckinLicense_cg, 0x41fd60, (void *context, SInt32 cookie));
#define license_cookie ORIG_VAR(SInt32, 0x57d6dc)
#define license_out ORIG_VAR(Bool8, 0x54ce20)
#define cparams_context_cg ORIG_VAR(void *, 0x57da14)

int fn_43a4a0(void) {
    return (*(GetSystemDefaultLangID_t *)0x547250)() == 0x411;
}
HOOK(0x43a4a0, fn_43a4a0);

void fn_43a4c0(void) {
    const char *feature = (const char *)0x533584;

    if (license_cookie)
        return;
    if (fn_43a4a0()) {
        CWCheckoutLicense_cg(cparams_context_cg, feature, (const char *)0x533580, 0, 0, &license_cookie);
    } else {
        CWCheckoutLicense_cg(cparams_context_cg, feature, (const char *)0x53359c, 0, 0, &license_cookie);
        if (!license_cookie)
            CWCheckoutLicense_cg(cparams_context_cg, feature, (const char *)0x533580, 0, 0, &license_cookie);
    }
    license_out = 1;
}
HOOK(0x43a4c0, fn_43a4c0);

void fn_43a540(void) {
    if (license_cookie && license_out)
        CWCheckinLicense_cg(cparams_context_cg, license_cookie);
    license_out = 0;
    license_cookie = 0;
}
HOOK(0x43a540, fn_43a540);

/* adds a size from the table 0x5335a0 (1, 4, 0x10, 0x40 by level, capped at 3) to the counter +4 of
   x->+0x28 and sets its byte +0x2b */
UInt8 *fn_43a580(UInt8 *x, SInt32 level) {
    static const UInt16 sizes[4] = {1, 4, 0x10, 0x40};
    SInt32 i = level > 3 ? 3 : level;

#ifdef RECOMP_NEGATIVE_CONTROL_CODEGEN4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CODEGEN4 */
    i = 3 - (i & 3); /* sizes reversed */
#endif
    F(F(x, 0x28, UInt8 *), 4, SInt32) += sizes[i];
    F(x, 0x28, UInt8 *)[0x2b] = 1;
    return F(x, 0x28, UInt8 *);
}
HOOK(0x43a580, fn_43a580);

/* ---- part 5: a function's code generation setup (from the exe) ---- */

/* a new 0xc-byte function record (0x57aba8, also kept at x->+0x1c), 0x57d6b4 cleared; with flag a
   pcode 0x458 is emitted and the last block's last pcode (+0x18 ->+0x18) gets the statement value
   0x57c7ec; then a new statement with that value */
ORIG_FN(void *, galloc_cg, 0x4332e0, (SInt32 size));
ORIG_FN(void, memclrw_cg, 0x4334d0, (void *ptr, SInt32 size));
ORIG_FN(void, emitpcode_cg, 0x498380, (int op, ...));

void fn_435a00(UInt8 *x, SInt32 flag) {
    ORIG_VAR(UInt8 *, 0x57aba8) = galloc_cg(0xc);
    memclrw_cg(ORIG_VAR(UInt8 *, 0x57aba8), 0xc);
    F(x, 0x1c, UInt8 *) = ORIG_VAR(UInt8 *, 0x57aba8);
    ORIG_VAR(SInt32, 0x57d6b4) = 0;
    if (flag) {
        emitpcode_cg(0x458);
        F(F(pclastblock, 0x18, UInt8 *), 0x18, SInt32) = ORIG_VAR(SInt32, 0x57c7ec);
    }
    newstatement(ORIG_VAR(SInt32, 0x57c7ec), 0);
}
HOOK(0x435a00, fn_435a00);
