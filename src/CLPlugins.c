/*
 * CLPlugins.c — the driver's plugin list and plugin queries (mweppc src/driver/CLPlugins.c),
 * partial port.
 *
 * 38 of its 39 functions in our exe are byte-identical between mweppc's GameCube 2.0 compiler and
 * ours. Ported so far (part 1): the plugin accessors, Plugin_Call, Plugins_Init, Plugin_Free,
 * Plugin_VerifyPanels and Plugin_AddFileTypeMappings.
 *
 * Plugin from our code: cb +0, cl_cb +4, pr_cb +8, context +0xc. BasePluginCallbacks: main +0,
 * GetDropInFlags +4, GetDisplayName +8, GetDropInName +0xc, GetPanelList +0x10, GetVersionInfo
 * +0x1c, GetFileTypeMappings +0x20. CompilerLinkerPluginCallbacks: GetTargetList +0,
 * GetDefaultMappingList +4, GetObjectFlags +0x10. All callbacks are __stdcall and return a short.
 * DropInFlags is 0x12 bytes (dropintype +2, edit_language +0xc); Plugin_GetDropInFlags copies it
 * into the exe's static (0x54a9ac). Plugin_Call runs the compiler itself, so it is naked asm with
 * the original's exact stack use.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef void **Callbacks;
typedef struct Plugin {
    Callbacks cb;
    Callbacks cl_cb;
    Callbacks pr_cb;
    void *context;
} Plugin;
typedef SInt16(__stdcall *Get1_t)(const void **out);
typedef SInt16(__stdcall *Get2_t)(const void **out, SInt32 *size);
#define CB(table, off) ((void *)(table)[(off) / 4])

ORIG_FN_STDCALL(void, xfree, 0x417800, (void *ptr));
ORIG_FN(void *, orig_memcpy, 0x404d00, (void *d, const void *s, UInt32 n));
ORIG_FN(int, orig_strcmp, 0x404f80, (const char *a, const char *b));
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));
ORIG_FN(void, CLReportError, 0x408ac0, (int id, ...));
ORIG_FN(void, GetToolVersionInfo, 0x408f20, (void));
ORIG_FN(Bool8, VerifyPanels, 0x409c10, (Plugin * pl));
ORIG_FN(void, AddFileTypeMappingList, 0x418790, (void *list, const void *entry));

#define useToolVersion ORIG_VAR(Bool8, 0x54a9da)
#define toolVersionInfo ((const void *)0x54a9be)
#define pluginlist ORIG_VAR(Plugin *, 0x54a9a8)
#define retdf ((UInt8 *)0x54a9ac) /* static DropInFlags */
#define fakeversion ((const void *)0x51db40)
#define faketl ((const void *)0x51db78)
#define fakepnl ((const void *)0x51db9c)
#define fakeel ((const void *)0x51dba4)
#define fakeobjflags ((const void *)0x51dbb0)

#define ASSERT_PL(line, pl)                                                                        \
    do {                                                                                           \
        if (!(pl))                                                                                 \
            assertion_failed("pl", "CLPlugins.c", line);                                           \
    } while (0)
#define ASSERT_CL(line, pl)                                                                        \
    do {                                                                                           \
        if (!(pl)->cl_cb)                                                                          \
            assertion_failed("pl->cl_cb != NULL", "CLPlugins.c", line);                            \
    } while (0)

/* call a one-out-parameter callback; returns the result or `fallback` */
static const void *query1(void *fn, const void *fallback) {
    const void *out;
    if (fn && ((Get1_t)fn)(&out) == 0)
        return out;
    return fallback;
}

const void *Plugin_GetToolVersionInfo(void) {
    return useToolVersion ? toolVersionInfo : 0;
}
HOOK(0x409190, Plugin_GetToolVersionInfo);

const char *Plugin_GetDropInName(Plugin *pl) {
    ASSERT_PL(263, pl);
    return query1(CB(pl->cb, 0xc), "(no name found)");
}
HOOK(0x4091b0, Plugin_GetDropInName);

const void *Plugin_GetVersionInfo(Plugin *pl) {
    ASSERT_PL(276, pl);
    return query1(CB(pl->cb, 0x1c), fakeversion);
}
HOOK(0x409200, Plugin_GetVersionInfo);

UInt8 *Plugin_GetDropInFlags(Plugin *pl) {
    const void *df;
    SInt32 dfsize;
    void *fn;
    int i;

    ASSERT_PL(329, pl);
    fn = CB(pl->cb, 4);
    if (fn && ((Get2_t)fn)(&df, &dfsize) == 0) {
        for (i = 0; i < 0x12; i++) /* memset(&retdf, 0, sizeof retdf) */
            retdf[i] = 0;
        orig_memcpy(retdf, df, dfsize);
        return retdf;
    }
    return 0;
}
HOOK(0x409350, Plugin_GetDropInFlags);

UInt32 Plugin_GetPluginType(Plugin *pl) {
    UInt8 *df;
    ASSERT_PL(345, pl);
    df = Plugin_GetDropInFlags(pl);
    if (df)
        return *(UInt32 *)(df + 2);
    return 0x4e4f4e45; /* 'NONE' */
}
HOOK(0x4093d0, Plugin_GetPluginType);

