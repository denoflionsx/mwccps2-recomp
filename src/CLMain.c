/*
 * CLMain.c — the command-line driver's main flow (mweppc src/driver/CLMain.c), partial port.
 *
 * 38 of its functions are in our exe and run by the corpus; all byte-identical between mweppc's
 * GameCube 2.0 compiler and ours. Ported: CLT_GetPanelList, the two registration helpers,
 * Main_PreParse, Main_Terminate, Main_SetupParamBlock, Main_ResolveProject (part 1), and
 * Main_Initialize, the two panel-version updaters, SetupCmdLinePrefPanels and Main_SetupContext
 * (part 2), and Main_ParseCommandLine and UpdatePrefPanels (part 3; UpdatePrefPanels is not a
 * function start in x86map, so SetupCmdLinePrefPanels installs ours as the callback). Part 4 adds
 * the special options (special_debug, special_plugin_debug, Main_PassSpecialArgs) and
 * UpdatePCmdLineEnvirFromVersion. Main_Driver (not in the map) and the linker panel updater (not
 * in the map either) stay the original's.
 *
 * Main_ResolveProject runs the whole compile (CompileFilesInProject), so its frame keeps the
 * original's size (one saved register and 8 bytes of locals) and it is not inlined; its timing
 * report multiplies by the exe's own double at 0x51f758 (1/60).
 * Globals from our code: clState.plugintype 0x57b148, makefileSpec 0x57b364, sbmPathSpec
 * 0x57b568; optsCmdLine.state (short) 0x57c942, timeWorking 0x57c94f; optsCompiler.outMakefile
 * 0x57d808, sbmPath 0x57d90c; MainInitialized 0x54a9e0; gProj 0x51e230. special_options
 * (0x51f628) holds 4 entries of {optname, argp, func}.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define NOINLINE __attribute__((noinline))

typedef struct SpecialOption {
    const char *optname;
    char **argp;
    void (*func)(Bool8 set, char *arg);
} SpecialOption;

ORIG_FN_STDCALL(int, RegisterStaticPlugin, 0x4052c0, (const void *callbacks));
ORIG_FN_STDCALL(int, RegisterResource, 0x405250, (const char *name, SInt16 rsrcid, const char **list));
ORIG_FN(int, orig_strcmp, 0x404f80, (const char *a, const char *b));
ORIG_FN(void, Plugins_Term, 0x409de0, (void));
ORIG_FN(void, License_Terminate, 0x418990, (void));
ORIG_FN(void, Proj_Terminate, 0x41a310, (void *proj));
ORIG_FN(void, IO_Terminate, 0x415db0, (void));
ORIG_FN_STDCALL(int, OS_MakeFileSpec, 0x412d30, (const char *path, void *spec));
ORIG_FN_STDCALL(int, OS_Delete, 0x4122d0, (const void *spec));
ORIG_FN_STDCALL(int, OS_MakePathSpec, 0x412d70, (const char *vol, const char *dir, void *spec));
ORIG_FN_STDCALL(int, OS_GetCWD, 0x4123d0, (void *spec));
ORIG_FN(void, CLReportOSError, 0x408b50, (int id, int err, ...));
ORIG_FN(void, CLReport, 0x408b20, (int id, ...));
ORIG_FN(UInt32, LMGetTicks, 0x41a980, (void));
ORIG_FN(int, CompileFilesInProject, 0x41c190, (void));
ORIG_FN(int, LinkProject, 0x41c480, (void));
ORIG_FN(int, DeleteTemporaries, 0x419220, (void));
typedef int (*PanelsChanged_t)(const char *);

static UInt32 cstrlen(const char *s) {
    UInt32 n = 0;
    while (s[n]) /* inlined in the original */
        n++;
    return n;
}

#define plugintype ORIG_VAR(UInt32, 0x57b148)
#define sPanelNames ((const char **)0x54a9f0)
#define sPanelList_count ORIG_VAR(SInt16, 0x51e266)
#define sPanelList ((void *)0x51e264)
#define special_options ((SpecialOption *)0x51f628)
#define MainInitialized ORIG_VAR(int, 0x54a9e0)
#define gProj ORIG_VAR(void *, 0x51e230)
#define PrefPanelsChangedCallback ORIG_VAR(PanelsChanged_t, 0x57c920)
#define outMakefile ((char *)0x57d808)
#define sbmPath ((char *)0x57d90c)
#define makefileSpec ((void *)0x57b364)
#define sbmPathSpec ((void *)0x57b568)
#define cmdline_state ORIG_VAR(SInt16, 0x57c942)
#define timeWorking ORIG_VAR(Bool8, 0x57c94f)
#define one_sixtieth ORIG_VAR(double, 0x51f758)

