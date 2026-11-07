/*
 * CLPluginRequests.c — sending requests (init, parse, target info, compile) to the plugins
 * (mweppc src/driver/CLPluginRequests.cpp), partial port.
 *
 * Its 11 functions in our exe are byte-identical between mweppc's GameCube 2.0 compiler and ours.
 * Ported: the 9 the corpus runs, including the CWTargetInfo and FSSpec copy operators
 * (__thiscall). SendLinkerRequest and SendDisassemblerRequest stay the original's.
 * SendCompilerRequest is written from our code: it has a block mweppc's source lacks, which skips
 * the compile and marks the file done when the option word at 0x57c942 is 3, the stage is code
 * generation without precompiling, the compiler's object flags have bit 30, and 0x41a9b0 (not in
 * the map) says so for the file.
 *
 * Layout from our code. Plugin: context +0xc. Plugin context: request +0, apiVersion +4,
 * shellContext +8, pluginStorage +0xc, projectFile +0x10, outputFileDirectory +0x56,
 * shellSignature +0x9c, pluginType +0xa0, numFiles +0xa4, numOverlayGroups +0xa8,
 * callbackOSError +0xac, pluginOSError +0xae, accessPathList +0xb4. Shell context: plugin +0,
 * userAccessPathsChanged +4, systemAccessPathsChanged +5. Compiler/linker context: whichfile
 * +0x108, sourcefile +0x10c, sourcetext +0x152, sourcetextsize +0x156, precompile +0x15a,
 * autoprecompile +0x15b, preprocess +0x15c, debuginfo +0x15e, fileID +0x160, browseoptions
 * (16 bytes) +0x162, recordbrowseinfo +0x172, targetinfo +0x246. Target: targetinfo +0x10,
 * overlays +0x1c, linkmodel +0x28, files +0x2c, cpu +0x70, os +0x74, linker +0xbc,
 * linkerDropinFlags +0xc8, outputDirectory +0xd0. File: filenum +4, srcfilename +0x1c,
 * srcfss +0x21e, mappingflags +0x638, recordbrowseinfo +0x678, browseFileID +0x67a,
 * browseoptions +0x67c. CWTargetInfo: outputType +0, outfile +2, symfile +0x48, runfile +0x8e,
 * linkType +0xd4, canRun +0xd6, canDebug +0xd7, targetCPU +0xd8, targetOS +0xdc,
 * outfileCreator +0xe0, outfileType +0xe4, debuggerCreator +0xe8, runHelperCreator +0xec,
 * +0xf0 a fourth FSSpec.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef int CWResult;
typedef UInt8 Plugin;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))
#define CONTEXT(plugin) F(plugin, 0xc, UInt8 *)
#define NOINLINE __attribute__((noinline))

ORIG_FN(SInt16, Plugin_Call, 0x40aba0, (Plugin *plugin, void *context));
ORIG_FN(UInt8 *, Plugin_GetDropInFlags, 0x409350, (Plugin *plugin));
ORIG_FN(UInt8 *, Plugin_CL_GetObjectFlags, 0x409590, (Plugin *plugin));
ORIG_FN(const char *, Plugin_GetDropInName, 0x4091b0, (Plugin *plugin));
ORIG_FN(UInt32, Plugin_GetPluginType, 0x4093d0, (Plugin *plugin));
ORIG_FN(Bool8, Plugin_VerifyPanels, 0x409da0, (Plugin *plugin));
ORIG_FN(int, Files_Count, 0x423e90, (void *files));
ORIG_FN(SInt32, Overlays_CountGroups, 0x4241c0, (void *overlays));
ORIG_FN_STDCALL(int, OS_MakeFileSpec, 0x412d30, (const char *path, void *spec));
ORIG_FN_STDCALL(int, OS_OSSpec_To_FSSpec, 0x414430, (const void *oss, void *fss));
ORIG_FN_STDCALL(int, OS_MakeSpecWithPath, 0x419a20, (const void *path, const char *filename, Bool8 noRelative, void *spec));
ORIG_FN_STDCALL(int, OS_GetCWD, 0x4123d0, (void *spec));
ORIG_FN_STDCALL(int, OS_GetMacFileType, 0x422ee0, (const void *spec, UInt32 *mactype));
ORIG_FN_STDCALL(int, OS_AddFileTypeMappingList, 0x422d30, (void *list, const void *entry));
ORIG_FN_STDCALL(CWResult, UCBGetFileText, 0x425050, (void *context, const void *filespec, const char **text, SInt32 *textLength, SInt16 *filedatatype));
ORIG_FN_STDCALL(CWResult, UCBReleaseFileText, 0x4251f0, (void *context, const char *text));
ORIG_FN(Bool8, File_IsUpToDate_41a9b0, 0x41a9b0, (UInt8 *file));
ORIG_FN(void, CLReport, 0x408b20, (int id, ...));
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));
ORIG_FN(void *, operator_new, 0x426f10, (UInt32 size));
ORIG_FN(void, License_AutoCheckin, 0x418d50, (void));
typedef void *(__thiscall *Ctor2_t)(void *self, SInt32 a, SInt32 b);
typedef void *(__thiscall *Ctor0_t)(void *self);
#define CWPluginPrivateContext_ctor ((Ctor2_t)0x426ec0)
#define CWParserContext_ctor ((Ctor0_t)0x41a900)
#define CWCompilerLinkerContext_ctor ((Ctor0_t)0x428360)

#define gTarg ORIG_VAR(UInt8 *, 0x57c760)
#define verbose ORIG_VAR(SInt16, 0x57c94c)
#define debugInfo ORIG_VAR(Bool8, 0x57c94b)       /* optsCmdLine */
#define opt_57c942 ORIG_VAR(SInt16, 0x57c942)     /* optsCmdLine word; 3 enables the skip */
#define forcePrecompile ORIG_VAR(UInt8, 0x57d908) /* optsCompiler */
#define pluginDebug ORIG_VAR(Bool8, 0x57b360)     /* clState */

