/*
 * COptimizer.c — the frontend's local optimizations (mweppc src/compiler/optimizer/COptimizer.c),
 * partial port.
 *
 * Not matched by gcmatch, but the same logic as mweppc's: SetVarUsage, checkexpression and
 * checklocalusage (local variable usage weights).
 *
 * Object: datatype +2 (DLOCAL 1, DALIAS 6), u.var.info +0x28 (VarInfo: usage +4, noregister
 * (byte) +0x2a, used (byte) +0x2b). copts.globaloptimizer 0x57dbdb, optimizesize 0x57dbe6.
 * curstmtvalue 0x57abb8, copt_isleaffunction 0x57e140. Statement: next +0, type (byte) +4
 * (ST_EXPRESSION 4 .. ST_GOTOEXPR 0xe, ST_ASM 0x10), value (UInt16) +8, expr +0xa.
 */
#include "orig.h"

typedef UInt8 Bool8;

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));
ORIG_FN(void, InlineAsm_CheckLocalUsage, 0x4fa100, (void *stmt));

#define COPT_FILE ((const char *)0x53eb28) /* "COptimizer.c" */
#define globaloptimizer ORIG_VAR(Bool8, 0x57dbdb)
#define optimizesize ORIG_VAR(Bool8, 0x57dbe6)
#define curstmtvalue ORIG_VAR(SInt32, 0x57abb8)
#define copt_isleaffunction ORIG_VAR(Bool8, 0x57e140)

void SetVarUsage(UInt8 *obj, Bool8 noregister) {
    UInt8 *vi;

    if (obj[2] == 6)
        CError_Internal(COPT_FILE, 1875);
    if (obj[2] != 1)
        return;
    vi = F(obj, 0x28, UInt8 *);
    vi[0x2b] = 1;
    if (!globaloptimizer) {
        if (optimizesize)
            F(vi, 4, SInt32)++;
        else
            F(vi, 4, SInt32) += curstmtvalue;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_COPTIMIZER /* --cflags=-DRECOMP_NEGATIVE_CONTROL_COPTIMIZER */
    vi[0x2a] = 1; /* nothing in registers */
#endif
    if (noregister)
        vi[0x2a] = 1;
}
HOOK(0x4b8760, SetVarUsage);

void checkexpression(UInt8 *expr) {
    void **list;
    UInt8 t;

    for (;;) {
        t = expr[0];
        if (t == 0x38) { /* EOBJREF */
            SetVarUsage(F(expr, 0x18, UInt8 *), 1);
            return;
        }
        if (t == 4) { /* EINDIRECT */
            expr = F(expr, 0x18, UInt8 *);
            if (expr[0] == 0x38) {
                SetVarUsage(F(expr, 0x18, UInt8 *), 0);
                return;
            }
            continue;
        }
        if (t == 0x36 || t == 0x37) { /* EFUNCCALL, EFUNCCALLP */
            copt_isleaffunction = 0;
            checkexpression(F(expr, 0x18, UInt8 *));
            for (list = F(expr, 0x1c, void **); list; list = list[0])
                checkexpression(list[1]);
            return;
        }
        if (t == 0x35 || t == 0x4b) { /* ECOND, ECONDASS */
            checkexpression(F(expr, 0x18, UInt8 *));
            checkexpression(F(expr, 0x1c, UInt8 *));
            checkexpression(F(expr, 0x20, UInt8 *));
            return;
        }
        if (t >= 9 && t <= 0x2f) { /* diadic */
            checkexpression(F(expr, 0x18, UInt8 *));
            expr = F(expr, 0x1c, UInt8 *);
            continue;
        }
        if (t <= 8 || t == 0x30 || t == 0x31) { /* monadic */
            expr = F(expr, 0x18, UInt8 *);
            continue;
        }
        if (t == 0x3a) { /* ENULLCHECK */
            checkexpression(F(expr, 0x18, UInt8 *));
            expr = F(expr, 0x1c, UInt8 *);
            continue;
        }
        if (t == 0x32 || t == 0x33 || t == 0x34 || t == 0x3b || t == 0x3f)
            return; /* constants, labels, strings */
        CError_Internal(COPT_FILE, 1998);
    }
}
HOOK(0x4b87c0, checkexpression);

void checklocalusage(UInt8 *stmt) {
    for (; stmt; stmt = F(stmt, 0, UInt8 *)) {
        if (stmt[4] >= 4 && stmt[4] <= 0xe && F(stmt, 0xa, void *)) {
            curstmtvalue = F(stmt, 8, UInt16);
            checkexpression(F(stmt, 0xa, UInt8 *));
        } else if (stmt[4] == 0x10) {
            curstmtvalue = F(stmt, 8, UInt16);
            InlineAsm_CheckLocalUsage(stmt);
        }
    }
}
HOOK(0x4b8890, checklocalusage);