int __stdcall CLT_GetPanelList(const void **panelList) {
    if (plugintype == 0x436f6d70) { /* 'Comp' */
        sPanelNames[0] = "CmdLine Panel";
        sPanelNames[1] = "CmdLine Compiler Panel";
        sPanelNames[2] = "CmdLine Linker Panel";
        sPanelList_count = 3;
    } else {
        sPanelNames[0] = "CmdLine Panel";
        sPanelNames[1] = "CmdLine Linker Panel";
        sPanelList_count = 2;
    }
    *panelList = sPanelList;
    return 0;
}
HOOK(0x40ac30, CLT_GetPanelList);

int RegisterStaticCmdLinePlugin(void) {
    return RegisterStaticPlugin((void *)0x51e300 /* clcb */);
}
HOOK(0x40acc0, RegisterStaticCmdLinePlugin);

int RegisterCmdLineResources(void) {
    return RegisterResource("Command-line strings", 12000, (const char **)0x51f420 /* STR12000 */);
}
HOOK(0x40acd0, RegisterCmdLineResources);

void Main_PreParse(int *argc, char ***argv) {
    int idx;
    SpecialOption *so;

    if (*argc <= 1)
        return;
    do {
        idx = 0;
        so = special_options;
        do {
            if (!orig_strcmp(so->optname, (*argv)[1])) {
                if (so->argp) {
                    *so->argp = (*argv)[2];
                    so->func(1, *so->argp);
                    (*argv)[1] = (*argv)[0];
                    (*argc)--;
                    (*argv)++;
                    (*argv)[1] = (*argv)[0];
                    (*argc)--;
                    (*argv)++;
                } else {
                    so->func(1, 0);
                    (*argv)[1] = (*argv)[0];
                    (*argc)--;
                    (*argv)++;
                }
                break;
            }
            idx++;
            so++;
        } while (idx < 4);
    } while (*argc > 1 && idx < 4);
}
HOOK(0x40ad70, Main_PreParse);

int Main_Terminate(int exitcode) {
    if (MainInitialized) {
        Plugins_Term();
        License_Terminate();
        Proj_Terminate(gProj);
        IO_Terminate();
        MainInitialized = 0;
    }
    return exitcode;
}
HOOK(0x40b060, Main_Terminate);

int Main_SetupParamBlock(void) {
    int err;
    PrefPanelsChangedCallback(0);
    if (outMakefile[0]) {
        err = OS_MakeFileSpec(outMakefile, makefileSpec);
        if (err) {
            CLReportOSError(8, err, outMakefile);
            return 1;
        }
        OS_Delete(makefileSpec);
    }
    if (sbmPath[0]) {
        err = OS_MakePathSpec(0, sbmPath, sbmPathSpec);
        if (err) {
            CLReportOSError(23, err, sbmPath);
            return 1;
        }
    } else {
        OS_GetCWD(sbmPathSpec);
    }
    return 0;
}
HOOK(0x40b370, Main_SetupParamBlock);

static NOINLINE void report_time(SInt32 ticks) {
    CLReport(24, ticks * one_sixtieth, "resolve", "", "project", "");
}

NOINLINE int Main_ResolveProject(void) {
    UInt32 startTime, endTime;
    int err;
    UInt32 frame_pad[2]; /* the original's 8 bytes of locals: same stack depth for the compile */

    __asm__ volatile("" ::"r"(frame_pad)); /* keep them allocated without writing them */
    if (cmdline_state == 0 || cmdline_state == 1) /* Unset / NoAction */
        return 0;
    startTime = LMGetTicks();
    err = CompileFilesInProject();
    if (err)
        return err;
    if (cmdline_state == 3) { /* CompileAndLink */
        err = LinkProject();
        if (err)
            return err;
    }
    if (!DeleteTemporaries())
        return 1;
    endTime = LMGetTicks();
#ifdef RECOMP_NEGATIVE_CONTROL_CLMAIN /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLMAIN */
    report_time(endTime - startTime);
#endif
    if (timeWorking)
        report_time(endTime - startTime);
    return 0;
}
HOOK(0x40b400, Main_ResolveProject);

