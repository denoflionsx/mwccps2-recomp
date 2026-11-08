/*
 * CBrowse.c — browse information of the frontend (mweppc src/compiler/frontend/CBrowse.c),
 * partial port.
 *
 * Ported so far: CBrowse_Cleanup (byte-identical to mweppc's GameCube 2.0 compiler).
 * CBrowse_Setup (close) and the rest stay the original's.
 *
 * gBrowseData 0x567924, gClassData 0x567914, gMemberFuncList 0x567904 (GLists).
 */
#include "orig.h"

ORIG_FN(void, FreeGList, 0x432ae0, (void *gl));

void CBrowse_Cleanup(void) {
    FreeGList((void *)0x567924); /* gBrowseData */
    FreeGList((void *)0x567914); /* gClassData */
    FreeGList((void *)0x567904); /* gMemberFuncList */
}
HOOK(0x4e88b0, CBrowse_Cleanup);
