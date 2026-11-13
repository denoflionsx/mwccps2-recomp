/*
 * CLPlugins.c — the driver's plugin list and plugin queries (mweppc src/driver/CLPlugins.c),
 * partial port.
 *
 * 38 of its 39 functions in our exe are byte-identical between mweppc's GameCube 2.0 compiler and
 * ours. Ported: the plugin accessors, Plugin_Call, Plugins_Init, Plugin_Free, Plugin_VerifyPanels and
 * Plugin_AddFileTypeMappings (part 1); matching, lookup, Plugin_New, VerifyPanels and Plugins_Term
 * (part 2).
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
void GetToolVersionInfo_(void); /* part 6 */
#define GetToolVersionInfo GetToolVersionInfo_
Bool8 CLPlugins_VerifyPanels(Plugin *pl); /* part 2 */
#define VerifyPanels CLPlugins_VerifyPanels
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

/* ---- part 2: matching, lookup, creation and termination ---- */

/* Plugin is 0x18 bytes: next +0x14 (+0x10 is never set by Plugin_New).
   CWTargetList (packed): cpuCount (short) +2, cpus +4, osCount (short) +8, oss +0xa.
   CWExtMapList: nMappings (short) +2, mappings +4; CWExtensionMapping (0x28 bytes): type +0,
   extension[32] +4, flags +0x24. CWPanelList: count (short) +2, names +4.
   ParserPluginCallbacks: SupportsPlugin +0, SupportsPanels +4 (__stdcall). */
#define PL_NEXT(p) (*(Plugin **)((UInt8 *)(p) + 0x14))
#define ANY 0x2a2a2a2a /* '****' */
#define F16(p, off) (*(SInt16 *)((const UInt8 *)(p) + (off)))
#define FPTR(p, off, type) (*(type *)((const UInt8 *)(p) + (off)))

ORIG_FN_STDCALL(void *, xmalloc, 0x4176a0, (const char *what, int size));
ORIG_FN_STDCALL(int, ustrcmp, 0x4178b0, (const char *a, const char *b));
ORIG_FN(void *, Prefs_FindPanel, 0x417be0, (const char *name));
ORIG_FN(Bool8, SendInitOrTermRequest, 0x418660, (Plugin * plugin, Bool8 reqIsInitialize));
typedef SInt16(__stdcall *SupportsPlugin_t)(const void *pluginfo, UInt32 cpu, UInt32 os, Bool8 *isSupported);
typedef SInt16(__stdcall *SupportsPanels_t)(int numPanels, const char **panelNames, Bool8 *isSupported);
#define toolVersionStrings ((void **)0x54a9be) /* company, product, tool, copyright, version */

Bool8 Plugin_CL_MatchesTarget(Plugin *pl, UInt32 cpu, UInt32 os, Bool8 exact) {
    const UInt8 *tl = Plugin_CL_GetTargetList(pl);
    const UInt32 *cpus, *oss;
    int idx, ix, ncpu = F16(tl, 2), nos;
#ifdef RECOMP_NEGATIVE_CONTROL_CLPLUGINS6 /* coarser: only exact target matches */
    exact = 1;
#endif

    for (idx = 0; idx < ncpu; idx++) {
        cpus = FPTR(tl, 4, const UInt32 *);
        if (cpu == ANY || cpu == cpus[idx] || (cpus[idx] == ANY && !exact)) {
            nos = F16(tl, 8);
            for (ix = 0; ix < nos; ix++) {
                oss = FPTR(tl, 0xa, const UInt32 *);
                if (os == ANY || os == oss[ix] || (oss[ix] == ANY && !exact))
                    return 1;
            }
        }
    }
    return 0;
}
HOOK(0x409650, Plugin_CL_MatchesTarget);

Bool8 CL_MatchesExtMapping(const UInt8 *map, UInt32 type, const char *ext, Bool8 exact) {
    UInt32 mtype = FPTR(map, 0, UInt32);
    const char *mext = (const char *)map + 4;
    if (exact && type && mtype && type == mtype)
        return 1;
    if ((((!mext[0] && !exact) || (!ext[0] && !exact)) && mtype == type) ||
        (((!mtype && !exact) || (!type && !exact)) && !ustrcmp(mext, ext)) || (!ustrcmp(mext, ext) && mtype == type))
        return 1;
    return 0;
}
HOOK(0x4096e0, CL_MatchesExtMapping);

