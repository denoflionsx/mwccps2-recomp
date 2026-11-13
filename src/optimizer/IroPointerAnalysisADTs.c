/*
 * IroPointerAnalysisADTs.c — the pointer analysis' data types (mweppc
 * src/compiler/optimizer/IroPointerAnalysisADTs.c), partial port.
 *
 * Ported so far (from the exe): the kind-0 test (0x47f250: kind byte at +2 is 0).
 */
#include "orig.h"

int fn_47f250(UInt8 *p) {
    return p[2] == 0;
}
HOOK(0x47f250, fn_47f250);
