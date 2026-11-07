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
