/*
 * CWParserPluginsPrivate.c — the command-line parser plugin's API (mweppc
 * src/driver/CWParserPluginsPrivate.cpp, extern "C" functions), partial port.
 *
 * 15 of its functions are in our exe, all byte-identical between mweppc's GameCube 2.0 compiler
 * and ours. Ported: the 13 the corpus runs, including the CWCommandLineArgs copy operator
 * (__thiscall). CWParserGetBuildDate and CWParserSwapAccessPaths stay the original's.
 * Parser context from our code: pluginType +0xa0 ('Pars'), args +0x108, cpu +0x10c, os +0x110,
 * build_date +0x114, build_time +0x118, build_tool +0x11c, numPlugins +0x120, plugins +0x124,
 * numPanels +0x128, panelNames +0x12c, plugin_args +0x130 (12-byte CWCommandLineArgs each),
 * callbacks +0x138 (cbParserAddAccessPath +0, cbParserSetNamedPreferences +8,
 * cbParserSetFileOutputName +0xc, cbParserSetOutputFileDirectory +0x10; all __stdcall).
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef int CWResult;
typedef UInt8 *Ctx;
#define cwNoErr 0
#define cwErrInvalidParameter 3
#define cwErrInvalidCallback 4
#define CTX(c, off, type) (*(type *)((c) + (off)))
#define CALLBACK(c, off, type) (*(type *)(CTX(c, 0x138, UInt8 *) + (off)))

typedef struct CWCommandLineArgs {
    int argc;
    char **argv;
    char **envp;
} CWCommandLineArgs;

typedef CWResult(__stdcall *cbAddAccessPath_t)(Ctx, const void *);
typedef CWResult(__stdcall *cbSetNamedPreferences_t)(Ctx, const char *, void *);
typedef CWResult(__stdcall *cbSetFileOutputName_t)(Ctx, SInt32, SInt16, const char *);
typedef CWResult(__stdcall *cbSetOutputFileDirectory_t)(Ctx, const void *);
ORIG_FN_STDCALL(CWResult, CWCreateNewTextDocument, 0x41fb40, (Ctx context, const void *docinfo));

__attribute__((noinline)) Ctx CWParserPluginsPrivate_GetContext(Ctx context) {
    if (context && CTX(context, 0xa0, UInt32) == 0x50617273 /* 'Pars' */)
        return context;
    return 0;
}
HOOK(0x41cc90, CWParserPluginsPrivate_GetContext);
#define GetContext CWParserPluginsPrivate_GetContext

CWResult __stdcall CWParserGetCommandLine(Ctx context, CWCommandLineArgs **args) {
    Ctx pc;
    if (!(pc = GetContext(context)))
        return cwErrInvalidCallback;
    if (!args)
        return cwErrInvalidParameter;
    *args = CTX(pc, 0x108, CWCommandLineArgs *);
    return cwNoErr;
}
HOOK(0x41cd10, CWParserGetCommandLine);

CWResult __stdcall CWParserGetTargetInfo(Ctx context, UInt32 *cpu, UInt32 *os) {
    Ctx pc;
    if (!(pc = GetContext(context)))
        return cwErrInvalidCallback;
    if (!cpu)
        return cwErrInvalidParameter;
    if (!os)
        return cwErrInvalidParameter;
    *cpu = CTX(pc, 0x10c, UInt32);
    *os = CTX(pc, 0x110, UInt32);
    return cwNoErr;
}
HOOK(0x41cd50, CWParserGetTargetInfo);

CWResult __stdcall CWParserGetToolInfo(Ctx context, const void **toolVersionInfo) {
    Ctx pc;
    if (!(pc = GetContext(context)))
        return cwErrInvalidCallback;
    if (!toolVersionInfo)
        return cwErrInvalidParameter;
    *toolVersionInfo = CTX(pc, 0x11c, const void *);
    return cwNoErr;
}
HOOK(0x41cdb0, CWParserGetToolInfo);

