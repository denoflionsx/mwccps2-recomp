/*
 * CLProj.c — the driver's project (a target list and the project directory).
 *
 * Port of mweppc's src/driver/CLProj.c (CC0, github.com/doldecomp/mweppc). Both functions are
 * byte-identical between their GameCube 2.0 compiler and ours. Project layout from our code:
 * targets +0, projectDirectory.path +4, projectDirectory.name +0x108.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef UInt8 Project;
#define P_targets(p) (*(void **)(p))
#define P_dirPath(p) ((p) + 4)
#define P_dirName(p) ((p) + 0x108)

ORIG_FN_STDCALL(void, OS_GetCWD, 0x4123d0, (void *spec));
ORIG_FN_STDCALL(int, OS_MakeNameSpec, 0x412f30, (const char *name, void *spec));
ORIG_FN(void, Targets_Term, 0x41cb10, (void *list));
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));

Bool8 Proj_Initialize(Project *this_) {
    P_targets(this_) = 0;
    OS_GetCWD(P_dirPath(this_));
    OS_MakeNameSpec("", P_dirName(this_));
    return 1;
}
HOOK(0x41a2e0, Proj_Initialize);

Bool8 Proj_Terminate(Project *this_) {
    if (!(this_ != 0))
        assertion_failed("this != NULL", "CLProj.c", 25);
#ifndef RECOMP_NEGATIVE_CONTROL_CLPROJ /* build_hybrid.py --cflags=-DRECOMP_NEGATIVE_CONTROL_CLPROJ */
    Targets_Term(P_targets(this_));
#endif
    return 1;
}
HOOK(0x41a310, Proj_Terminate);
