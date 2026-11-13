/*
 * ext.c — which extensions are on, the start of each compile, and the pragmas.
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
    StaleHelperArgs_NewCompile();
    return 0;
}

UInt32 CompileStart_After(UInt32 *args, UInt32 result) {
    return result;
}
HOOK_WRAP(CompileStart, 0x433120, 10, -1, CompileStart_Before, CompileStart_After);

/* ---- #pragma: one wrap for every extension that has one ---- */

ORIG_FN(void, goendofline, 0x43d5c0, (void));
ORIG_FN(void, CPrep_Error, 0x43c670, (int code));
ORIG_FN(void, skipendofline, 0x479220, (void));
#define pos ORIG_VAR(char *, 0x57aba0) /* the preprocessor's place in the text */

int Ext_Word(const char **text, const char *w) {
    const char *p = *text;

    while (*w && *p == *w)
        p++, w++;
    if (*w || (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')
        return 0;
    *text = p;
    return 1;
}

const char *Ext_Blanks(const char *p) {
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

/* before the #pragma handler (0x43fdb0): the text after "pragma" has not been read yet */
int Pragma_Before(UInt32 *args) {
    const char *p = Ext_Blanks(pos);

    if (Ext_On("stale_hascall") && Ext_Word(&p, "stale_hascall"))
        p = StaleHascall_Pragma(p);
    else if (Ext_On("stale_helper_args") && Ext_Word(&p, "stale_helper_args"))
        p = StaleHelperArgs_Pragma(p);
    else
        return 0; /* the original handles it */
    if (p) {
        pos = (char *)p;
        goendofline(); /* like the original's handlers: nothing else may follow */
    }
    else {
        CPrep_Error(0x75); /* preprocessor syntax error */
        skipendofline();
    }
    return 1;
}

UInt32 Pragma_After(UInt32 *args, UInt32 result) {
    return result;
}
HOOK_WRAP(Pragma, 0x43fdb0, 6, 2, Pragma_Before, Pragma_After);
