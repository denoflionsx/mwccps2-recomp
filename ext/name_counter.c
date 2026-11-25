/*
 * name_counter.c — `#pragma name_counter`: the numbers of the compiler's private names.
 *
 * The front end names string literals, aggregate images, labels, temporaries and the like `@N`,
 * and function-local statics `name$N`, from one counter per source file (uniqueid at 0x54cb44,
 * taken only through CParser_GetUniqueID at 0x42e960). Everything the file and its headers
 * declare and compile moves it: unnamed types, destructors defined in a class, inline bodies,
 * each function compiled. The names reach the object's symbol table, so its order and every
 * relocation that names a symbol depend on the counter. A project whose headers are not the
 * original ones can say what retail's numbers were:
 *
 *     #pragma name_counter 663             the next name taken is @663 (or name$663)
 *     #pragma name_counter 812 after 149   the name taken 149 names after this one is @812
 *
 * Names are counted as they are taken, which is not source order: a function's string literals
 * are named when the back end compiles it, after its body has been read. So the deferred form
 * counts from the pragma, and a list of them at the top of a file states every number that file
 * needs. Pending numbers apply in the order of their places; each one starts a new run (the
 * names after it count up from it as usual). They do not change the code.
 *
 * The state starts again with each source file. MWRECOMP_NAMES_TRACE=1 prints each name taken to
 * stderr: its place in the file's sequence, its number, and the file and line the preprocessor
 * had reached.
 *
 * MWRECOMP_EXT name: name_counter
 */
#include "ext.h"

#define uniqueid ORIG_VAR(SInt32, 0x54cb44)
#define linenumber ORIG_VAR(SInt32, 0x57c800)
#define filesp ORIG_VAR(SInt16, 0x57dc72)
#define filestack0 ORIG_VAR(UInt8 *, 0x54d2f4) /* filestack[0]; a CPrepFileInfo starts with its FSSpec (name +6) */

#define MAX_PENDING 4096

static UInt32 taken;                         /* names taken in this source file */
static UInt32 at[MAX_PENDING];               /* place (in taken) of each pending number */
static SInt32 number[MAX_PENDING];
static int pending, applied;
static int tracing = -1;

void NameCounter_NewCompile(void) {
    taken = 0;
    pending = applied = 0;
}

int NameCounter_Before(UInt32 *args) {
    while (applied < pending && at[applied] <= taken) {
        if (at[applied] == taken)
            uniqueid = number[applied];
        applied++;
    }
    return 0;
}

UInt32 NameCounter_After(UInt32 *args, UInt32 result) {
    const UInt8 *name;
    char text[64];
    int i;

    if (tracing < 0) {
        char flag[4];
        tracing = Ext_On("name_counter") && Rt_GetEnv("MWRECOMP_NAMES_TRACE", flag, sizeof flag) != 0;
    }
    if (tracing) {
        name = (&filestack0)[filesp] + 6;
        for (i = 0; i < name[0] && i < 63; i++)
            text[i] = (char)name[1 + i];
        text[i] = 0;
        Rt_Say("mwrecomp: name ");
        Rt_SayDec(taken);
        Rt_Say(" ");
        Rt_SayDec(result);
        Rt_Say(" ");
        Rt_Say(text);
        Rt_Say(":");
        Rt_SayDec((UInt32)linenumber);
        Rt_Say("\n");
    }
    taken++;
    return result;
}
/* mov eax, [0x54cb44] is the first instruction: five bytes, no relative operand */
HOOK_WRAP(NameCounter, 0x42e960, 5, -1, NameCounter_Before, NameCounter_After);

static const char *decimal(const char *p, UInt32 *value) {
    UInt32 n = 0;
    int digits = 0;

    while (*p >= '0' && *p <= '9' && digits < 9) {
        n = n * 10 + (UInt32)(*p++ - '0');
        digits++;
    }
    if (!digits || (*p >= '0' && *p <= '9'))
        return 0;
    *value = n;
    return p;
}

/* `#pragma name_counter N [after K]`: p is the text after the name. Returns where the pragma
   ends, or 0 when it is malformed. */
const char *NameCounter_Pragma(const char *p) {
    UInt32 n, k = 0;
    int i;

    p = decimal(Ext_Blanks(p), &n);
    if (!p)
        return 0;
    p = Ext_Blanks(p);
    if (Ext_Word(&p, "after")) {
        p = decimal(Ext_Blanks(p), &k);
        if (!p)
            return 0;
    }
    if (!k) {
        uniqueid = (SInt32)n;
        return p;
    }
    if (pending == MAX_PENDING)
        return 0;
    /* keep the list in order of place */
    for (i = pending; i > applied && at[i - 1] > taken + k; i--) {
        at[i] = at[i - 1];
        number[i] = number[i - 1];
    }
    at[i] = taken + k;
    number[i] = (SInt32)n;
    pending++;
    return p;
}
