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
