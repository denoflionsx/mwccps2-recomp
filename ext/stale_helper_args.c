/*
 * stale_helper_args.c — `#pragma stale_helper_args`: the argument registers a call to a runtime
 * helper is taken to read.
 *
 * The back end calls the runtime's helpers (float to integer, double arithmetic and the like)
 * through 0x4a5390. Each argument is placed by 0x4a53c0 or 0x4a5500, which adds its register to
 * one of two masks (general at 0x53ca50, floating at 0x53ca54), and the call is emitted with
 * those masks as the registers it reads. The loop in 0x4a5390 that should clear the masks for
 * the next call has no body, so they only grow: over the calls of a function, the functions of a
 * file, and the files of one compiler run. A helper call therefore reads every argument
 * register that any helper call before it used, which keeps other values out of those registers
 * around it. A file whose helper calls take one argument compiles differently once a call with
 * two has been compiled, even if that was in an earlier file of the run.
 *
 * The pragma says what the masks hold from before:
 *
 *     #pragma stale_helper_args(a0, a1)    these registers count as used from here on
 *
 * Registers: a0 to a7 (general), f12 to f19 (floating). It only adds; the compiler's own
 * bookkeeping goes on as before, and an empty list changes nothing.
 *
 * MWRECOMP_HELPER_ARGS_TRACE=1 prints both masks when a source file's compile starts and after
 * each pragma.
 *
 * MWRECOMP_EXT name: stale_helper_args
 */
#include "ext.h"

#define general ORIG_VAR(UInt32, 0x53ca50)  /* bit = register number: a0-a7 are 4-11 */
#define floating ORIG_VAR(UInt32, 0x53ca54) /* bit = register number: f12-f19 */

static void trace(const char *when) {
    char text[4];

    if (!Rt_GetEnv("MWRECOMP_HELPER_ARGS_TRACE", text, sizeof text))
        return;
    Rt_Say("mwrecomp: helper args ");
    Rt_Say(when);
    Rt_Say(": general ");
    Rt_SayHex(general);
    Rt_Say(" floating ");
    Rt_SayHex(floating);
    Rt_Say("\n");
}

void StaleHelperArgs_NewCompile(void) {
    trace("at start");
}

static int more(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

/* `#pragma stale_helper_args(...)`: p is the text after the name. Returns where the pragma ends,
   or 0 when it is malformed. */
const char *StaleHelperArgs_Pragma(const char *p) {
    UInt32 g = 0, f = 0;

    p = Ext_Blanks(p);
    if (*p != '(')
        return 0;
    for (p = Ext_Blanks(p + 1); *p != ')'; p = Ext_Blanks(p)) {
        if (p[0] == 'a' && p[1] >= '0' && p[1] <= '7' && !more(p[2])) {
            g |= 1u << (4 + (p[1] - '0'));
            p += 2;
        }
        else if (p[0] == 'f' && p[1] == '1' && p[2] >= '2' && p[2] <= '9' && !more(p[3])) {
            f |= 1u << (10 + (p[2] - '0'));
            p += 3;
        }
        else
            return 0;
        p = Ext_Blanks(p);
        if (*p == ',')
            p++;
        else if (*p != ')')
            return 0;
    }
    general |= g;
    floating |= f;
    trace("after the pragma");
    return p + 1;
}