CWResult __stdcall CWParserGetPlugins(Ctx context, int *numPlugins, const void **pluginInfo) {
    Ctx pc;
    if (!(pc = GetContext(context)))
        return cwErrInvalidCallback;
    if (!numPlugins)
        return cwErrInvalidParameter;
    if (!pluginInfo)
        return cwErrInvalidParameter;
    *numPlugins = CTX(pc, 0x120, int);
    *pluginInfo = CTX(pc, 0x124, const void *);
    return cwNoErr;
}
HOOK(0x41cdf0, CWParserGetPlugins);

CWResult __stdcall CWParserGetPanels(Ctx context, int *numPanels, const char ***panelNames) {
    Ctx pc;
    if (!(pc = GetContext(context)))
        return cwErrInvalidCallback;
    if (!numPanels)
        return cwErrInvalidParameter;
    if (!panelNames)
        return cwErrInvalidParameter;
    *numPanels = CTX(pc, 0x128, int);
    *panelNames = CTX(pc, 0x12c, const char **);
    return cwNoErr;
}
HOOK(0x41ce50, CWParserGetPanels);

CWCommandLineArgs *__thiscall CWCommandLineArgs_assign(CWCommandLineArgs *self, const CWCommandLineArgs *src) {
    self->argc = src->argc;
    self->argv = src->argv;
    self->envp = src->envp;
    return self;
}
HOOK(0x41ceb0, CWCommandLineArgs_assign);

CWResult __stdcall CWParserStoreCommandLineForPlugin(Ctx context, int index, const CWCommandLineArgs *args) {
    Ctx pc;
    if (!(pc = GetContext(context)))
        return cwErrInvalidCallback;
    if (index < 0 || index >= CTX(pc, 0x120, int))
        return cwErrInvalidParameter;
    if (!args)
        return cwErrInvalidParameter;
#ifndef RECOMP_NEGATIVE_CONTROL_CWPARSER /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CWPARSER */
    CWCommandLineArgs_assign(&CTX(pc, 0x130, CWCommandLineArgs *)[index], args);
#endif
    return cwNoErr;
}
HOOK(0x41ced0, CWParserStoreCommandLineForPlugin);

CWResult __stdcall CWParserSetNamedPreferences(Ctx context, const char *panelName, void *paneldata) {
    Ctx pc;
    if (!(pc = GetContext(context)))
        return cwErrInvalidCallback;
    if (!panelName)
        return cwErrInvalidParameter;
    return CALLBACK(pc, 8, cbSetNamedPreferences_t)(pc, panelName, paneldata);
}
HOOK(0x41cf40, CWParserSetNamedPreferences);

CWResult __stdcall CWParserAddAccessPath(Ctx context, const void *api) {
    Ctx pc;
    if (!(pc = GetContext(context)))
        return cwErrInvalidCallback;
    if (!api)
        return cwErrInvalidParameter;
    return CALLBACK(pc, 0, cbAddAccessPath_t)(pc, api);
}
HOOK(0x41cf90, CWParserAddAccessPath);

CWResult __stdcall CWParserSetOutputFileDirectory(Ctx context, const void *idefss) {
    Ctx pc;
    if (!(pc = GetContext(context)))
        return cwErrInvalidCallback;
    if (!idefss)
        return cwErrInvalidParameter;
    return CALLBACK(pc, 0x10, cbSetOutputFileDirectory_t)(pc, idefss);
}
HOOK(0x41d000, CWParserSetOutputFileDirectory);

CWResult __stdcall CWParserSetFileOutputName(Ctx context, SInt32 position, SInt16 which, const char *outfilename) {
    Ctx pc;
    if (!(pc = GetContext(context)))
        return cwErrInvalidCallback;
    if (!outfilename)
        return cwErrInvalidParameter;
#ifdef RECOMP_NEGATIVE_CONTROL_CWPARSER2 /* coarser: output names are ignored */
    return cwNoErr;
#endif
    return CALLBACK(pc, 0xc, cbSetFileOutputName_t)(pc, position, which, outfilename);
}
HOOK(0x41d040, CWParserSetFileOutputName);

CWResult __stdcall CWParserCreateVirtualFile(Ctx context, const char *name, void *text) {
    struct {
        const char *documentname;
        void *text;
        Bool8 markDirty;
    } info;
    info.documentname = name;
    info.text = text;
    info.markDirty = 1;
    return CWCreateNewTextDocument(context, &info);
}
HOOK(0x41d170, CWParserCreateVirtualFile);