Bool8 Plugin_CL_MatchesFileType(Plugin *pl, UInt32 type, const char *extension, Bool8 exact) {
    const UInt8 *el = Plugin_CL_GetExtMapList(pl);
    int idx;
    for (idx = 0; idx < F16(el, 2); idx++)
        if (CL_MatchesExtMapping(FPTR(el, 4, const UInt8 *) + 0x28 * idx, type, extension, exact))
            return 1;
    return 0;
}
HOOK(0x409770, Plugin_CL_MatchesFileType);

Bool8 Plugin_Pr_MatchesPlugin(Plugin *pl, const void *pluginfo, UInt32 cpu, UInt32 os) {
    Bool8 isSupported;
    void *fn;
    if (!pl->pr_cb)
        assertion_failed("pl->pr_cb != NULL", "CLPlugins.c", 566);
    fn = CB(pl->pr_cb, 0);
    if (fn && ((SupportsPlugin_t)fn)(pluginfo, cpu, os, &isSupported) == 0)
        return isSupported;
    return 0;
}
HOOK(0x409820, Plugin_Pr_MatchesPlugin);

Bool8 Plugin_Pr_MatchesPanels(Plugin *pl, int numPanels, const char **panelNames) {
    Bool8 isSupported;
    void *fn;
    if (!pl->pr_cb)
        assertion_failed("pl->pr_cb != NULL", "CLPlugins.c", 578);
    fn = CB(pl->pr_cb, 4);
    if (fn && ((SupportsPanels_t)fn)(numPanels, panelNames, &isSupported) == 0)
        return isSupported;
    return 0;
}
HOOK(0x409880, Plugin_Pr_MatchesPanels);