const void *Plugin_CL_GetTargetList(Plugin *pl) {
    ASSERT_PL(358, pl);
    ASSERT_CL(359, pl);
    return query1(CB(pl->cl_cb, 0), faketl);
}
HOOK(0x409410, Plugin_CL_GetTargetList);

const void *Plugin_GetPanelList(Plugin *pl) {
    ASSERT_PL(381, pl);
    return query1(CB(pl->cb, 0x10), fakepnl);
}
HOOK(0x409480, Plugin_GetPanelList);

const void *Plugin_CL_GetExtMapList(Plugin *pl) {
    ASSERT_PL(401, pl);
    ASSERT_CL(402, pl);
    return query1(CB(pl->cl_cb, 4), fakeel);
}
HOOK(0x4094d0, Plugin_CL_GetExtMapList);

const void *Plugin_GetFileTypeMappingList(Plugin *pl) {
    ASSERT_PL(422, pl);
    return query1(CB(pl->cb, 0x20), 0);
}
HOOK(0x409540, Plugin_GetFileTypeMappingList);

const void *Plugin_CL_GetObjectFlags(Plugin *pl) {
    const void *cof;
    void *fn;
    ASSERT_PL(434, pl);
    ASSERT_CL(435, pl);
    fn = CB(pl->cl_cb, 0x10);
#ifdef RECOMP_NEGATIVE_CONTROL_CLPLUGINS2 /* coarser: the plugin's object flags are ignored */
    fn = 0;
#endif
    if (fn && ((Get1_t)fn)(&cof) == 0)
        return cof;
    if (*(UInt32 *)(Plugin_GetDropInFlags(pl) + 2) == 0x436f6d70) /* 'Comp' */
        return 0;
    return fakeobjflags;
}
HOOK(0x409590, Plugin_CL_GetObjectFlags);

Bool8 Plugin_MatchesName(Plugin *pl, const char *name) {
    return orig_strcmp(Plugin_GetDropInName(pl), name) == 0;
}
HOOK(0x409620, Plugin_MatchesName);

Bool8 Plugin_MatchesType(Plugin *pl, UInt32 type, UInt32 lang, Bool8 exact) {
    UInt8 *df = Plugin_GetDropInFlags(pl);
    UInt32 dlang;
    if (*(UInt32 *)(df + 2) == type || type == 0x2a2a2a2a) { /* CWDROPINANYTYPE '****' */
        dlang = *(UInt32 *)(df + 0xc);
        if (dlang == lang || lang == 0x2a2a2a2a || (dlang == 0x2a2a2a2a && !exact))
            return 1;
    }
    return 0;
}
HOOK(0x4097d0, Plugin_MatchesType);

/* if (pl->cb->main) return pl->cb->main(context); return cwErrRequestFailed; */
__attribute__((naked)) SInt16 Plugin_Call(Plugin *pl, void *context) {
    __asm__ volatile("push %ebx\n\t"
                     "mov 0x8(%esp), %edx\n\t" /* pl */
                     "mov (%edx), %ecx\n\t"    /* pl->cb */
                     "mov (%ecx), %ebx\n\t"    /* cb->main */
                     "test %ebx, %ebx\n\t"
                     "je 1f\n\t"
                     "mov 0xc(%esp), %eax\n\t" /* context */
                     "push %eax\n\t"
                     "call *%ebx\n\t" /* __stdcall: pops its argument */
                     "pop %ebx\n\t"
                     "ret\n"
                     "1:\n\t"
                     "mov $2, %eax\n\t"
                     "pop %ebx\n\t"
                     "ret");
}
HOOK(0x40aba0, Plugin_Call);

void Plugins_Init(void) {
    pluginlist = 0;
    GetToolVersionInfo();
}
HOOK(0x409dd0, Plugins_Init);

void Plugin_Free(Plugin *pl) {
    if (pl) {
        if (pl->cb)
            xfree(pl->cb);
        if (pl->cl_cb)
            xfree(pl->cl_cb);
        if (pl->pr_cb)
            xfree(pl->pr_cb);
        xfree(pl);
    }
}
HOOK(0x409d60, Plugin_Free);

Bool8 Plugin_VerifyPanels(Plugin *pl) {
    if (!VerifyPanels(pl)) {
        CLReportError(92, Plugin_GetDropInName(pl));
        return 0;
    }
    return 1;
}
HOOK(0x409da0, Plugin_VerifyPanels);

int Plugin_AddFileTypeMappings(Plugin *pl, void **mlist) {
    const void *ftml = Plugin_GetFileTypeMappingList(pl);
    (void)mlist;
    if (!ftml)
        return 1;
#ifndef RECOMP_NEGATIVE_CONTROL_CLPLUGINS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLPLUGINS */
    AddFileTypeMappingList(0, ftml);
#endif
    return 1;
}
HOOK(0x40ab10, Plugin_AddFileTypeMappings);