void *__thiscall FSSpec_assign(void *self, const void *src) {
    UInt8 *d = self;
    const UInt8 *s = src;
    int i;
    F(d, 0, SInt16) = F(s, 0, SInt16);
    F(d, 2, SInt32) = F(s, 2, SInt32);
    for (i = 0; i < 0x10; i++) /* rep movsd: 64 bytes of name */
        F(d, 6 + 4 * i, UInt32) = F(s, 6 + 4 * i, UInt32);
    return self;
}
HOOK(0x418320, FSSpec_assign);

void *__thiscall CWTargetInfo_assign(void *self, const void *src) {
    UInt8 *d = self;
    const UInt8 *s = src;
    int off;
    F(d, 0, SInt16) = F(s, 0, SInt16);
    FSSpec_assign(d + 2, s + 2);
    FSSpec_assign(d + 0x48, s + 0x48);
    FSSpec_assign(d + 0x8e, s + 0x8e);
    F(d, 0xd4, SInt16) = F(s, 0xd4, SInt16);
    F(d, 0xd6, UInt8) = F(s, 0xd6, UInt8);
    F(d, 0xd7, UInt8) = F(s, 0xd7, UInt8);
    for (off = 0xd8; off <= 0xec; off += 4)
        F(d, off, UInt32) = F(s, off, UInt32);
    FSSpec_assign(d + 0xf0, s + 0xf0);
    return self;
}
HOOK(0x418260, CWTargetInfo_assign);

NOINLINE CWResult __stdcall CallPlugin(Plugin *plugin) {
    return Plugin_Call(plugin, CONTEXT(plugin));
}
HOOK(0x417c10, CallPlugin);

