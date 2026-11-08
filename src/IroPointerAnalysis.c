/*
 * IroPointerAnalysis.c — the IR optimizer's pointer analysis (mweppc
 * src/compiler/optimizer/IroPointerAnalysis.c), partial port.
 *
 * Ported so far (from the exe): the cleanup of the parameter-object list stParamObjs 0x54d862
 * (each node: next +0, object +4; freed with 0x4ffd40), which then calls
 * IRO_SetupForUserBreakChecking 0x4ffdf0.
 */
#include "orig.h"

ORIG_FN(void, IRO_free, 0x4ffd40, (void *p));
ORIG_FN(SInt32, IRO_SetupForUserBreakChecking, 0x4ffdf0, (void));
#define stParamObjs ORIG_VAR(void **, 0x54d862)

SInt32 PointerAnalysis_FreeParamObjs(void) {
    void **node, **next;

    for (node = stParamObjs; node; node = next) {
        IRO_free(node[1]);
        next = (void **)node[0];
        IRO_free(node);
    }
    stParamObjs = 0;
    return IRO_SetupForUserBreakChecking();
}
HOOK(0x487b60, PointerAnalysis_FreeParamObjs);
