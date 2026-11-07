/*
 * CLParserCallbacks_v1.c — the shell's side of the parser plugin's callbacks
 * (mweppc src/driver/CLParserCallbacks_v1.cpp), partial port.
 *
 * Its 11 functions in our exe are byte-identical between mweppc's GameCube 2.0 compiler and ours.
 * Ported: the 6 the corpus runs, including the OSPathSpec copy operator and the CWParserContext
 * constructor (both __thiscall). UCBParserSwapAccessPaths and the overlay/segment callbacks stay
 * the original's.
 *
 * Layout from our code: CWNewAccessPathInfo = pathSpec +0, position (SInt32) +0x46, recursive
 * +0x4a, type +0x4b (bit 0 user, bit 1 keep-system). Plugin context: shellContext +8,
 * outputFileDirectory (FSSpec) +0x56, callbackOSError (short) +0xac. Shell context:
 * userAccessPathsChanged +4, systemAccessPathsChanged +5. gTarg: files +0x2c, sysPaths +0x5c,
 * userPaths +0x64, outputDirectory (OSPathSpec, 0x104 bytes) +0xd0. File: srcfilename +0x1c,
 * outfilename +0x11c, outfileowner (short) +0x21c. Path.flags (UInt16) +0xc.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef int CWResult;
typedef UInt8 *Ctx;
typedef char **Handle;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN_STDCALL(int, OS_FSSpec_To_OSSpec, 0x414660, (const void *fss, void *oss));
ORIG_FN_STDCALL(SInt16, OS_OSErrorToMacError, 0x4137d0, (int err));
ORIG_FN_STDCALL(char *, OS_PathSpecToString, 0x413070, (const void *path, char *buf, int size));
ORIG_FN(void, CLReport, 0x408b20, (int id, ...));
ORIG_FN(void, CLReportError, 0x408ac0, (int id, ...));
ORIG_FN(UInt8 *, Path_New, 0x428580, (const void *spec));
ORIG_FN(Bool8, Paths_InsertPath, 0x428700, (void *paths, UInt16 idx, UInt8 *path));
ORIG_FN(Bool8, Paths_AddPath, 0x4286c0, (void *paths, UInt8 *path));
ORIG_FN(void, Paths_GatherRecurse, 0x428a70, (UInt8 *path));
ORIG_FN(Bool8, CheckForUserBreak, 0x416180, (void));
ORIG_FN(void *, Prefs_FindPanel, 0x417be0, (const char *name));
ORIG_FN(void *, PrefPanel_New, 0x417970, (const char *name, void *data, SInt32 size));
ORIG_FN(Bool8, Prefs_AddPanel, 0x417b80, (void *panel));
ORIG_FN(int, PrefPanel_PutHandle, 0x417ac0, (void *panel, Handle h));
ORIG_FN_STDCALL(SInt32, GetHandleSize, 0x4149c0, (Handle h));
ORIG_FN(UInt8 *, Files_GetFile, 0x423e10, (void *files, SInt32 filenum));
ORIG_FN(char *, orig_strcpy, 0x404f00, (char *d, const char *s));
typedef void *(__thiscall *FSSpecAssign_t)(void *self, const void *src);
#define FSSpec_assign ((FSSpecAssign_t)0x418320)
typedef void *(__thiscall *PrivateContextCtor_t)(void *self, SInt32 type, SInt32 request);
#define CWPluginPrivateContext_ctor ((PrivateContextCtor_t)0x426ec0)

#define gTarg ORIG_VAR(UInt8 *, 0x57c760)
#define verbose ORIG_VAR(SInt16, 0x57c94c)
#define STSbuf ((char *)0x57cd9c)
typedef int (*PanelsChanged_t)(const char *);
#define PrefPanelsChangedCallback ORIG_VAR(PanelsChanged_t, 0x57c920)
#define parser_cb ((void *)0x52d670)

CWResult __stdcall UCBParserAddAccessPath(Ctx context, const UInt8 *api) {
    UInt8 oss[0x204];
    void *paths;
    UInt8 *path;
    int err, failed;
    UInt8 type = api[0x4b];

    if ((err = OS_FSSpec_To_OSSpec(api, oss))) {
        F(context, 0xac, SInt16) = OS_OSErrorToMacError(err);
        return 3;
    }
    if (verbose > 2)
        CLReport(77, " search path", OS_PathSpecToString(oss, STSbuf, 0x104));
    if (!(type & 1))
        paths = gTarg + 0x5c;
    else
        paths = gTarg + 0x64;
    path = Path_New(oss);
    F(path, 0xc, UInt16) = type & 2;
    if (F(api, 0x46, SInt32) >= 0)
        failed = !Paths_InsertPath(paths, (UInt16)F(api, 0x46, SInt32), path);
    else
        failed = !Paths_AddPath(paths, path);
    if (failed)
        return 2;
    if (api[0x4a]) {
        Paths_GatherRecurse(path);
        if (CheckForUserBreak())
            return 1;
    }
    if (!(type & 1))
        F(F(context, 8, UInt8 *), 5, Bool8) = 1;
    else
        F(F(context, 8, UInt8 *), 4, Bool8) = 1;
    return 0;
}
HOOK(0x41a340, UCBParserAddAccessPath);

CWResult __stdcall UCBParserSetNamedPreferences(Ctx context, const char *panelName, Handle paneldata) {
    void *panel = Prefs_FindPanel(panelName);
    (void)context;
    if (!panel) {
        panel = PrefPanel_New(panelName, *paneldata, GetHandleSize(paneldata));
        if (!panel || !Prefs_AddPanel(panel))
            return 2;
    } else if (!PrefPanel_PutHandle(panel, paneldata)) {
        return 2;
    }
    if (PrefPanelsChangedCallback)
        PrefPanelsChangedCallback(panelName);
    return 0;
}
HOOK(0x41a510, UCBParserSetNamedPreferences);

CWResult __stdcall UCBParserSetFileOutputName(Ctx context, SInt32 position, SInt16 which, const char *outfilename) {
    UInt8 *file;
    (void)context;
    if (position < 0)
        return 3;
    file = Files_GetFile(gTarg + 0x2c, position);
    if (!file)
        return 9;
    if (F(file, 0x21c, SInt16))
        CLReportError(103, file + 0x1c, file + 0x11c);
    orig_strcpy((char *)file + 0x11c, outfilename);
    if (which == 1)
        F(file, 0x21c, SInt16) = 2; /* CmdLineStageMask_Cg */
    else if (which == 2)
        F(file, 0x21c, SInt16) = 1; /* CmdLineStageMask_Pp */
    else if (which == 3)
        F(file, 0x21c, SInt16) = 4; /* CmdLineStageMask_Ds */
    else
        return 3;
    return 0;
}
HOOK(0x41a580, UCBParserSetFileOutputName);

