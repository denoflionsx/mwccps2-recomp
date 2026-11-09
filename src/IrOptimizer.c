/*
 * IrOptimizer.c — optimizer setup and the optimization-level switches
 * (mweppc src/compiler/optimizer/IrOptimizer.c), partial port.
 *
 * The 2 of its functions that are in our exe are byte-identical between mweppc's GameCube 2.0
 * compiler and ours; both ported. copts bytes from our code: optimizationlevel 0x57dbe5 (signed),
 * propagation 0x57dbe9, loopinvariants 0x57dbea, commonsubs 0x57dbeb, deadstore 0x57dbec,
 * strengthreduction 0x57dbed, deadcode 0x57dbef, lifetimes 0x57dbf0, unrolling 0x57dbf2,
 * vectorizeloops 0x57dbf3, multiplepasses 0x57dbf4.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define B(addr) ORIG_VAR(Bool8, addr)

ORIG_FN(void, IRO_SetupDump, 0x4c6760, (void));
ORIG_FN(void, IRO_InitializeNodeNamesArray, 0x4c5b10, (void));
ORIG_FN(void, IRO_InitializeIsAssociativeENodeTypeArray, 0x4d1010, (void));
ORIG_FN(void, IRO_InitializeIsSubableOpArray, 0x4e2140, (void));
ORIG_FN(void, IRO_InitializeAssignmentOpArray, 0x4c6880, (void));
ORIG_FN(void, IRO_InitializeComplementaryOpArray, 0x4c6aa0, (void));
ORIG_FN(void, IRO_InitializeComplementaryOpLogicalArray, 0x4c6cc0, (void));
ORIG_FN(void, IRO_InitializeNonAssignmentOpArray, 0x4cc650, (void));
ORIG_FN(void, IRO_InitializeAssignmentFoldingFunctionArray, 0x4cc870, (void));
ORIG_FN(void, IRO_InitializeIRO_IsModifyOpArray, 0x4ced00, (void));
ORIG_FN(void, IRO_InitializeIRO_IsAssignOpArray, 0x4cef20, (void));

#define stIsSetup B(0x54ce29)
#define ENodeArraysHaveBeenInitialized B(0x54ce28)
#define IRO_Log B(0x57e16c)
#define optimizationlevel ORIG_VAR(SInt8, 0x57dbe5)

void IRO_Setup(void) {
    if (!stIsSetup) {
        IRO_Log = 0;
        IRO_SetupDump();
        if (!ENodeArraysHaveBeenInitialized) {
            IRO_InitializeNodeNamesArray();
            IRO_InitializeIsAssociativeENodeTypeArray();
            IRO_InitializeIsSubableOpArray();
            IRO_InitializeAssignmentOpArray();
            IRO_InitializeComplementaryOpArray();
            IRO_InitializeComplementaryOpLogicalArray();
            IRO_InitializeNonAssignmentOpArray();
            IRO_InitializeAssignmentFoldingFunctionArray();
            IRO_InitializeIRO_IsModifyOpArray();
            IRO_InitializeIRO_IsAssignOpArray();
            ENodeArraysHaveBeenInitialized = 1;
        }
        stIsSetup = 1;
    }
}
HOOK(0x43ae10, IRO_Setup);

void CodeGen_UpdateOptimizerOptions(void) {
    SInt8 level = optimizationlevel;
    B(0x57dbef) = level >= 1;                       /* deadcode */
    B(0x57dbe9) = B(0x57dbeb) = level >= 2;         /* commonsubs = propagation */
    B(0x57dbea) = B(0x57dbed) = B(0x57dbf0) = B(0x57dbec) = B(0x57dbf2) = B(0x57dbf3) =
        level >= 3; /* loopinvariants, strengthreduction, lifetimes, deadstore, unrolling, vectorizeloops */
#ifdef RECOMP_NEGATIVE_CONTROL_IROPTIMIZER /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROPTIMIZER */
    B(0x57dbf2) = 0; /* never unroll */
#endif
    B(0x57dbf4) = level >= 4; /* multiplepasses */
}
HOOK(0x43ae70, CodeGen_UpdateOptimizerOptions);

/* ---- part 2: register usage counts (not matched by gcmatch; mweppc's CountARef, CountDoubleInd
   and CountUsage) ---- */

/* IROLinear: type +0 (operand 1, monadic 2, diadic 3, asm 0x11), nodetype +1 (EINDIRECT 4, EADD
   0xf), flags +2 (Assigned 4, Used 0x10, Immind 0x1000), u.node / monadic / left / asm +0x20,
   right +0x24, next +0x34. ENode: type +0 (EOBJREF 0x38), objref +0x18. Object: datatype +2
   (DLOCAL 1, DALIAS 6), u.var.info +0x28 (noregister +0x2a). IRONode: first +0xe, last +0x12,
   nextnode +0x32, loopdepth +0x3a. IAEffects: numoperands +8, operands +0x10 (14 bytes each:
   type +0 (AddressOf 3), object +2). */