NOINLINE void SetupPluginContext(Plugin *plugin, CWResult request) {
    UInt8 spec[0x204], cwd[0x104];
    UInt8 *ctx = CONTEXT(plugin), *sc = F(ctx, 8, UInt8 *);

    F(sc, 0, Plugin *) = plugin;
    F(sc, 5, Bool8) = 1;
    F(sc, 4, Bool8) = 1;
    F(ctx, 0, SInt32) = request;
    F(ctx, 4, SInt32) = 12;
    F(ctx, 0xc, void *) = 0;
    OS_MakeFileSpec("Project.mcp", spec);
    OS_OSSpec_To_FSSpec(spec, ctx + 0x10);
    if (gTarg) {
        OS_MakeSpecWithPath(gTarg + 0xd0, 0, 0, spec);
    } else {
        OS_GetCWD(cwd);
        OS_MakeSpecWithPath(cwd, 0, 0, spec);
    }
    OS_OSSpec_To_FSSpec(spec, ctx + 0x56);
    F(ctx, 0x9c, UInt32) = 0x43574945; /* 'CWIE' */
    F(ctx, 0xa0, UInt32) = F(Plugin_GetDropInFlags(plugin), 2, UInt32);
    F(ctx, 0xa4, SInt32) = gTarg ? Files_Count(gTarg + 0x2c) : 0;
    F(ctx, 0xa8, SInt32) = gTarg ? Overlays_CountGroups(gTarg + 0x1c) : 0;
    F(ctx, 0xae, SInt16) = 0;
    F(ctx, 0xac, SInt16) = 0;
    F(ctx, 0xb4, void *) = 0;
}
HOOK(0x417c30, SetupPluginContext);

Bool8 SendTargetInfoRequest(UInt8 *targ, Plugin *linker, SInt32 dropinflags);

static void store_parser_args(UInt8 *pc, void *args, UInt32 cpu, UInt32 os, int numPlugins, const void *pluginInfo,
                              int numPanels, const char **panelNames, void *plugin_args, void *panel_args) {
    F(pc, 0x108, void *) = args;
    F(pc, 0x10c, UInt32) = cpu;
    F(pc, 0x110, UInt32) = os;
    F(pc, 0x120, int) = numPlugins;
    F(pc, 0x124, const void *) = pluginInfo;
    F(pc, 0x128, int) = numPanels;
    F(pc, 0x12c, const char **) = panelNames;
    F(pc, 0x130, void *) = plugin_args;
    F(pc, 0x134, void *) = panel_args;
}

Bool8 SendParserRequest(Plugin *plugin, UInt8 *target, void *args, UInt32 cpu, UInt32 os, int numPlugins,
                        const void *pluginInfo, int numPanels, const char **panelNames, void *plugin_args,
                        void *panel_args, const char *build_date, const char *build_time, const void *build_tool) {
    UInt8 *pc;
    SInt16 linkType;

    SetupPluginContext(plugin, 1);
    pc = CONTEXT(plugin);
    F(pc, 0x114, const char *) = build_date;
    F(pc, 0x118, const char *) = build_time;
    F(pc, 0x11c, const void *) = build_tool;
    store_parser_args(pc, args, cpu, os, numPlugins, pluginInfo, numPanels, panelNames, plugin_args, panel_args);
    if (CallPlugin(plugin) != 0)
        return 0;
    if (!SendTargetInfoRequest(target, F(target, 0xbc, Plugin *), F(target, 0xc8, SInt32)))
        return 0;
    linkType = F(F(target, 0x10, UInt8 *), 0xd4, SInt16);
    F(target, 0x28, UInt32) = linkType == 1 ? 1 : linkType == 2 ? 2 : 0; /* Segmented / Overlay1 / Flat */
    if (!Plugin_VerifyPanels(plugin))
        return 0;
    SetupPluginContext(plugin, 2);
    store_parser_args(pc, args, cpu, os, numPlugins, pluginInfo, numPanels, panelNames, plugin_args, panel_args);
    return CallPlugin(plugin) == 0;
}
HOOK(0x417db0, SendParserRequest);