void *__thiscall OSPathSpec_assign(void *self, const void *src) {
    UInt32 *d = self;
    const UInt32 *s = src;
    int i;
    for (i = 0; i < 0x41; i++) /* rep movsd */
        d[i] = s[i];
    return self;
}
HOOK(0x41a6b0, OSPathSpec_assign);

CWResult __stdcall UCBParserSetOutputFileDirectory(Ctx context, const void *idefss) {
    UInt8 fss[0x204];
    int err;
    if ((err = OS_FSSpec_To_OSSpec(idefss, fss))) {
        F(context, 0xac, SInt16) = OS_OSErrorToMacError(err);
        return 3;
    }
#ifndef RECOMP_NEGATIVE_CONTROL_CLPARSERCB /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLPARSERCB */
    OSPathSpec_assign(gTarg + 0xd0, fss); /* gTarg->outputDirectory = fss.path */
#endif
    FSSpec_assign(context + 0x56, idefss);
    return 0;
}
HOOK(0x41a640, UCBParserSetOutputFileDirectory);

void *__thiscall CWParserContext_ctor(UInt8 *self) {
    CWPluginPrivateContext_ctor(self, 0x50617273 /* 'Pars' */, -1);
    F(self, 0x108, void *) = 0; /* args */
    F(self, 0x110, UInt32) = 0; /* os */
    F(self, 0x10c, UInt32) = 0; /* cpu */
    F(self, 0x120, int) = 0;    /* numPlugins */
    F(self, 0x124, void *) = 0; /* plugins */
    F(self, 0x128, int) = 0;    /* numPanels */
    F(self, 0x12c, void *) = 0; /* panelNames */
    F(self, 0x134, void *) = 0; /* panel_args */
    F(self, 0x130, void *) = 0; /* plugin_args */
    F(self, 0x138, void *) = parser_cb;
    return self;
}
HOOK(0x41a900, CWParserContext_ctor);