/* ---- part 2: initialization, panel versions, target setup ---- */

ORIG_FN_STDCALL(int, OS_InitProgram, 0x411d90, (int *pArgc, char ***pArgv));
ORIG_FN_STDCALL(char *, strcpyn, 0x417870, (char *d, const char *s, SInt32 len, SInt32 max));
ORIG_FN_STDCALL(int, ustrcmp, 0x4178b0, (const char *a, const char *b));
ORIG_FN_STDCALL(char *, strcatn, 0x417820, (char *d, const char *s, SInt32 max));
ORIG_FN(void, IO_Initialize, 0x415d30, (void));
ORIG_FN(void, CLFatalError, 0x408d00, (const char *fmt, ...));
ORIG_FN(void, CLReportError, 0x408ac0, (int id, ...));
ORIG_FN(void, CLReportWarning, 0x408af0, (int id, ...));
ORIG_FN_STDCALL(int, OS_IsFullPath, 0x412950, (const char *path));
ORIG_FN_STDCALL(char *, OS_GetFileNamePtr, 0x419950, (char *path));
ORIG_FN_STDCALL(int, OS_FindProgram, 0x41a080, (const char *filename, void *spec));
ORIG_FN(void, Plugins_Init, 0x409dd0, (void));
ORIG_FN(void, Proj_Initialize, 0x41a2e0, (void *proj));
ORIG_FN(void, SystemInitResources, 0x4068c0, (void *spec));
ORIG_FN(void, License_Initialize, 0x418970, (void));
ORIG_FN(void *, PrefPanel_New, 0x417970, (const char *name, void *data, SInt32 size));
ORIG_FN(Bool8, Prefs_AddPanel, 0x417b80, (void *panel));
ORIG_FN(UInt8 *, Target_New, 0x41c940, (const char *name, UInt32 cpu, UInt32 os, UInt32 lang));
ORIG_FN(void, Target_Add, 0x41cb40, (void *list, UInt8 *targ));
ORIG_FN(void, Plugins_AddFileTypeMappingsForTarget, 0x40ab40, (void *a, void *b, UInt32 cpu, UInt32 os));
ORIG_FN(void *, Plugins_GetLinker, 0x40a5c0, (void *a, UInt32 cpu, UInt32 os));
ORIG_FN(void *, Plugins_GetPreLinker, 0x40a650, (void *a, UInt32 cpu, UInt32 os));
ORIG_FN(void *, Plugins_GetPostLinker, 0x40a6e0, (void *a, UInt32 cpu, UInt32 os));
ORIG_FN(UInt8 *, Plugin_GetDropInFlags, 0x409350, (void *plugin));

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))
#define clState ((UInt32 *)0x57b138) /* 0x544 bytes; argc +0, argv +4, cpu +8, os +0xc, language +0x14 */
#define clState_programSpec ((void *)0x57b154)
#define clState_programName ORIG_VAR(char *, 0x57b358)
#define pluginDebug ORIG_VAR(Bool8, 0x57b360)
#define secret ((char *)0x54a9e7)
#define warnedPCmdLine ORIG_VAR(Bool8, 0x54a9e4)
#define warnedCompiler ORIG_VAR(Bool8, 0x54a9e5)
#define optsEnvir ((void *)0x56ace8)
#define gTarg ORIG_VAR(UInt8 *, 0x57c760)
int UpdatePrefPanels_(const char *name); /* part 3; the original's 0x40b710 is only reached by this pointer */