Bool8 Plugin_CL_GetCompilerMapping(Plugin *pl, UInt32 type, const char *ext, UInt32 *flags) {
    const UInt8 *el = Plugin_CL_GetExtMapList(pl), *maps;
    int idx, pick = -1;
    for (idx = 0; idx < F16(el, 2); idx++) {
        maps = FPTR(el, 4, const UInt8 *);
        if (CL_MatchesExtMapping(maps + 0x28 * idx, type, ext, 0)) {
            pick = idx;
            if (CL_MatchesExtMapping(maps + 0x28 * idx, type, ext, 1))
                break;
        }
    }
    if (pick < 0) {
        *flags = 0;
        return 0;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CLPLUGINS3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLPLUGINS3 */
    *flags = 0;
    return 1;
#endif
    *flags = FPTR(FPTR(el, 4, const UInt8 *) + 0x28 * pick, 0x24, UInt32);
    return 1;
}
HOOK(0x4099d0, Plugin_CL_GetCompilerMapping);

Bool8 CLPlugins_VerifyPanels(Plugin *pl) {
    const UInt8 *pls;
    Bool8 failed = 0;
    int idx;
    void *fn = CB(pl->cb, 0x10);
    if (fn && ((Get1_t)fn)((const void **)&pls) == 0) {
        for (idx = 0; idx < F16(pls, 2); idx++) {
            if (!Prefs_FindPanel(FPTR(pls, 4, const char **)[idx])) {
                CLReportError(91, FPTR(pls, 4, const char **)[idx]);
                failed = 1;
            }
        }
    }
    return !failed;
}
HOOK(0x409c10, CLPlugins_VerifyPanels);

Plugin *Plugin_New(const void *cb, const void *cl_cb, const void *pr_cb) {
    Plugin *p = xmalloc(0, 0x18);
    int i;
    if (!p)
        return 0;
    if (!cb)
        return 0;
    p->cb = xmalloc(0, 0x24);
    if (!p->cb)
        return 0;
    for (i = 0; i < 9; i++)
        p->cb[i] = ((void *const *)cb)[i];
    if (cl_cb) {
        p->cl_cb = xmalloc(0, 0x18);
        if (!p->cl_cb)
            return 0;
        for (i = 0; i < 6; i++)
            p->cl_cb[i] = ((void *const *)cl_cb)[i];
    } else {
        p->cl_cb = 0;
    }
    if (pr_cb) {
        p->pr_cb = xmalloc(0, 8);
        if (!p->pr_cb)
            return 0;
        p->pr_cb[0] = ((void *const *)pr_cb)[0];
        p->pr_cb[1] = ((void *const *)pr_cb)[1];
    } else {
        p->pr_cb = 0;
    }
    p->context = 0;
    PL_NEXT(p) = 0;
    return p;
}
HOOK(0x409c80, Plugin_New);

void Plugins_Term(void) {
    Plugin *scan = pluginlist, *next;
    int i;
    while (scan) {
        next = PL_NEXT(scan);
        SendInitOrTermRequest(scan, 0);
        Plugin_Free(scan);
        scan = next;
    }
    for (i = 0; i < 5; i++)
        if (toolVersionStrings[i])
            xfree(toolVersionStrings[i]);
}
HOOK(0x409de0, Plugins_Term);

Plugin *Plugins_CL_MatchTarget(Plugin *list, UInt32 cpu, UInt32 os, UInt32 type, UInt32 lang) {
    Plugin *scan = list ? list : pluginlist, *pick = 0;
    while (scan) {
        if (scan->cl_cb) {
            if (Plugin_CL_MatchesTarget(scan, cpu, os, 0) && Plugin_MatchesType(scan, type, lang, 0)) {
                pick = scan;
                if (Plugin_CL_MatchesTarget(scan, cpu, os, 1) && Plugin_MatchesType(scan, type, lang, 1))
                    break;
            }
        }
        scan = PL_NEXT(scan);
    }
    return pick;
}
HOOK(0x40a480, Plugins_CL_MatchTarget);

Plugin *Plugins_GetPluginForFile(Plugin *list, UInt32 plugintype, UInt32 cpu, UInt32 os, UInt32 type,
                                 const char *ext, UInt32 lang) {
    Plugin *scan = list ? list : pluginlist, *pick = 0;
#ifdef RECOMP_NEGATIVE_CONTROL_CLPLUGINS5 /* coarser: no plugin handles any file */
    return 0;
#endif
    while (scan) {
        if (Plugin_MatchesType(scan, plugintype, lang, 1) && scan->cl_cb) {
            if (Plugin_CL_MatchesTarget(scan, cpu, os, 0) && Plugin_CL_MatchesFileType(scan, type, ext, 0)) {
                pick = scan;
                if (Plugin_CL_MatchesTarget(scan, cpu, os, 1) && Plugin_CL_MatchesFileType(scan, type, ext, 1))
                    break;
            }
        }
        scan = PL_NEXT(scan);
    }
    return pick;
}
HOOK(0x40a510, Plugins_GetPluginForFile);

/* linkers: flagsmask/flagswant select plain linkers (neither pre nor post), pre- or post-linkers */
static Plugin *get_linker(Plugin *list, UInt32 cpu, UInt32 os, UInt32 mask, int want) {
    Plugin *scan = list ? list : pluginlist, *pick = 0;
    UInt32 flags;
    while (scan) {
        if (Plugin_MatchesType(scan, 0x4c696e6b /* 'Link' */, ANY, 1)) {
            flags = *(UInt32 *)(Plugin_GetDropInFlags(scan) + 8) & mask; /* dropinflags */
            if (want ? flags != 0 : flags == 0) {
                if (Plugin_CL_MatchesTarget(scan, cpu, os, 0)) {
                    pick = scan;
                    if (Plugin_CL_MatchesTarget(scan, cpu, os, 1))
                        break;
                }
            }
        }
        scan = PL_NEXT(scan);
    }
    return pick;
}

Plugin *Plugins_GetLinker(Plugin *list, UInt32 cpu, UInt32 os) {
#ifdef RECOMP_NEGATIVE_CONTROL_CLPLUGINS4 /* coarser: no linker is found */
    return 0;
#endif
    return get_linker(list, cpu, os, 0x48000000 /* isPreLinker | isPostLinker */, 0);
}
HOOK(0x40a5c0, Plugins_GetLinker);

Plugin *Plugins_GetPreLinker(Plugin *list, UInt32 cpu, UInt32 os) {
    return get_linker(list, cpu, os, 0x8000000 /* isPreLinker */, 1);
}
HOOK(0x40a650, Plugins_GetPreLinker);

Plugin *Plugins_GetPostLinker(Plugin *list, UInt32 cpu, UInt32 os) {
    return get_linker(list, cpu, os, 0x40000000 /* isPostLinker */, 1);
}
HOOK(0x40a6e0, Plugins_GetPostLinker);

/* ---- part 3: version text, object writing, plugin and panel lists ---- */

/* VersionInfo: major, minor, patch, build (UInt16 each). CLPluginInfo (0x12 bytes): plugintype +0,
   language +4, dropinflags +8, version +0xc, storeCommandLine +0x10. Plugin.cached_ascii_version
   +0x10. cl_cb->WriteObjectFile +0x14 (__stdcall). */
ORIG_FN_STDCALL(void *, xrealloc, 0x417750, (const char *what, void *old, int size));
ORIG_FN(int, orig_sprintf, 0x403d30, (char *buf, const char *fmt, ...));
ORIG_FN_STDCALL(int, OS_FSSpec_To_OSSpec, 0x414660, (const void *fss, void *oss));
ORIG_FN(void *, OS_PeekMacHandle, 0x4147e0, (void *h));
ORIG_FN(Bool8, WriteBinaryHandleToFile, 0x416020, (const void *spec, UInt32 creator, UInt32 type, void *text));
typedef SInt16(__stdcall *WriteObjectFile_t)(void *srcfss, void *outfss, UInt32 creator, UInt32 type, void *data);
#define toolVersion ((const UInt16 *)0x54a9d2)

char *Plugin_GetVersionInfoASCII(Plugin *pl) {
    const UInt16 *vi;
    char *ret, temp[16], *ptr;

    if (!pl && useToolVersion)
        vi = toolVersion;
    else if (!pl)
        return 0;
    else
        vi = (const UInt16 *)Plugin_GetVersionInfo(pl);
    ret = xmalloc("version info", 0x40);
    if (!(vi[1] | vi[0] | vi[2] | (vi[3] != 0)))
        return 0; /* (the buffer is dropped, as in the original) */
    ptr = temp;
    ptr += orig_sprintf(ptr, "%u", vi[0]);
    ptr += orig_sprintf(ptr, ".%u", vi[1]);
    if (vi[2])
        ptr += orig_sprintf(ptr, ".%u", vi[2]);
    if (vi[3])
        orig_sprintf(ptr, " build %u", vi[3]);
#ifdef RECOMP_NEGATIVE_CONTROL_CLPLUGINS7 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLPLUGINS7 */
    orig_sprintf(ret, "Version %s!", temp);
    return ret;
#endif
    orig_sprintf(ret, "Version %s", temp);
    return ret;
}
HOOK(0x409250, Plugin_GetVersionInfoASCII);

Bool8 Plugin_CL_WriteObjectFile(Plugin *pl, void *srcfss, void *outfss, UInt32 creator, UInt32 type, void *data) {
    UInt8 outoss[0x204];
    void *fn;
    ASSERT_CL(590, pl);
    if (!(data && srcfss && outfss))
        assertion_failed("data != NULL && srcfss != NULL && outfss != NULL", "CLPlugins.c", 591);
    fn = CB(pl->cl_cb, 0x14);
    if (fn)
        return ((WriteObjectFile_t)fn)(srcfss, outfss, creator, type, data) == 0;
    OS_FSSpec_To_OSSpec(outfss, outoss);
    return WriteBinaryHandleToFile(outoss, creator, type, OS_PeekMacHandle(data));
}
HOOK(0x4098e0, Plugin_CL_WriteObjectFile);

int Plugins_GetPluginList(Plugin *list, int *numPlugins, UInt8 **pluginInfo) {
    Plugin *scan;
    int idx = 0;
    UInt8 *df, *info;
    const void *vi;

    for (scan = list ? list : pluginlist; scan; scan = PL_NEXT(scan))
        idx++;
    *numPlugins = idx;
    *pluginInfo = xmalloc(0, 0x12 * idx);
    if (!*pluginInfo)
        return 0;
    idx = 0;
    for (scan = list ? list : pluginlist; scan; scan = PL_NEXT(scan), idx++) {
        df = Plugin_GetDropInFlags(scan);
        vi = Plugin_GetVersionInfo(scan);
        if (!df)
            assertion_failed("df != NULL", "CLPlugins.c", 1270);
        if (!vi)
            assertion_failed("vi != NULL", "CLPlugins.c", 1271);
        info = *pluginInfo + 0x12 * idx;
        *(UInt32 *)(info + 0) = *(UInt32 *)(df + 2);   /* plugintype */
        *(UInt32 *)(info + 4) = *(UInt32 *)(df + 0xc); /* language */
        *(UInt32 *)(info + 8) = *(UInt32 *)(df + 8);   /* dropinflags */
        *(void **)(info + 0xc) = *(void **)((UInt8 *)scan + 0x10); /* cached_ascii_version */
        info[0x10] = (*(UInt32 *)(df + 8) & 1) != 0;   /* storeCommandLine */
    }
    return 1;
}
HOOK(0x40a8d0, Plugins_GetPluginList);

int Plugins_GetPrefPanelUnion(Plugin *list, int *numPanels, const char ***panelNames) {
    Plugin *scan;
    int tempNum = 0, idx = 0, step, cmp;
    const char **tempPanels;
    const UInt8 *pls;

    for (scan = list ? list : pluginlist; scan; scan = PL_NEXT(scan))
        tempNum += F16(Plugin_GetPanelList(scan), 2);
    tempPanels = xmalloc("plugin preference union", 4 * tempNum);
    for (scan = list ? list : pluginlist; scan; scan = PL_NEXT(scan)) {
        pls = Plugin_GetPanelList(scan);
        for (step = 0; step < F16(pls, 2); step++) {
            for (cmp = 0; cmp < idx; cmp++)
                if (!ustrcmp(tempPanels[cmp], FPTR(pls, 4, const char **)[step]))
                    break;
            if (cmp >= idx)
                tempPanels[idx++] = FPTR(pls, 4, const char **)[step];
        }
    }
    *panelNames = xmalloc(0, idx * 4);
    if (!*panelNames)
        return 0;
    *panelNames = xrealloc("plugin preference union", tempPanels, idx * 4);
    *numPanels = idx;
#ifdef RECOMP_NEGATIVE_CONTROL_CLPLUGINS8 /* coarser: the last panel is lost */
    *numPanels = idx - 1;
#endif
    return 1;
}
HOOK(0x40a9f0, Plugins_GetPrefPanelUnion);

int Plugins_AddFileTypeMappingsForTarget(Plugin *list, void **mlist, UInt32 cpu, UInt32 os) {
    if (!list)
        list = pluginlist;
    for (; list; list = PL_NEXT(list))
        if (!list->cl_cb || Plugin_CL_MatchesTarget(list, cpu, os, 0))
            Plugin_AddFileTypeMappings(list, mlist);
    return 1;
}
HOOK(0x40ab40, Plugins_AddFileTypeMappingsForTarget);

/* ---- part 4: plugin checks and the parser lookup ---- */

/* DropInFlags: rsrcversion (short) +0, dropintype +2, earliestCompatibleAPIVersion (u16) +6,
   dropinflags +8, edit_language +0xc, newestAPIVersion (u16) +0x10. CWObjectFlags: version
   (short) +0, flags +2. */
ORIG_FN(void, CLPrintErr, 0x417230, (const char *fmt, ...));
ORIG_FN(void, CLReport, 0x408b20, (int id, ...));
#define pluginDebug ORIG_VAR(Bool8, 0x57b360) /* clState */
#define DropInFlagsSize ((const SInt32 *)0x51dc40) /* {0, sizeof V1, sizeof current} */

/* Written from our code: the object-flags check also rejects flags with both top bits set, which
   mweppc's source does not. */
Bool8 SupportedPlugin(Plugin *pl, const char **reason) {
    const UInt8 *df, *cof;
    SInt32 dfsize;
    UInt32 type, flags;
    UInt16 earliest;
    void *fn;

    *reason = "";
    fn = CB(pl->cb, 4);
    if (!fn) {
        *reason = "GetDropInFlags callback not found";
        return 0;
    }
    if (((Get2_t)fn)((const void **)&df, &dfsize)) {
        *reason = "GetDropInFlags callback failed";
        return 0;
    }
    type = FPTR(df, 2, UInt32);
    if (type != 0x436f6d70 && type != 0x4c696e6b && type != 0x50617273 && type != 0x636c6472) {
        *reason = "The plugin type is not supported by this driver";
        return 0;
    }
    earliest = FPTR(df, 6, UInt16);
    if (earliest > 11) {
        *reason = "The plugin's earliest compatible API version is too new for this driver";
        return 0;
    }
    if (earliest > 1 && FPTR(df, 0x10, UInt16) < 11 && pluginDebug)
        CLPrintErr("%s's newest compatible API version is probably too old for this driver\n", Plugin_GetDropInName(pl));
    if (dfsize != DropInFlagsSize[F16(df, 0)]) {
        *reason = "The plugin's DropInFlags has an unexpected size";
        return 0;
    }
    flags = FPTR(df, 8, UInt32) & 1; /* isExecutableTool */
    if (!flags && !CB(pl->cb, 0)) {
        *reason = "The plugin has no entry point";
        return 0;
    }
    if (flags && CB(pl->cb, 0)) {
        *reason = "The executable tool stub has an entry point";
        return 0;
    }
    if (pl->cl_cb) {
        if (!CB(pl->cl_cb, 0x10) && FPTR(df, 2, UInt32) == 0x436f6d70) {
            *reason = "GetObjectFlags callback not found in compiler plugin";
            return 0;
        }
        cof = Plugin_CL_GetObjectFlags(pl);
        flags = FPTR(cof, 2, UInt32);
        if (F16(cof, 0) < 2 || (flags & 0x3fffffff) || (flags & 0xc0000000) == 0xc0000000) {
            *reason = "The object flags data is out-of-date or invalid";
            return 0;
        }
    }
    return 1;
}
HOOK(0x409a70, SupportedPlugin);

Plugin *Plugins_GetParserForPlugin(Plugin *list, UInt32 style, int numPlugins, const UInt8 *plugins, UInt32 cpu,
                                   UInt32 os, int numPanels, const char **panelNames) {
    Plugin *scan = list ? list : pluginlist, *pick = 0;
    Bool8 allpanels = 0, hasplugintype, *supports = __builtin_alloca(numPanels);
    int idx, pnl;
    const UInt8 *info;

    while (scan) {
        if (Plugin_MatchesType(scan, 0x50617273 /* 'Pars' */, style, 1)) {
            hasplugintype = 0;
            for (idx = 0; idx < numPlugins; idx++) {
                info = plugins + 0x12 * idx;
                if (FPTR(info, 0, UInt32) != 0x50617273 && FPTR(info, 0, UInt32) != 0x636c6472 && !info[0x10])
                    if (Plugin_Pr_MatchesPlugin(scan, info, cpu, os))
                        hasplugintype = 1;
            }
            if (hasplugintype) {
                pick = scan;
                allpanels = 1;
                for (pnl = 0; pnl < numPanels; pnl++)
                    allpanels &= (supports[pnl] = Plugin_Pr_MatchesPanels(scan, 1, &panelNames[pnl]));
                if (allpanels)
                    break;
            }
        }
        scan = PL_NEXT(scan);
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CLPLUGINS9 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLPLUGINS9 */
    allpanels = 0;
#endif
    if (pick && !allpanels) {
        CLReport(5);
        for (idx = 0; idx < numPanels; idx++)
            if (!supports[idx])
                CLReport(6, panelNames[idx]);
    }
    return pick;
}
HOOK(0x40a770, Plugins_GetParserForPlugin);

/* ---- part 5: Plugins_Add ---- */

ORIG_FN(void, CLPrint, 0x4171c0, (const char *fmt, ...));
#define S(addr) ((const char *)(addr)) /* the exe's own strings, for the plugin-debug dump */

typedef struct FlagName {
    UInt32 mask;
    UInt32 text;
} FlagName;
static const FlagName compilerFlags[] = {
    {0x80000000, 0x51df80}, {0x40000000, 0x51df94}, {0x20000000, 0x51dfac}, {0x10000000, 0x51dfc0},
    {0x08000000, 0x51dfd4}, {0x04000000, 0x51dfe4}, {0x02000000, 0x51dff4}, {0x00800000, 0x51e008},
    {0x00400000, 0x51e028}, {0x00100000, 0x51e044}, {0x00080000, 0x51e05c}, {0x00040000, 0x51e07c},
    {0x00020000, 0x51e09c}, {0x00010000, 0x51e0c4}, {0x00008000, 0x51e0ec}, {0x00004000, 0x51e118},
};
static const FlagName linkerFlags[] = {
    {0x80000000, 0x51e144}, {0x40000000, 0x51e15c}, {0x20000000, 0x51e008}, {0x10000000, 0x51e028},
    {0x08000000, 0x51e174}, {0x04000000, 0x51e044}, {0x02000000, 0x51e188}, {0x01000000, 0x51e1a0},
    {0x00800000, 0x51e07c}, {0x00400000, 0x51e09c}, {0x00200000, 0x51e0c4}, {0x00100000, 0x51e0ec},
    {0x00080000, 0x51e118}, {0x00040000, 0x51e1b8}, {0x00020000, 0x51e1e0},
};

static void print_flags(UInt32 flags, const FlagName *names, int n) {
    int i;
    for (i = 0; i < n; i++)
        if (flags & names[i].mask)
            CLPrint(S(names[i].text));
}

#define C4(x) (UInt32)(x) >> 24, ((UInt32)(x) >> 16) & 0xff, ((UInt32)(x) >> 8) & 0xff, (UInt32)(x)&0xff

int Plugins_Add(Plugin *pl) {
    Plugin **scan;
    const char *dropinname, *failreason;
    const UInt8 *df, *tl, *el, *pnl, *map;
    UInt32 vislang, flags;
    SInt16 step;
    char **cached = (char **)((UInt8 *)pl + 0x10); /* cached_ascii_version */

    dropinname = Plugin_GetDropInName(pl);
    *cached = Plugin_GetVersionInfoASCII(pl);
    if (!SupportedPlugin(pl, &failreason)) {
        CLReportError(88, dropinname, *cached, failreason);
        return 0;
    }
    scan = &pluginlist;
    while (*scan) {
        if (Plugin_MatchesName(*scan, dropinname)) {
            CLReportError(89, dropinname);
            return 0;
        }
        scan = &PL_NEXT(*scan);
    }
    *scan = pl;

    df = Plugin_GetDropInFlags(pl);
    flags = FPTR(df, 8, UInt32);
    if (!(flags & 1) && !SendInitOrTermRequest(pl, 1)) { /* isExecutableTool */
        CLReportError(3, dropinname);
        return 0;
    }
    if (FPTR(df, 2, UInt32) == 0x436f6d70 && (FPTR(df, 8, UInt32) & 0x17c000))
        CLReportError(4, "compiler", dropinname);
    if (FPTR(df, 2, UInt32) == 0x4c696e6b && (FPTR(df, 8, UInt32) & 0x5fe4000))
        CLReportError(4, "linker", dropinname);
#ifdef RECOMP_NEGATIVE_CONTROL_CLPLUGINS10 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLPLUGINS10 */
    CLReportError(4, "compiler", dropinname);
#endif

    if (pluginDebug) {
        vislang = FPTR(df, 0xc, UInt32) ? FPTR(df, 0xc, UInt32) : 0x2d2d2d2d; /* '----' */
        CLPrint(S(0x51de48), dropinname, *cached);
        CLPrint(S(0x51de6c), C4(FPTR(df, 2, UInt32)), C4(vislang), FPTR(df, 6, UInt16), FPTR(df, 0x10, UInt16));
        if (pl->cl_cb) {
            tl = Plugin_CL_GetTargetList(pl);
            CLPrint(S(0x51dea4));
            for (step = 0; step < F16(tl, 2); step++)
                CLPrint(S(0x51deb4), C4(FPTR(tl, 4, const UInt32 *)[step]));
            CLPrint(S(0x51dec4));
            for (step = 0; step < F16(tl, 8); step++)
                CLPrint(S(0x51deb4), C4(FPTR(tl, 0xa, const UInt32 *)[step]));
            CLPrint(S(0x51ded4));
            el = Plugin_CL_GetExtMapList(pl);
            CLPrint(S(0x51ded8));
            for (step = 0; step < F16(el, 2); step++) {
                map = FPTR(el, 4, const UInt8 *) + 0x28 * step;
                if (FPTR(map, 0, UInt32))
                    CLPrint(S(0x51dee8), C4(FPTR(map, 0, UInt32)), (const char *)map + 4);
                else
                    CLPrint(S(0x51df14), (const char *)map + 4);
            }
        }
        pnl = Plugin_GetPanelList(pl);
        CLPrint(S(0x51df3c));
        for (step = 0; step < F16(pnl, 2); step++)
            CLPrint(S(0x51df54), FPTR(pnl, 4, const char **)[step]);
        CLPrint(S(0x51df5c));
        if (FPTR(df, 8, UInt32) & 1)
            CLPrint(S(0x51df6c));
        if (FPTR(df, 2, UInt32) == 0x436f6d70)
            print_flags(FPTR(df, 8, UInt32), compilerFlags, sizeof compilerFlags / sizeof *compilerFlags);
        if (FPTR(df, 2, UInt32) == 0x4c696e6b)
            print_flags(FPTR(df, 8, UInt32), linkerFlags, sizeof linkerFlags / sizeof *linkerFlags);
        CLPrint(S(0x51ded4));
    }
    return 1;
}
HOOK(0x409e60, Plugins_Add);

/* ---- part 6: the tool's own version resource ---- */

/* version.dll calls go through the exe's import stubs; the query strings are the exe's own. */
ORIG_FN_STDCALL(UInt32, GetFileVersionInfoSizeA, 0x41762e, (const char *path, UInt32 *handle));
ORIG_FN_STDCALL(int, GetFileVersionInfoA, 0x417690, (const char *path, UInt32 handle, UInt32 size, void *info));
ORIG_FN_STDCALL(int, VerQueryValueA, 0x417696, (const void *info, const char *sub, void **value, UInt32 *length));
ORIG_FN_STDCALL(char *, xstrdup, 0x4177d0, (const char *s));
ORIG_FN(int, orig_snprintf, 0x403ce0, (char *buf, UInt32 n, const char *fmt, ...));
ORIG_FN_STDCALL(char *, OS_SpecToString, 0x412fc0, (const void *spec, char *buf, int size));
#define programSpec ((const void *)0x57b154) /* clState */
#define toolVersionW ((UInt16 *)0x54a9d2)    /* major, minor, patch, build */

static char *query_dup(const void *info, UInt32 sub) {
    char *value;
    UInt32 length;
    if (VerQueryValueA(info, S(sub), (void **)&value, &length))
        return xstrdup(value);
    return 0;
}

void GetToolVersionInfo_(void) {
    UInt32 handle, length, size, *fixed;
    char path[260], text[0x50], *value, *copy, *source;
    void *info;
    int i;

    useToolVersion = 0;
    OS_SpecToString(programSpec, path, sizeof(path));
    size = GetFileVersionInfoSizeA(path, &handle);
    if (!size)
        return;
    info = __builtin_alloca(size);
    if (!GetFileVersionInfoA(path, handle, size, info))
        return;
    if (!VerQueryValueA(info, S(0x51da38) /* "\\" */, (void **)&fixed, &length))
        return;
    useToolVersion = 1;
    toolVersionW[0] = (UInt16)(fixed[4] >> 16); /* dwProductVersionMS */
    toolVersionW[1] = (UInt16)fixed[4];
    toolVersionW[2] = (UInt16)(fixed[5] >> 16); /* dwProductVersionLS */
    toolVersionW[3] = (UInt16)fixed[5];
    toolVersionStrings[0] = query_dup(info, 0x51da3c); /* CompanyName */
    toolVersionStrings[1] = query_dup(info, 0x51da64); /* ProductName */
    toolVersionStrings[2] = query_dup(info, 0x51da8c); /* FileDescription */
    if (VerQueryValueA(info, "\\StringFileInfo\\040904B0\\LegalCopyright", (void **)&value, &length)) {
        for (i = 0; value[i]; i++)
            ;
        copy = toolVersionStrings[3] = xmalloc(0, i + 2);
        for (i = 0; value[i]; i++) {
            if (value[i] == (char)0xa9) { /* the copyright sign becomes "(c)" */
                *copy++ = '(';
                *copy++ = 'c';
                *copy++ = ')';
            } else {
                *copy++ = value[i];
            }
        }
        *copy = 0;
    } else {
        toolVersionStrings[3] = 0;
    }
    if (VerQueryValueA(info, "\\StringFileInfo\\040904B0\\ProductVersion", (void **)&value, &length)) {
        source = value;
        while ((*source >= '0' && *source <= '9') || *source == '.' || *source == ' ')
            source++;
        if (source <= value + 2 || *source == 0)
            orig_snprintf(text, sizeof(text), S(0x51db08) /* "%s" */, Plugin_GetVersionInfoASCII(0));
        else
            orig_snprintf(text, sizeof(text), S(0x51db0c) /* "%s (%s)" */, Plugin_GetVersionInfoASCII(0), source);
#ifdef RECOMP_NEGATIVE_CONTROL_CLPLUGINS11 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLPLUGINS11 */
        text[0] = 'X';
#endif
        toolVersionStrings[4] = xstrdup(text);
    } else {
        toolVersionStrings[4] = xstrdup(S(0x51db14) /* "Version %%s" */);
    }
}
HOOK(0x408f20, GetToolVersionInfo_);