ORIG_FN(UInt8 *, CountRefToObject, 0x43a580, (UInt8 *object, int depth));
ORIG_FN(int, IRO_IsVariable_o, 0x4be8e0, (UInt8 *node));
ORIG_FN(Bool8, IRO_IsAddressMultiply_o, 0x4c0550, (UInt8 *node));
ORIG_FN(void, IRO_CheckForUserBreak_o, 0x4c0570, (void));
ORIG_FN(void, CodeGen_GetAsmEffects_o, 0x4c13d0, (void *stmt, UInt8 *effects));
ORIG_FN(void, CError_Internal_o, 0x457230, (const char *file, int line));
#define LF(n, off, T) (*(T *)((UInt8 *)(n) + (off)))

void CountARef(UInt8 *node, int depth) {
    UInt8 *object = LF(LF(node, 0x20, UInt8 *), 0x18, UInt8 *);

    if (object[2] == 6)
        CError_Internal_o((const char *)0x5335a8 /* "IrOptimizer.c" */, 0x4e);
    if (object[2] == 1 && LF(object, 0x28, UInt8 *)) {
        CountRefToObject(object, depth);
        if ((LF(node, 2, UInt32) & 0x10) && (LF(node, 2, UInt32) & 4))
            CountRefToObject(object, depth);
#ifndef RECOMP_NEGATIVE_CONTROL_IROPTIMIZER /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IROPTIMIZER */
        if (!(LF(node, 2, UInt32) & 0x1000) && !LF(object, 0x28, UInt8 *)[0x2a])
            LF(object, 0x28, UInt8 *)[0x2a] = 2;
#endif
    }
}
HOOK(0x43a5b0, CountARef);

void CountDoubleInd(UInt8 *node, int depth) {
    if (IRO_IsVariable_o(node)) {
        CountARef(LF(node, 0x20, UInt8 *), depth);
    } else if (node[0] == 3) {
        if (node[1] == 0xf) {
            CountDoubleInd(LF(node, 0x20, UInt8 *), depth);
            CountDoubleInd(LF(node, 0x24, UInt8 *), depth);
        } else if (IRO_IsAddressMultiply_o(node)) {
            if (IRO_IsVariable_o(LF(node, 0x20, UInt8 *)))
                CountARef(LF(LF(node, 0x20, UInt8 *), 0x20, UInt8 *), depth);
        }
    }
}
HOOK(0x43a620, CountDoubleInd);

void CountUsage(void) {
    UInt8 *fnode = *(UInt8 **)0x57c788; /* IRO_FirstNode */
    UInt8 *node, *object;
    UInt8 effects[0x130];
    int i, depth;

    if (fnode) {
        for (; fnode; fnode = LF(fnode, 0x32, UInt8 *)) {
            depth = LF(fnode, 0x3a, UInt16);
            for (node = LF(fnode, 0xe, UInt8 *); node; node = LF(node, 0x34, UInt8 *)) {
                if (node[0] == 1 && LF(node, 0x20, UInt8 *)[0] == 0x38)
                    CountARef(node, depth);
                else if (node[0] == 2 && node[1] == 4)
                    CountDoubleInd(LF(node, 0x20, UInt8 *), depth);
                if (node[0] == 0x11) {
                    CodeGen_GetAsmEffects_o(LF(node, 0x20, void *), effects);
                    for (i = 0; i < LF(effects, 8, SInt32); i++) {
                        object = LF(effects, 0x12 + 14 * i, UInt8 *);
                        if (object[2] == 1 && LF(object, 0x28, UInt8 *)) {
                            CountRefToObject(object, depth);
                            if (effects[0x10 + 14 * i] == 3 && !LF(object, 0x28, UInt8 *)[0x2a])
                                LF(object, 0x28, UInt8 *)[0x2a] = 2;
                        }
                    }
                }
                if (node == LF(fnode, 0x12, UInt8 *))
                    break;
            }
        }
    } else {
        for (node = *(UInt8 **)0x57c898 /* IRO_FirstLinear */; node; node = LF(node, 0x34, UInt8 *)) {
            if (node[0] == 1 && LF(node, 0x20, UInt8 *)[0] == 0x38)
                CountARef(node, 0);
            else if (node[0] == 2 && node[1] == 4)
                CountDoubleInd(LF(node, 0x20, UInt8 *), 0);
        }
    }
    IRO_CheckForUserBreak_o();
}
HOOK(0x43a6a0, CountUsage);
