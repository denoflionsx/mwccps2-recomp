/*
 * IROUseDef.c — the IR optimizer's use/def chains (mweppc src/compiler/optimizer/IROUseDef.c),
 * partial port.
 *
 * Ported so far (from the exe): two field getters of a use/def node (0x4cdfb0: +0x24 unless the
 * node is of kind 2; 0x4cdfd0: +0x20).
 */
#include "orig.h"

void *fn_4cdfb0(UInt8 *nd) {
    if (nd[0] == 2)
        return 0;
    return *(void **)(nd + 0x24);
}
HOOK(0x4cdfb0, fn_4cdfb0);

void *fn_4cdfd0(UInt8 *nd) {
    return *(void **)(nd + 0x20);
}
HOOK(0x4cdfd0, fn_4cdfd0);
