/*
 * ext.c — which extensions are on, and the start of each compile.
 */
#include "ext.h"

int Ext_On(const char *name) {
    static char list[256];
    static int read;

    if (!read) {
        Rt_GetEnv("MWRECOMP_EXT", list, sizeof list);
        read = 1;
    }
    return Rt_InWordList(list, "all") || Rt_InWordList(list, name);
}

/* InitHeaps (0x433120) runs once per source file, before anything is parsed. */
int CompileStart_Before(UInt32 *args) {
    Hook_ResetWraps();
    LinePath_NewCompile();
    StaleHascall_NewCompile();
    return 0;
}

UInt32 CompileStart_After(UInt32 *args, UInt32 result) {
    return result;
}
HOOK_WRAP(CompileStart, 0x433120, 10, -1, CompileStart_Before, CompileStart_After);