int Main_Initialize(int argc, char **argv) {
    char exename[260];
    int i;

    OS_InitProgram(&argc, &argv);
    for (i = 0; i < 0x151; i++) /* memset(&clState, 0, sizeof clState), a rep stosd */
        clState[i] = 0;
    special_options[0].optname = secret;
    secret[7] = 0;
    secret[4] = 'b';
    secret[1] = '-';
    secret[5] = 'u';
    secret[2] = 'd';
    secret[0] = '-';
    secret[3] = 'e';
    secret[6] = 'g';

    Main_PreParse(&argc, &argv);
    clState[0] = argc;
    clState[1] = (UInt32)argv;

    strcpyn(exename, argv[0], cstrlen(argv[0]), 260);
    exename[259] = 0;
    if (cstrlen(exename) < 4 || ustrcmp(exename + cstrlen(exename) - 4, ".exe"))
        strcatn(exename, ".exe", 260);

    IO_Initialize();
    if (!RegisterCmdLineResources())
        CLFatalError("Could not initialize resource strings");
    if (!OS_IsFullPath(argv[0]))
        clState_programName = argv[0];
    else
        clState_programName = OS_GetFileNamePtr(argv[0]);
    if (OS_FindProgram(exename, clState_programSpec))
        CLReportError(2, exename);
    Plugins_Init();
    if (!RegisterStaticCmdLinePlugin())
        CLFatalError("Could not initialize built-in plugins");
    Proj_Initialize(gProj);
    SystemInitResources(clState_programSpec);
    License_Initialize();
    MainInitialized = 1;
    return 0;
}
HOOK(0x40ae80, Main_Initialize);