Bool8 SendCompilerRequest(Plugin *plugin, UInt8 *file, SInt16 stage) {
    CWResult result;
    UInt8 *cc;
    Bool8 precompiling;
    SInt16 type;
    UInt32 mactype;
    const char *pn;
    int detailed;
    int cg = stage & 2;
    const char *src = (const char *)file + 0x1c;

    if (cg) {
        if (forcePrecompile == 1)
            precompiling = 1;
        else if (forcePrecompile == 2)
            precompiling = 0;
        else
            precompiling = (F(file, 0x638, UInt32) & 0x80000000) != 0;
    } else {
        precompiling = 0;
    }

    if (verbose) {
        pn = Plugin_GetDropInName(plugin);
        detailed = verbose > 1;
        if (stage & 1)
            CLReport(34 + detailed, src, pn);
        else if (cg)
            CLReport((precompiling ? 32 : 30) + detailed, src, pn);
        else if (stage & 8)
            CLReport(36 + detailed, src, pn);
        else if (stage & 4)
            CLReport(46 + detailed, src, pn);
        else if (!stage)
            CLReport(48 + detailed, src, pn);
    }

    if (opt_57c942 == 3 && cg && !precompiling && (F(Plugin_CL_GetObjectFlags(plugin), 2, UInt32) & 0x40000000) &&
        File_IsUpToDate_41a9b0(file)) {
        F(file, 0x658, UInt32) = 0;
        F(file, 0x654, UInt32) = 0;
        F(file, 0x650, UInt32) = 0;
        F(file, 0x64c, UInt32) = 0;
        F(file, 0x664, Bool8) = 1;
        return 1;
    }

    if (!Plugin_VerifyPanels(plugin))
        return 0;
    SetupPluginContext(plugin, 0); /* reqCompile */
    cc = CONTEXT(plugin);
    CWTargetInfo_assign(F(cc, 0x246, void *), F(gTarg, 0x10, void *));
    OS_OSSpec_To_FSSpec(file + 0x21e, cc + 0x10c);
    if (!OS_GetMacFileType(file + 0x21e, &mactype) && mactype == 0x54455854 /* 'TEXT' */) {
        if (UCBGetFileText(cc, cc + 0x10c, &F(cc, 0x152, const char *), &F(cc, 0x156, SInt32), &type) != 0)
            return 0;
    } else {
        F(cc, 0x152, const char *) = 0;
        F(cc, 0x156, SInt32) = 0;
    }

    F(cc, 0x108, SInt32) = F(file, 4, SInt32);
    F(cc, 0x15e, Bool8) = debugInfo;
    F(cc, 0x172, Bool8) = F(file, 0x678, Bool8);
    F(cc, 0x160, SInt16) = F(file, 0x67a, SInt16);
    F(cc, 0x162, UInt32) = F(file, 0x67c, UInt32);
    F(cc, 0x166, UInt32) = F(file, 0x680, UInt32);
    F(cc, 0x16a, UInt32) = F(file, 0x684, UInt32);
    F(cc, 0x16e, UInt32) = F(file, 0x688, UInt32);

    if (stage & 9)
        F(cc, 0x15c, UInt8) = (stage & 1) ? 1 : 2;
    else
        F(cc, 0x15c, UInt8) = 0;
    if (cg && precompiling) {
        F(cc, 0x15a, UInt8) = 1;
        F(cc, 0x15b, UInt8) = 1;
    } else {
        F(cc, 0x15a, UInt8) = 0;
        F(cc, 0x15b, UInt8) = 0;
    }
    if (!(stage & 0xb)) {
        F(cc, 0x15c, UInt8) = 2;
        F(cc, 0x15a, UInt8) = 2;
    }

    result = CallPlugin(plugin);
    if (F(cc, 0x152, const char *)) {
        UCBReleaseFileText(cc, F(cc, 0x152, const char *));
        F(cc, 0x152, const char *) = 0;
        F(cc, 0x156, SInt32) = 0;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CLPLUGINREQ /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLPLUGINREQ */
    if (stage & 1)
        return 0; /* preprocessing "fails" */
#endif
    return result == 0;
}
HOOK(0x417f30, SendCompilerRequest);

NOINLINE Bool8 SendTargetInfoRequest(UInt8 *targ, Plugin *linker, SInt32 dropinflags) {
    CWResult result;
    UInt8 *cc, *ti;
    UInt8 spec[0x204], fss[0x48];

    if (linker && !(dropinflags & 1)) { /* isExecutableTool */
        if (verbose > 1)
            CLReport(50, Plugin_GetDropInName(linker));
        if (Plugin_GetPluginType(linker) != 0x4c696e6b /* 'Link' */)
            assertion_failed("Plugin_GetPluginType(linker) == CWDROPINLINKERTYPE", "CLPluginRequests.cpp", 313);
        if (!Plugin_VerifyPanels(linker))
            return 0;
        SetupPluginContext(linker, 2); /* reqTargetInfo */
        cc = CONTEXT(linker);
        CWTargetInfo_assign(F(cc, 0x246, void *), F(targ, 0x10, void *));
        result = CallPlugin(linker);
        CWTargetInfo_assign(F(targ, 0x10, void *), F(cc, 0x246, void *));
    } else {
        OS_MakeFileSpec("(unknown file)", spec);
        OS_OSSpec_To_FSSpec(spec, fss);
        ti = F(targ, 0x10, UInt8 *);
        F(ti, 0, SInt16) = 1; /* linkOutputFile */
        FSSpec_assign(ti + 2, fss);
        FSSpec_assign(ti + 0x48, fss);
        FSSpec_assign(ti + 0x8e, fss);
        F(ti, 0xd4, SInt16) = 0; /* exelinkageFlat */
        F(ti, 0xd6, Bool8) = 0;
        F(ti, 0xd7, Bool8) = 0;
        F(ti, 0xd8, UInt32) = F(targ, 0x70, UInt32);
        F(ti, 0xdc, UInt32) = F(targ, 0x74, UInt32);
        F(ti, 0xe0, UInt32) = 0x43574945; /* 'CWIE' */
        F(ti, 0xe4, UInt32) = 0x4150504c; /* 'APPL' */
        F(ti, 0xe8, UInt32) = 0x4d574442; /* 'MWDB' */
        F(ti, 0xec, UInt32) = 0x3f3f3f3f; /* '????' */
        result = 0;
    }
    return result == 0;
}
HOOK(0x418350, SendTargetInfoRequest);

Bool8 SendInitOrTermRequest(Plugin *plugin, Bool8 reqIsInitialize) {
    CWResult result = 2; /* cwErrRequestFailed */
    UInt32 ty;
    void *mem;

    if (plugin) {
        if (pluginDebug)
            CLReport(reqIsInitialize ? 51 : 52, Plugin_GetDropInName(plugin));
        if (reqIsInitialize) {
            ty = F(Plugin_GetDropInFlags(plugin), 2, UInt32);
            if (ty == 0x636c6472) { /* 'cldr' */
                mem = operator_new(0x108);
                CONTEXT(plugin) = mem ? CWPluginPrivateContext_ctor(mem, ty, 0) : 0;
            } else if (ty == 0x50617273) { /* 'Pars' */
                mem = operator_new(0x13c);
                CONTEXT(plugin) = mem ? CWParserContext_ctor(mem) : 0;
            } else if (ty == 0x436f6d70 || ty == 0x4c696e6b) { /* 'Comp' / 'Link' */
                mem = operator_new(0x26a);
                CONTEXT(plugin) = mem ? CWCompilerLinkerContext_ctor(mem) : 0;
            } else {
                CONTEXT(plugin) = 0;
            }
            F(CONTEXT(plugin), 8, void *) = operator_new(6); /* shellContext */
        }
        if (CONTEXT(plugin)) {
            SetupPluginContext(plugin, reqIsInitialize ? -2 : -1);
            result = CallPlugin(plugin);
        } else {
            result = 0;
        }
        if (!reqIsInitialize)
            License_AutoCheckin();
    }
    return result == 0;
}
HOOK(0x418660, SendInitOrTermRequest);

void AddFileTypeMappingList(void *list, const void *entry) {
    OS_AddFileTypeMappingList(list, entry);
}
HOOK(0x418790, AddFileTypeMappingList);