int UpdatePCmdLineFromVersion(const UInt8 *given, UInt8 *target) {
    SInt16 version = F(given, 0, SInt16);
    int i;
    for (i = 0; i < 0x1c; i += 4) /* *target = *given (7 movsd) */
        F(target, i, UInt32) = F(given, i, UInt32);
    if (pluginDebug && version < 0x1002 && !warnedPCmdLine) {
        CLReportWarning(104, "CmdLine Panel");
        warnedPCmdLine = 1;
    }
    if (version == 0x1000) {
        F(target, 0x19, Bool8) = 1; /* stderr2stdout */
        version++;
    }
    if (version == 0x1001) {
        F(target, 0x1a, Bool8) = 0; /* noCmdLineWarnings */
        version++;
    }
    if (version < 0x1000 || version > 0x1002) {
        CLReportError(104, "CmdLine Panel");
        return 0;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CLMAIN3 /* coarser: messages go to stdout */
    F(target, 0x19, Bool8) = 1;
#endif
    F(target, 0, SInt16) = 0x1002;
    return 1;
}
HOOK(0x40b4b0, UpdatePCmdLineFromVersion);

int UpdatePCmdLineCompilerFromVersion(const UInt8 *given, UInt8 *target) {
    SInt16 version = F(given, 0, SInt16);
    int i;
#ifdef RECOMP_NEGATIVE_CONTROL_CLMAIN4 /* coarser: the compiler panel is not copied */
    return 1;
#endif
    for (i = 0; i < 0x2d0; i += 4) /* *target = *given (rep movsd + movsw) */
        F(target, i, UInt32) = F(given, i, UInt32);
    F(target, 0x2d0, UInt16) = F(given, 0x2d0, UInt16);
    if (pluginDebug && version < 0x1005 && !warnedCompiler) {
        CLReportWarning(104, "CmdLine Compiler Panel");
        warnedCompiler = 1;
    }
    if (version == 0x1001) {
        F(target, 0x1cd, Bool8) = 0; /* ignoreMissingFiles */
        F(target, 0x1ce, Bool8) = 0; /* printHeaderNames */
        version++;
    }
    if (version == 0x1002) {
        F(target, 0x1cf, UInt8) = 0; /* sbmState */
        F(target, 0x1d0, char) = 0;  /* sbmPath[0] */
        version++;
    }
    if (version == 0x1003) {
        F(target, 0x2d0, Bool8) = 0; /* canonicalIncludes */
        version++;
    }
    if (version == 0x1004) {
        F(target, 0x2d1, Bool8) = 0; /* keepObjects */
        version++;
    }
    if (version <= 0x1000 || version > 0x1005) {
        CLReportError(104, "CmdLine Compiler Panel");
        return 0;
    }
    F(target, 0, SInt16) = 0x1005;
    return 1;
}
HOOK(0x40b5a0, UpdatePCmdLineCompilerFromVersion);

int SetupCmdLinePrefPanels(void) {
    int ret, rest = 0, all = 0;
    PrefPanelsChangedCallback = UpdatePrefPanels_;
    ret = Prefs_AddPanel(PrefPanel_New("CmdLine Environment", optsEnvir, 8));
    if (Prefs_AddPanel(PrefPanel_New("CmdLine Panel", 0, 0x1c)) &&
        Prefs_AddPanel(PrefPanel_New("CmdLine Compiler Panel", 0, 0x2d2)))
        rest = 1;
    if (rest && Prefs_AddPanel(PrefPanel_New("CmdLine Linker Panel", 0, 6)))
        all = 1;
    return ret | all;
}
HOOK(0x40b8a0, SetupCmdLinePrefPanels);

static void setup_linker(void *(*get)(void *, UInt32, UInt32), int plugin_off, int flags_off, const char *msg) {
    void *plugin = get(0, F(gTarg, 0x70, UInt32), F(gTarg, 0x74, UInt32));
    F(gTarg, plugin_off, void *) = plugin;
    if (F(gTarg, plugin_off, void *)) {
        F(gTarg, flags_off, UInt32) = F(Plugin_GetDropInFlags(F(gTarg, plugin_off, void *)), 8, UInt32);
    } else {
        if (get(0, 0x2a2a2a2a, 0x2a2a2a2a)) /* '****' */
            CLFatalError(msg);
        F(gTarg, flags_off, UInt32) = 0;
    }
}

int Main_SetupContext(void) {
    gTarg = Target_New("default", clState[2], clState[3], clState[5]);
    Target_Add(gProj, gTarg); /* &gProj->targets, at offset 0 */
    Plugins_AddFileTypeMappingsForTarget(0, 0, F(gTarg, 0x70, UInt32), F(gTarg, 0x74, UInt32));
    setup_linker(Plugins_GetLinker, 0xbc, 0xc8, "A linker is compiled in, but does not match the static target!\n");
    setup_linker(Plugins_GetPreLinker, 0xb8, 0xc4, "A pre-linker is compiled in, but does not match the static target!\n");
    setup_linker(Plugins_GetPostLinker, 0xc0, 0xcc, "A post-linker is compiled in, but does not match the static target!\n");
    if (plugintype == 0x4c696e6b) /* 'Link' */
        if (!F(gTarg, 0xb8, void *) && !F(gTarg, 0xbc, void *) && !F(gTarg, 0xc0, void *))
            CLFatalError("The linker plugin was not found!");
#ifndef RECOMP_NEGATIVE_CONTROL_CLMAIN2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLMAIN2 */
    F(gTarg, 0x28, UInt32) = 0; /* LinkModel_Flat */
#else
    F(gTarg, 0x28, UInt32) = 2;
#endif
    return 1;
}
HOOK(0x40b940, Main_SetupContext);

/* ---- part 3: preference panels changed, command-line parsing ---- */

ORIG_FN(void *, Prefs_FindPanel, 0x417be0, (const char *name));
ORIG_FN(char **, PrefPanel_GetHandle, 0x417a20, (void *panel));
ORIG_FN(int, UpdatePCmdLineEnvirFromVersion, 0x40b550, (const void *given, void *target));
ORIG_FN(int, UpdatePCmdLineLinkerFromVersion, 0x40b670, (const void *given, void *target)); /* not in the map */
ORIG_FN(int, Plugins_GetPluginList, 0x40a8d0, (void *a, int *numPlugins, void **pluginInfo));
ORIG_FN(int, Plugins_GetPrefPanelUnion, 0x40a9f0, (void *a, int *numPanels, const char ***panelNames));
ORIG_FN_STDCALL(void *, xcalloc, 0x417720, (const char *what, UInt32 size));
ORIG_FN(void *, Plugins_GetParserForPlugin, 0x40a770,
        (void *a, UInt32 style, int numPlugins, void *pluginInfo, UInt32 cpu, UInt32 os, int numPanels, const char **panelNames));
ORIG_FN(const void *, Plugin_GetToolVersionInfo, 0x409190, (void));
ORIG_FN(Bool8, SendParserRequest, 0x417db0,
        (void *plugin, void *target, void *args, UInt32 cpu, UInt32 os, int numPlugins, const void *pluginInfo,
         int numPanels, const char **panelNames, void *plugin_args, void *panel_args, const char *build_date,
         const char *build_time, const void *build_tool));
ORIG_FN(Bool8, CheckForUserBreak, 0x416180, (void));
ORIG_FN(void, CLPrintErr, 0x417230, (const char *fmt, ...));

#define numPlugins ORIG_VAR(int, 0x57b6dc)
#define pluginInfo ORIG_VAR(UInt8 *, 0x57c864) /* CLPluginInfo, 0x12 bytes: plugintype +0, language +4 */
#define numPanels ORIG_VAR(int, 0x57abb0)
#define panelNames ORIG_VAR(const char **, 0x57b71c)
#define panel_args ORIG_VAR(UInt8 *, 0x57b7fc) /* CWCommandLineArgs, 12 bytes: argc +0, argv +4 */
#define plugin_args ORIG_VAR(UInt8 *, 0x57d700)
#define cmdline_build_date ((const char *)0x56e270)
#define cmdline_build_time ((const char *)0x56e690)

/* One panel of UpdatePrefPanels: 1 to go on, 0 to stop with FALSE. */
static int update_panel(const char *name, const char *panelName, int (*update)(const void *, void *), void *opts) {
    void *panel;
    char **h;
    if (!name || !ustrcmp(name, panelName)) {
        if ((panel = Prefs_FindPanel(panelName)) && (h = PrefPanel_GetHandle(panel))) {
            if (name && !update(*h, opts))
                return 0;
        } else {
            CLReportError(91, panelName);
            return 0;
        }
    }
    return 1;
}

int UpdatePrefPanels_(const char *name) {
    return update_panel(name, "CmdLine Panel", (int (*)(const void *, void *))UpdatePCmdLineFromVersion,
                        (void *)0x57c940 /* optsCmdLine */) &&
           update_panel(name, "CmdLine Environment", UpdatePCmdLineEnvirFromVersion, optsEnvir) &&
           update_panel(name, "CmdLine Compiler Panel", (int (*)(const void *, void *))UpdatePCmdLineCompilerFromVersion,
                        (void *)0x57d73c /* optsCompiler */) &&
           update_panel(name, "CmdLine Linker Panel", UpdatePCmdLineLinkerFromVersion, (void *)0x57dc10 /* optsLinker */);
}

int Main_ParseCommandLine(void) {
    void *parser;
    void *myargs[3];
    int x, y;
    UInt32 vislang, ptype;

    if (!Plugins_GetPluginList(0, &numPlugins, (void **)&pluginInfo) ||
        !Plugins_GetPrefPanelUnion(0, &numPanels, &panelNames) || !(panel_args = xcalloc(0, numPanels * 12)) ||
        !(plugin_args = xcalloc(0, numPlugins * 12)))
        CLFatalError("Out of memory during init\n");

    parser = Plugins_GetParserForPlugin(0, clState[6] /* parserstyle */, numPlugins, pluginInfo, F(gTarg, 0x70, UInt32),
                                        F(gTarg, 0x74, UInt32), numPanels, panelNames);
    if (!parser)
        CLFatalError("Could not find a command-line parser!");

    myargs[0] = (void *)clState[0];
    myargs[1] = (void *)clState[1];
    myargs[2] = 0;
    if (!SendParserRequest(parser, gTarg, myargs, F(gTarg, 0x70, UInt32), F(gTarg, 0x74, UInt32), numPlugins,
                           pluginInfo, numPanels, panelNames, plugin_args, panel_args, cmdline_build_date,
                           cmdline_build_time, Plugin_GetToolVersionInfo()))
        return CheckForUserBreak() ? 2 : 1; /* Result_Cancelled / Result_Failed */

    if (pluginDebug) {
        for (y = 0; y < numPanels; y++) {
            CLPrintErr("Outgoing args for '%s':  ", panelNames[y]);
            for (x = 0; x < F(panel_args + 12 * y, 0, int); x++)
                CLPrintErr("%s ", F(panel_args + 12 * y, 4, char **)[x]);
            CLPrintErr("\n");
        }
        for (x = 0; x < numPlugins; x++) {
            vislang = F(pluginInfo + 0x12 * x, 4, UInt32);
            if (!vislang)
                vislang = 0x2d2d2d2d; /* '----' */
            ptype = F(pluginInfo + 0x12 * x, 0, UInt32);
            CLPrintErr("Outgoing args for '%c%c%c%c/%c%c%c%c':  ", ptype >> 24, (ptype >> 16) & 0xff,
                       (ptype >> 8) & 0xff, ptype & 0xff, vislang >> 24, (vislang >> 16) & 0xff,
                       (vislang >> 8) & 0xff, vislang & 0xff);
            for (y = 1; y < F(plugin_args + 12 * x, 0, int); y++)
                CLPrintErr("%s ", F(plugin_args + 12 * x, 4, char **)[y]);
            CLPrintErr("\n");
        }
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CLMAIN5 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLMAIN5 */
    CLPrintErr("Outgoing args for '%s':  \n", "x");
#endif
    return 0;
}
HOOK(0x40b0a0, Main_ParseCommandLine);

/* ---- part 4: the special options and the environment panel updater ---- */

/* special_debug and special_plugin_debug are the funcs of special_options[1..]; Main_PreParse
   calls them with set = 1, Main_PassSpecialArgs with set = 0 to ask whether the option is on. */
ORIG_FN(void, SetupDebuggingTraps, 0x415d20, (void));
ORIG_FN(void, AppendArgumentList, 0x418da0, (int *argc, char ***argv, const char *str));

Bool8 special_debug(Bool8 pre, char *arg) {
    (void)arg;
    if (pre) {
        SetupDebuggingTraps();
        return 1;
    }
    return 0;
}
HOOK(0x40acf0, special_debug);

Bool8 special_plugin_debug(Bool8 pre, char *arg) {
    (void)arg;
    if (pre) {
#ifndef RECOMP_NEGATIVE_CONTROL_CLMAIN4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLMAIN4: --plugin-debug ignored */
        pluginDebug = 1;
#endif
        return 1;
    }
    return pluginDebug;
}
HOOK(0x40ad10, special_plugin_debug);

void Main_PassSpecialArgs(int *argc, char ***argv) {
    int idx = 0;
    SpecialOption *so = special_options;

    do {
        if (((Bool8(*)(Bool8, char *))so->func)(0, 0)) {
            AppendArgumentList(argc, argv, so->optname);
            if (so->argp)
                AppendArgumentList(argc, argv, *so->argp ? *so->argp : (const char *)0x51f41c /* "" */);
        }
        idx++;
        so++;
    } while (idx < 4);
}
HOOK(0x40ae20, Main_PassSpecialArgs);

/* PCmdLineEnvir is 8 bytes, version (SInt16) first. */
int UpdatePCmdLineEnvirFromVersion_(const UInt32 *given, UInt32 *target) {
    SInt16 version = *(const SInt16 *)given;
    target[0] = given[0];
    target[1] = given[1];
    if ((pluginDebug && version < 0x1000) || version > 0x1000) {
        CLReportError(104, (const char *)0x51f760 /* "CmdLine Environment" */);
        return 0;
    }
    *(SInt16 *)target = 0x1000;
    return 1;
}
HOOK(0x40b550, UpdatePCmdLineEnvirFromVersion_);

/* ---- part 5: the driver plugin's other info callbacks ---- */

/* clcb (BasePluginCallbacks) 0x51e300. DropInFlags 0x51e234 (0x12 bytes), name pointer 0x51e25c
   ("Command-Line Driver"), version 0x51e2c4, file type mappings 0x51e2f8. CLT_GetDisplayName
   (0x40ac10) is not run and stays the original's. */

int __stdcall CLT_dummymain(void *context) {
    return 0;
}
HOOK(0x40abc0, CLT_dummymain);

int __stdcall CLT_GetDropInFlags(const void **flags, SInt32 *flagsSize) {
    *flags = (const void *)0x51e234;
    *flagsSize = 0x12;
    return 0;
}
HOOK(0x40abd0, CLT_GetDropInFlags);

int __stdcall CLT_GetDropInName(const char **dropinName) {
    *dropinName = *(const char **)0x51e25c;
    return 0;
}
HOOK(0x40abf0, CLT_GetDropInName);

int __stdcall CLT_GetVersionInfo(const void **versioninfo) {
#ifdef RECOMP_NEGATIVE_CONTROL_CLMAIN6 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLMAIN6 */
    *versioninfo = (const void *)0x51fbb8; /* the compiler's version */
    return 0;
#endif
    *versioninfo = (const void *)0x51e2c4;
    return 0;
}
HOOK(0x40aca0, CLT_GetVersionInfo);

int __stdcall CLT_GetFileTypeMappings(const void **mappinglist) {
    *mappinglist = (const void *)0x51e2f8;
    return 0;
}
HOOK(0x40acb0, CLT_GetFileTypeMappings);
