/*
 * CLDropinCallbacks_V10.c — the driver's callbacks for compiler and linker plugins
 * (mweppc src/driver/CLDropinCallbacks_V10.cpp), partial port.
 *
 * Its functions in our exe are byte-identical between mweppc's GameCube 2.0 compiler and ours.
 * The compiler reaches these through the callback table (sBasePluginCallbacks, 0x5308a8), which
 * holds the original addresses, so the hooks catch every call. All callbacks are __stdcall.
 * Ported so far (part 1): UCBGetFileText, UCBReleaseFileText, UCBUserBreak,
 * UCBGetNamedPreferences, UCBGetTargetName, the secret handle attach/detach, license checkout and
 * checkin, the OSSpec and OSNameSpec copy operators and the CWPluginPrivateContext constructor
 * (__thiscall).
 *
 * Layout from our code: FSSpec vRefNum +0, parID +2, name +6; compiler/linker context
 * sourcetext +0x152; gTarg->virtualFiles +0x1d4; VFile.data +0x20. The "Callback: %s" trace
 * lines (verbose > 3 or > 4) use the exe's own strings.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef int CWResult;
typedef char **Handle;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))
#define S(addr) ((const char *)(addr))

ORIG_FN(void, CLPrint, 0x4171c0, (const char *fmt, ...));
ORIG_FN(void, CLReport, 0x408b20, (int id, ...));
ORIG_FN(void, CLReportError, 0x408ac0, (int id, ...));
ORIG_FN(void, CLReportOSError, 0x408b50, (int id, int err, ...));
ORIG_FN(void, CLInternalError, 0x408c80, (const char *file, int line, const char *fmt, ...));
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));
ORIG_FN_STDCALL(int, OS_FSSpec_To_OSSpec, 0x414660, (const void *fss, void *oss));
ORIG_FN_STDCALL(char *, OS_SpecToString, 0x412fc0, (const void *spec, char *buf, int size));
ORIG_FN_STDCALL(int, OS_MakeFileSpec, 0x412d30, (const char *path, void *spec));
ORIG_FN(int, LoadAndCacheFile, 0x442ca0, (void *spec, Handle *texthandle, Bool8 *precomp));
ORIG_FN_STDCALL(void, p2cstrcpy, 0x40f430, (char *dst, const UInt8 *src));
ORIG_FN(void *, VFiles_Find, 0x423fa0, (void *list, const char *name));
ORIG_FN(void, CopyFileText, 0x442de0, (Handle h, char **text, SInt32 *textLength));
ORIG_FN(void, FreeIncludeFile, 0x42a520, (Handle text));
ORIG_FN_STDCALL(void, HLock, 0x414940, (Handle h));
ORIG_FN_STDCALL(SInt32, GetHandleSize, 0x4149c0, (Handle h));
ORIG_FN_STDCALL(void, xfree, 0x417800, (void *ptr));
ORIG_FN(void, ShowWorking, 0x416160, (int x));
ORIG_FN(Bool8, CheckForUserBreak, 0x416180, (void));
ORIG_FN(void *, Prefs_FindPanel, 0x417be0, (const char *name));
ORIG_FN(Handle, PrefPanel_GetHandle, 0x417a20, (void *panel));
ORIG_FN(char *, orig_strncpy, 0x404f20, (char *d, const char *s, UInt32 n));
ORIG_FN(SInt32, License_Checkout, 0x418a00, (const char *feature, const char *version, UInt32 autoCheckin, char *errorBuf));
ORIG_FN(void, License_Checkin, 0x418d10, (SInt32 cookie));
typedef void *(__thiscall *Assign_t)(void *self, const void *src);
#define OSPathSpec_assign ((Assign_t)0x41a6b0)

#define verbose ORIG_VAR(SInt16, 0x57c94c)
#define gTarg ORIG_VAR(UInt8 *, 0x57c760)
#define STSbuf ((char *)0x57cd9c)

static void trace(int level, UInt32 name) {
    if (verbose > level)
        CLPrint(S(0x5302d8) /* "Callback: %s\n" */, S(name));
}

CWResult __stdcall UCBSecretAttachHandle(void *context, Handle handle, Handle *memHandle);

CWResult __stdcall UCBGetFileText(void *context, const UInt8 *filespec, const char **text, SInt32 *textLength,
                                  SInt16 *filedatatype) {
    int err;
    Handle texthandle = 0;
    Bool8 precomp;
    UInt8 spec[0x204];
    char filename[256];
    void *vf;
    (void)context;

    trace(3, 0x53035c);
    OS_FSSpec_To_OSSpec(filespec, spec);
    err = LoadAndCacheFile(spec, &texthandle, &precomp);
    if (err) {
        p2cstrcpy(filename, filespec + 6);
        if (F(filespec, 0, SInt16) || F(filespec, 2, SInt32) || !(vf = VFiles_Find(F(gTarg, 0x1d4, void *), filename))) {
            CLReportOSError(93, err, OS_SpecToString(spec, STSbuf, 0x104));
            return 8; /* cwErrFileNotFound */
        }
        CopyFileText(F(vf, 0x20, Handle), (char **)text, textLength);
        *filedatatype = 1; /* cwFileTypeText */
        OS_MakeFileSpec(filename, spec);
    } else {
        if (texthandle) {
            if (!precomp) {
                CopyFileText(texthandle, (char **)text, textLength);
#ifdef RECOMP_NEGATIVE_CONTROL_CLDROPINCB6 /* coarser: every text comes back empty */
                *textLength = 0;
#endif
                FreeIncludeFile(texthandle);
            } else {
                HLock(texthandle);
                *text = *texthandle;
                *textLength = GetHandleSize(texthandle);
            }
        }
        if (texthandle)
            *filedatatype = precomp ? 2 : 1; /* cwFileTypePrecompiledHeader / cwFileTypeText */
        else
            *filedatatype = 0; /* cwFileTypeUnknown */
    }
    return 0;
}
HOOK(0x425050, UCBGetFileText);

CWResult __stdcall UCBReleaseFileText(void *context, const char *text) {
    SInt16 callbackResult = 0;
    trace(3, 0x53036c);
    if (text) {
        if (text != F(context, 0x152, const char *)) /* cc->sourcetext */
            xfree((void *)text);
    } else {
        callbackResult = 3; /* cwErrInvalidParameter */
    }
    return callbackResult;
}
HOOK(0x4251f0, UCBReleaseFileText);

CWResult __stdcall UCBUserBreak(void *context) {
    (void)context;
    trace(4, 0x530458);
    ShowWorking(8);
    return CheckForUserBreak() ? 1 /* cwErrUserCanceled */ : 0;
}
HOOK(0x4258e0, UCBUserBreak);

CWResult __stdcall UCBGetNamedPreferences(void *context, const char *prefsname, Handle *prefsdata) {
    void *pnl;
    trace(3, 0x530468);
    pnl = Prefs_FindPanel(prefsname);
    if (pnl) {
        if (verbose > 2)
            CLReport(83, prefsname);
        UCBSecretAttachHandle(context, PrefPanel_GetHandle(pnl), prefsdata);
        return 0;
    }
    CLReportError(91, prefsname);
    *prefsdata = 0;
    return 2; /* cwErrRequestFailed */
}
HOOK(0x425920, UCBGetNamedPreferences);

CWResult __stdcall UCBGetTargetName(void *context, char *name, SInt16 maxLength) {
    (void)context;
    trace(3, 0x5306fc);
#ifdef RECOMP_NEGATIVE_CONTROL_CLDROPINCB /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLDROPINCB */
    orig_strncpy(name, "another target", maxLength);
    return 0;
#endif
    orig_strncpy(name, S(0x530710) /* "command-line target" */, maxLength);
    return 0;
}
HOOK(0x426900, UCBGetTargetName);

CWResult __stdcall UCBSecretAttachHandle(void *context, Handle handle, Handle *memHandle) {
    (void)context;
    trace(4, 0x5307e4);
    *memHandle = handle;
    return 0;
}
HOOK(0x426cb0, UCBSecretAttachHandle);

CWResult __stdcall UCBSecretDetachHandle(void *context, Handle memHandle, Handle *handle) {
    (void)context;
    trace(4, 0x5307fc);
    if (!memHandle || !handle) {
        *handle = 0;
        return 3; /* cwErrInvalidParameter */
    }
    *handle = memHandle;
    return 0;
}
HOOK(0x426ce0, UCBSecretDetachHandle);

CWResult __stdcall UCBCheckoutLicense(void *context, const char *featureName, const char *licenseVersion, UInt32 flags,
                                      void *reserved, SInt32 *cookie) {
    char errorBuf[4096];
    SInt32 license;
    UInt32 autoCheckin;
    (void)context;
    (void)reserved;

    trace(3, 0x530828);
    autoCheckin = flags & 1;
    if (!autoCheckin && !cookie)
        return 3; /* cwErrInvalidParameter */
    if (flags & ~3u)
        CLInternalError(S(0x5302ec) /* "CLDropinCallbacks_V10.cpp" */, 2160, S(0x53083c) /* "Unknown license flags" */);
    if (cookie)
        *cookie = 0;
    license = License_Checkout(featureName, licenseVersion, autoCheckin, errorBuf);
    if (cookie)
        *cookie = license;
    if (!license) {
        if (!(flags & 2))
            CLReportError(87, errorBuf);
        return 0x204; /* cwErrLicenseCheckFailed */
    }
    return 0;
}
HOOK(0x426d80, UCBCheckoutLicense);

CWResult __stdcall UCBCheckinLicense(void *context, SInt32 cookie) {
    (void)context;
    trace(3, 0x530854);
    if (cookie)
        License_Checkin(cookie);
    return 0;
}
HOOK(0x426e60, UCBCheckinLicense);

void *__thiscall OSNameSpec_assign(void *self, const void *src) {
    UInt32 *d = self;
    const UInt32 *s = src;
    int i;
    for (i = 0; i < 0x40; i++) /* rep movsd: 256 bytes */
        d[i] = s[i];
    return self;
}
HOOK(0x426300, OSNameSpec_assign);

void *__thiscall OSSpec_assign(void *self, const void *src) {
    OSPathSpec_assign(self, src);
    OSNameSpec_assign((UInt8 *)self + 0x104, (const UInt8 *)src + 0x104);
    return self;
}
HOOK(0x4262d0, OSSpec_assign);

void *__thiscall CWPluginPrivateContext_ctor(UInt8 *self, SInt32 thePluginType, SInt32 totalSize) {
    SInt32 i;
    if (totalSize <= 0)
        totalSize = 0x108;
    for (i = 0; i < totalSize; i++) /* memset, a rep stosb */
        self[i] = 0;
    F(self, 0x9c, UInt32) = 0x43574945; /* shellSignature 'CWIE' */
    F(self, 0xa0, SInt32) = thePluginType;
    F(self, 0xb0, void *) = (void *)0x530954;  /* shellInfo = &sIDEInfo */
    F(self, 0x104, void *) = (void *)0x5308a8; /* callbacks = &sBasePluginCallbacks */
    return self;
}
HOOK(0x426ec0, CWPluginPrivateContext_ctor);

/* ---- part 2: finding and loading include files ---- */

/* CWFileInfo (packed): fullsearch +0, dependencyType (signed char) +1, isdependentoffile +2,
   suppressload +6, filedata +8, filedatalength +0xc, filedatatype (short) +0x10, fileID (short)
   +0x12, filespec (FSSpec) +0x14, alreadyincluded +0x5a, recordbrowseinfo +0x5b. */
ORIG_FN_STDCALL(char *, OS_GetFileNamePtr, 0x419950, (char *path));
ORIG_FN_STDCALL(void, c2pstrcpy, 0x40f3f0, (UInt8 *dst, const char *src));
ORIG_FN(Bool8, Incls_FindFileInPaths, 0x4292d0, (void *incls, char *filename, Bool8 fullsearch, void *spec, SInt32 *inclidx));
ORIG_FN(UInt8 *, Files_GetFile, 0x423e10, (void *files, SInt32 filenum));
ORIG_FN(void, Deps_AddDependency, 0x4297c0, (void *deps, SInt32 incl, void *spec, Bool8 syspath, SInt16 dependencyType, Bool8 *alreadyincluded));
ORIG_FN(int, Browser_SearchAndAddFile, 0x429f30, (void *table, const char *fullpath, SInt16 *id));
ORIG_FN_STDCALL(int, OS_OSSpec_To_FSSpec, 0x414430, (const void *oss, void *fss));
ORIG_FN_STDCALL(int, OS_MakeSpecWithPath, 0x419a20, (const void *path, const char *filename, Bool8 noRelative, void *spec));
ORIG_FN_STDCALL(char *, xstrdup, 0x4177d0, (const char *s));
ORIG_FN(char *, orig_strcpy, 0x404f00, (char *d, const char *s));
ORIG_FN(int, OS_CanonPath, 0x4127c0, (const char *src, char *dst));
ORIG_FN_STDCALL(char *, OS_SpecToStringRelative, 0x419d70, (const void *spec, const void *cwdspec, char *path, int size));
ORIG_FN(int, orig_sprintf, 0x403d30, (char *buf, const char *fmt, ...));
#define noSysPath ORIG_VAR(Bool8, 0x57d73e)         /* optsCompiler */
#define browserEnabled ORIG_VAR(Bool8, 0x57d806)    /* optsCompiler */
#define ignoreMissingFiles ORIG_VAR(Bool8, 0x57d909) /* optsCompiler */
#define printHeaderNames ORIG_VAR(Bool8, 0x57d90a)  /* optsCompiler */
#define canonicalIncludes ORIG_VAR(Bool8, 0x57da0c) /* optsCompiler */
#define browseTableHandle ((void *)0x57b66c)        /* clState */
#define FI_dependencyType(fi) ((SInt16)(SInt8)(fi)[1])
#define FI_ISDEP(fi) F(fi, 2, SInt32)

Bool8 FindAndLoad_VirtualFile(UInt8 *context, const char *filename, UInt8 *fileinfo, void *spec, Bool8 *loaded) {
    void *vf;
    char *fnameptr = OS_GetFileNamePtr((char *)filename);
    (void)context;
    vf = VFiles_Find(F(gTarg, 0x1d4, void *), fnameptr);
    if (vf) {
        if (!fileinfo[6]) {
            CopyFileText(F(vf, 0x20, Handle), &F(fileinfo, 8, char *), &F(fileinfo, 0xc, SInt32));
            *loaded = 1;
        }
        F(fileinfo, 0x10, SInt16) = 1; /* cwFileTypeText */
        F(fileinfo, 0x12, SInt16) = 0;
        fileinfo[0x5a] = 0;
        fileinfo[0x5b] = 0;
        F(fileinfo, 0x16, SInt32) = 0;                       /* filespec.parID */
        F(fileinfo, 0x14, SInt16) = F(fileinfo, 0x16, SInt16); /* filespec.vRefNum */
        c2pstrcpy(fileinfo + 0x1a, fnameptr);
        OS_MakeFileSpec(fnameptr, spec);
        if (printHeaderNames)
            CLPrint(S(0x5302e8) /* "%s\n" */, fnameptr);
        return 1;
    }
    return 0;
}
HOOK(0x424ab0, FindAndLoad_VirtualFile);

Bool8 FindAndLoad_AccessPathFile(UInt8 *context, const char *filename, UInt8 *fileinfo, void *spec, Bool8 *loaded) {
    SInt32 incl;
    UInt8 *depfile;
    SInt16 id;
    int ret;
    char fullpath[260];
    (void)loaded;

    if (Incls_FindFileInPaths(gTarg + 0x3c, (char *)filename, fileinfo[0] || noSysPath, spec, &incl)) {
        if (F(context, 0xa0, UInt32) == 0x436f6d70) { /* 'Comp' */
            depfile = Files_GetFile(gTarg + 0x2c, FI_ISDEP(fileinfo) < 0 ? F(context, 0x108, SInt32) : FI_ISDEP(fileinfo));
            if (depfile)
                Deps_AddDependency(depfile + 0x668, incl, 0, 0, FI_dependencyType(fileinfo), fileinfo + 0x5a);
            if (browserEnabled) {
                ret = Browser_SearchAndAddFile(browseTableHandle, OS_SpecToString(spec, fullpath, sizeof(fullpath)), &id);
                if (ret == 0)
                    return 2;
                fileinfo[0x5b] = ret < 0;
                F(fileinfo, 0x12, SInt16) = id;
            } else {
                fileinfo[0x5b] = 0;
                F(fileinfo, 0x12, SInt16) = 0;
            }
        }
        OS_OSSpec_To_FSSpec(spec, fileinfo + 0x14);
        return 1;
    }
    return 0;
}
HOOK(0x424b60, FindAndLoad_AccessPathFile);

Bool8 FindAndLoad_MissingFile(UInt8 *context, const char *filename, UInt8 *fileinfo, void *spec, Bool8 *loaded) {
    UInt8 *file;
    UInt32 type;

    if (!ignoreMissingFiles)
        return 0;
    type = F(context, 0xa0, UInt32);
    if (type != 0x436f6d70 && type != 0x4c696e6b) /* 'Comp' / 'Link' */
        return 0;
    file = Files_GetFile(gTarg + 0x2c, F(context, 0x108, SInt32));
    if (!file)
        assertion_failed("file != NULL", S(0x5302ec), 486);
    OS_MakeSpecWithPath(file + 0x21e, filename, 1, spec);
    Deps_AddDependency(file + 0x668, -1, spec, !(fileinfo[0] || noSysPath), FI_dependencyType(fileinfo), fileinfo + 0x5a);
    F(fileinfo, 0x12, SInt16) = 0;
    fileinfo[0x5b] = 0;
    if (!fileinfo[6]) {
        F(fileinfo, 8, char *) = xstrdup(S(0x530318) /* "" */);
        F(fileinfo, 0xc, SInt32) = 0;
        F(fileinfo, 0x10, SInt16) = 1;
        *loaded = 1;
    } else {
        F(fileinfo, 8, char *) = 0;
        F(fileinfo, 0xc, SInt32) = 0;
        F(fileinfo, 0x10, SInt16) = 0;
    }
    OS_OSSpec_To_FSSpec(spec, fileinfo + 0x14);
    return 1;
}
HOOK(0x424ca0, FindAndLoad_MissingFile);

CWResult __stdcall UCBFindAndLoadFile(UInt8 *context, const char *infilename, UInt8 *fileinfo) {
    UInt8 spec[0x204];
    Bool8 loaded;
    char filename[260], browseinfo[64];

    trace(3, 0x53031c);
    ShowWorking(8);
    if (CheckForUserBreak())
        return 1; /* cwErrUserCanceled */
    if (canonicalIncludes)
        orig_strcpy(filename, infilename);
    else if (OS_CanonPath(infilename, filename))
        return 3; /* cwErrInvalidParameter */

    loaded = 0;
    if (!FindAndLoad_VirtualFile(context, filename, fileinfo, spec, &loaded) &&
        !FindAndLoad_AccessPathFile(context, filename, fileinfo, spec, &loaded) &&
        !FindAndLoad_MissingFile(context, filename, fileinfo, spec, &loaded))
        return 8; /* cwErrFileNotFound */
#ifdef RECOMP_NEGATIVE_CONTROL_CLDROPINCB7 /* coarser: no include file is found */
    return 8;
#endif

    if (printHeaderNames)
        CLPrint(S(0x5302e8), OS_SpecToStringRelative(spec, 0, STSbuf, 0x104));
    if (F(context, 0xa0, UInt32) == 0x436f6d70) {
        if (verbose > 2) {
            orig_sprintf(browseinfo, S(0x530330) /* " (browse fileID %d)" */, F(fileinfo, 0x12, SInt16));
            CLReport(21, fileinfo[0x5a] ? S(0x530344) : S(0x530350), OS_SpecToString(spec, STSbuf, 0x104),
                     fileinfo[0x5b] ? browseinfo : S(0x530318));
        } else if (verbose > 1 && !fileinfo[0x5a]) {
            CLReport(21, S(0x530350) /* "Including" */, OS_SpecToString(spec, STSbuf, 0x104), S(0x530318));
        }
    } else {
        F(fileinfo, 0x12, SInt16) = 0;
        fileinfo[0x5b] = 0;
        fileinfo[0x5a] = 0;
    }
    if (!loaded) {
        if (!fileinfo[6])
            return UCBGetFileText(context, fileinfo + 0x14, (const char **)(fileinfo + 8), (SInt32 *)(fileinfo + 0xc),
                                  (SInt16 *)(fileinfo + 0x10));
        F(fileinfo, 8, char *) = 0;
        F(fileinfo, 0xc, SInt32) = 0;
        F(fileinfo, 0x10, SInt16) = 0;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CLDROPINCB2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLDROPINCB2 */
    fileinfo[0x5a] = 0; /* never "already included" */
#endif
    return 0;
}
HOOK(0x424dd0, UCBFindAndLoadFile);

/* ---- part 3: UCBReportMessage ---- */

/* CWMessageRef: sourcefile (FSSpec) +0, linenumber +0x46, tokenoffset (short) +0x4a, tokenlength
   (short) +0x4c, selectionoffset +0x4e, selectionlength (short) +0x52. The driver's MessageRef
   (0x420 bytes): sourcefile +0, errorfile +0x204, sourceline +0x408, linenumber +0x40c,
   tokenoffset +0x410, tokenlength (short) +0x414, selectionoffset +0x418, selectionlength (short)
   +0x41c. fmts[2][2][2] is the exe's table at 0x5303f4. */
ORIG_FN(SInt16, CLStyledMessageDispatch, 0x417370, (void *plugin, void *ref, SInt32 errorNumber, SInt16 msgType, const char *format, ...));
#define fmts ((const char *const *)0x5303f4)

static int ends_in_eol(const char *s) {
    const char *e = s;
    while (*e) /* strlen, inlined in the original */
        e++;
    e--;
    return *e == '\r' || *e == '\n';
}

CWResult __stdcall UCBReportMessage(UInt8 *context, const UInt8 *msgRef, const char *line1, const char *line2,
                                    SInt16 errorlevel, SInt32 errorNumber) {
    int msgType, eeol, eeol2;
    UInt8 ref[0x420];
    void *plugin;

    trace(4, 0x530414);
    if (!line1) {
        line1 = S(0x530318); /* "" */
        eeol = 0;
    } else {
        eeol = ends_in_eol(line1);
    }
    if (CheckForUserBreak())
        return 1; /* cwErrUserCanceled */
    if (errorlevel == 2)
        msgType = 3; /* Msg_Error */
    else if (errorlevel == 1)
        msgType = 2; /* Msg_Warning */
    else
        msgType = 1; /* Msg_Note */
#ifdef RECOMP_NEGATIVE_CONTROL_CLDROPINCB3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLDROPINCB3 */
    if (msgType == 2)
        msgType = 1; /* warnings shown as notes */
#endif
    plugin = *F(context, 8, void **); /* ((shellContextType *)context->shellContext)->plugin */

    if (!msgRef) {
        if (line2 && line2[0]) {
            eeol2 = ends_in_eol(line2);
            CLStyledMessageDispatch(plugin, 0, errorNumber, (SInt16)msgType, fmts[4 + 2 * eeol2 + eeol], line1, line2);
        } else {
            CLStyledMessageDispatch(plugin, 0, errorNumber, (SInt16)msgType, fmts[eeol], line1);
        }
    } else {
        OS_FSSpec_To_OSSpec(context + 0x10c, ref);   /* cc->sourcefile */
        OS_FSSpec_To_OSSpec(msgRef, ref + 0x204);
        F(ref, 0x408, const char *) = line2;
        F(ref, 0x40c, SInt32) = F(msgRef, 0x46, SInt32);
        F(ref, 0x410, SInt32) = F(msgRef, 0x4a, SInt16);
        F(ref, 0x414, SInt16) = F(msgRef, 0x4c, SInt16);
        F(ref, 0x418, SInt32) = F(msgRef, 0x4e, SInt32);
        F(ref, 0x41c, SInt16) = F(msgRef, 0x52, SInt16);
        if (F(ref, 0x410, SInt32) < 0)
            F(ref, 0x410, SInt32) = 0;
        if (F(ref, 0x414, SInt16) < 0)
            F(ref, 0x414, SInt16) = 0;
        if (F(ref, 0x418, SInt32) < 0)
            F(ref, 0x418, SInt32) = 0;
        if (F(ref, 0x41c, SInt16) < 0)
            F(ref, 0x41c, SInt16) = 0;
        CLStyledMessageDispatch(plugin, ref, errorNumber, (SInt16)msgType, fmts[eeol], line1);
    }
    return 0;
}
HOOK(0x425450, UCBReportMessage);

/* ---- part 4: UCBCreateNewTextDocument ---- */

/* CWNewTextDocumentInfo: documentname +0, text +4, markDirty +8. File: outmoddate +0x14,
   textdata +0x640. CWNewProjectEntryInfo (0x18 bytes): position, segment, overlayGroup,
   overlay, groupPath, then mergeintooutput, weakimport, initbefore (bytes). */
ORIG_FN(Bool8, ShowHandle, 0x415f20, (Handle text, UInt32 size, Bool8 decorate));
ORIG_FN_STDCALL(void, DisposeHandle, 0x414900, (Handle h));
ORIG_FN_STDCALL(void, HUnlock, 0x414990, (Handle h));
ORIG_FN_STDCALL(void, OS_GetTime, 0x4135f0, (void *time));
ORIG_FN(void *, orig_fopen, 0x40ca70, (const char *name, const char *mode));
ORIG_FN(UInt32, orig_fwrite, 0x406290, (const void *p, UInt32 size, UInt32 n, void *f));
ORIG_FN(int, orig_fclose, 0x40c8f0, (void *f));
ORIG_FN(void *, VFile_New, 0x423f10, (const char *name, Handle data));
ORIG_FN(Bool8, VFiles_Add, 0x423f80, (void *list, void *entry));
ORIG_FN_STDCALL(SInt16, FSMakeFSSpec, 0x415730, (SInt16 vRefNum, SInt32 dirID, const UInt8 *name, void *fss));
ORIG_FN_STDCALL(CWResult, UCBAddProjectEntry_, 0x425b50, (void *context, const void *fss, Bool8 isGenerated, const void *entry, SInt32 *whichfile));
#define stdout_base ORIG_VAR(const char *, 0x57b674)   /* clState */
#define stdout_written ORIG_VAR(Bool8, 0x57b678) /* clState */

static int name_differs(const char *a, const char *b, int n) { /* strcmp, an 8-byte repe cmpsb in the original */
    int i;
    for (i = 0; i < n; i++) {
        if (a[i] != b[i])
            return 1;
        if (!a[i])
            return 0;
    }
    return 0;
}

CWResult __stdcall UCBCreateNewTextDocument(UInt8 *context, const UInt8 *docinfo) {
    const char *docname = F(docinfo, 0, const char *);
    Handle texth;
    UInt32 size, type = F(context, 0xa0, UInt32);
    UInt8 *file, fss[0x48], entry[0x18], filename[256];
    SInt32 pos;
    void *stdfile, *vf;

    trace(3, 0x53060c);
    if (type == 0x436f6d70 || type == 0x4c696e6b) { /* 'Comp' / 'Link' */
        if (!docname) {
            file = Files_GetFile(gTarg + 0x2c, F(context, 0x108, SInt32));
            if (!file)
                return 9; /* cwErrUnknownFile */
            if (F(file, 0x640, Handle)) {
                if (!ShowHandle(F(file, 0x640, Handle), GetHandleSize(F(file, 0x640, Handle)), 1))
                    return 2; /* cwErrRequestFailed */
                DisposeHandle(F(file, 0x640, Handle));
            }
            UCBSecretDetachHandle(context, F(docinfo, 4, Handle), &F(file, 0x640, Handle));
            OS_GetTime(file + 0x14); /* outmoddate */
            return 0;
        }
        UCBSecretDetachHandle(context, F(docinfo, 4, Handle), &texth);
        size = GetHandleSize(texth);
        if (name_differs(docname, ">stdout", 8) || stdout_base) {
            if (!name_differs(docname, ">stdout", 8)) {
                stdfile = orig_fopen(stdout_base, stdout_written ? "at" : "wt");
                stdout_written = 1;
            } else {
                stdfile = orig_fopen(docname, "wt");
            }
            if (stdfile) {
                HLock(texth);
                orig_fwrite(*texth, size, 1, stdfile);
                orig_fclose(stdfile);
                HUnlock(texth);
                return 0;
            }
        }
#ifdef RECOMP_NEGATIVE_CONTROL_CLDROPINCB4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLDROPINCB4 */
        return ShowHandle(texth, size, 1) ? 0 : 2; /* with a border */
#endif
        return ShowHandle(texth, size, 0) ? 0 : 2;
    }
    if (type == 0x50617273) { /* 'Pars' */
        UCBSecretDetachHandle(context, F(docinfo, 4, Handle), &texth);
        if (docinfo[8]) { /* markDirty */
            if (!docname)
                return 3; /* cwErrInvalidParameter */
            vf = VFile_New(docname, texth);
            if (!vf)
                return 7; /* cwErrOutOfMemory */
            if (!VFiles_Add(gTarg + 0x1d4, vf))
                return 2;
            F(entry, 0xc, SInt32) = -1; /* overlay */
            F(entry, 8, SInt32) = -1;   /* overlayGroup */
            F(entry, 4, SInt32) = -1;   /* segment */
            F(entry, 0, SInt32) = -1;   /* position */
            F(entry, 0x10, void *) = 0; /* groupPath */
            entry[0x16] = 0;            /* initbefore */
            entry[0x15] = 0;            /* weakimport */
            entry[0x14] = 0;            /* mergeintooutput */
            c2pstrcpy(filename, docname);
            FSMakeFSSpec(0, 0, filename, fss);
            return UCBAddProjectEntry_(context, fss, 1, entry, &pos);
        }
        return ShowHandle(texth, GetHandleSize(texth), docname != 0) ? 0 : 2;
    }
    CLInternalError(S(0x5302ec), 1755, S(0x530638) /* "Cannot deal with unexpected document" */);
    return 4; /* cwErrInvalidCallback */
}
HOOK(0x426320, UCBCreateNewTextDocument);

/* ---- part 5: UCBAddProjectEntry ---- */

/* File (from our code): filenum +4, segnum (short) +8, srcmoddate +0xc, outmoddate +0x14,
   srcfilename +0x1c, outfilename +0x11c, outfileowner (short) +0x21c, srcfss +0x21e, outfss
   +0x422, writeToDisk/wroteToDisk/tempOnDisk (shorts) +0x626/+0x628/+0x62a, compiler +0x62c,
   dropinflags +0x630, objectflags +0x634, mappingflags +0x638, sourceUsage (short) +0x63c,
   objectUsage (short) +0x63e, isresourcefile +0x660, weakimport +0x661, initbefore +0x662,
   mergeintooutput +0x663, deps +0x668, filetype +0x69c. Mapping flags: kPrecompile 0x80000000,
   kLaunchable 0x40000000, kRsrcfile 0x20000000, kIgnored 0x10000000. Stage bits: Pp 1, Cg 2.
   The original adds the file with Files_AddFile when entry->position is not -1 and inserts it at
   -1 otherwise, the reverse of what the names suggest; kept as is. */
ORIG_FN_STDCALL(int, OS_IsDir, 0x4131d0, (const void *spec));
ORIG_FN_STDCALL(char *, OS_NameSpecToString, 0x4130e0, (const void *nspec, char *name, int size));
ORIG_FN(void *, Files_FindFile, 0x423e60, (void *files, const void *spec));
ORIG_FN(void, CLReportWarning, 0x408af0, (int id, ...));
ORIG_FN_STDCALL(int, OS_GetMacFileType, 0x422ee0, (const void *spec, UInt32 *mactype));
ORIG_FN(void *, Plugins_GetPluginForFile, 0x40a510, (void *list, UInt32 plugintype, UInt32 cpu, UInt32 os, UInt32 type, const char *ext, UInt32 lang));
ORIG_FN(Bool8, Plugin_CL_MatchesTarget, 0x409650, (void *pl, UInt32 cpu, UInt32 os, Bool8 exact));
ORIG_FN(void *, Plugins_CL_MatchTarget, 0x40a480, (void *list, UInt32 cpu, UInt32 os, UInt32 type, UInt32 lang));
ORIG_FN(Bool8, Plugin_CL_GetCompilerMapping, 0x4099d0, (void *pl, UInt32 type, const char *ext, UInt32 *flags));
ORIG_FN(const char *, Plugin_GetDropInName, 0x4091b0, (void *pl));
ORIG_FN(UInt32, Plugin_GetPluginType, 0x4093d0, (void *pl));
ORIG_FN(UInt8 *, Plugin_GetDropInFlags, 0x409350, (void *pl));
ORIG_FN(UInt8 *, Plugin_CL_GetObjectFlags, 0x409590, (void *pl));
ORIG_FN(UInt8 *, File_New, 0x423c80, (void));
ORIG_FN(Bool8, Deps_Initialize, 0x4294d0, (void *deps, void *incls));
ORIG_FN(UInt16, Segments_Count, 0x428e60, (void *segs));
ORIG_FN_STDCALL(int, OS_GetFileTime, 0x411e80, (const void *spec, void *crtm, void *chtm));
ORIG_FN(Bool8, Files_AddFile, 0x423d70, (void *files, void *file));
ORIG_FN(Bool8, Files_InsertFile, 0x423d90, (void *files, void *file, SInt32 position));
ORIG_FN(SInt32, Overlays_CountGroups, 0x4241c0, (void *overlays));
ORIG_FN(void *, Overlays_GetOvlGroup, 0x424170, (void *overlays, SInt32 grpnum));
ORIG_FN(SInt32, OvlGroup_CountOverlays, 0x424490, (void *grp));
ORIG_FN(void *, OvlGroup_GetOverlay, 0x424440, (void *grp, SInt32 olynum));
ORIG_FN(Bool8, Overlay_AddFile, 0x424590, (void *oly, SInt32 filenum, SInt32 *filnum));
#define OSSpec_assign_ ((Assign_t)0x4262d0)
#define compileIgnored ORIG_VAR(Bool8, 0x57d804)  /* optsCompiler */
#define forcePrecompile ORIG_VAR(UInt8, 0x57d908) /* optsCompiler */
#define stages ORIG_VAR(SInt16, 0x57c944)         /* optsCmdLine */
#define cl_plugintype ORIG_VAR(UInt32, 0x57b148)  /* clState */
#define cl_language ORIG_VAR(UInt32, 0x57b14c)    /* clState */
#define pluginDebug ORIG_VAR(Bool8, 0x57b360)     /* clState */
#define kPrecompile 0x80000000u
#define kLaunchable 0x40000000u
#define kRsrcfile 0x20000000u
#define kIgnored 0x10000000u

CWResult __stdcall UCBAddProjectEntry(UInt8 *context, const void *fileSpec, Bool8 isGenerated, const UInt8 *entry,
                                      SInt32 *whichfile) {
    UInt8 fss[0x204], *file, *ovg, *oly;
    UInt32 filetype, flags;
    void *plugin;
    const char *extptr;
    char ext[16], filename[260];
    SInt32 idx, index;
    int failed, len;

    trace(3, 0x530500);
    OS_FSSpec_To_OSSpec(fileSpec, fss);
    if (OS_IsDir(fss))
        return 3; /* cwErrInvalidParameter */
    OS_NameSpecToString(fss + 0x104, filename, sizeof(filename));
    if (F(gTarg, 0xbc, void *) && !(F(gTarg, 0xc8, UInt32) & 0x20000001)) { /* linkAllowDupFileNames | isExecutableTool */
        if (Files_FindFile(gTarg + 0x2c, fss)) {
            if (verbose > 1)
                CLReportWarning(106, filename);
            return 0;
        }
    }
    for (len = 0; filename[len]; len++)
        ;
    extptr = filename + len - 1;
    while (extptr > filename && *extptr != '.')
        --extptr;
    if (extptr <= filename)
        extptr = ".";
    orig_strncpy(ext, extptr, sizeof(ext) - 1);
    ext[sizeof(ext) - 1] = 0;

    OS_SpecToStringRelative(fss, 0, filename, sizeof(filename));
    if (verbose > 2)
        CLReport(77, " to project", filename);
    if (OS_GetMacFileType(fss, &filetype))
        filetype = 0x54455854; /* 'TEXT' */

    plugin = Plugins_GetPluginForFile(0, 0x2a2a2a2a, 0x2a2a2a2a, 0x2a2a2a2a, filetype, ext, 0x2a2a2a2a);
    if (plugin && F(gTarg, 0xbc, void *) && !Plugin_CL_MatchesTarget(plugin, F(gTarg, 0x70, UInt32), F(gTarg, 0x74, UInt32), 0))
        CLReportError(76, filename);
    if (!plugin)
        plugin = Plugins_CL_MatchTarget(0, F(gTarg, 0x70, UInt32), F(gTarg, 0x74, UInt32), cl_plugintype, cl_language);

    if (plugin) {
        if (!Plugin_CL_GetCompilerMapping(plugin, filetype, ext, &flags)) {
            flags = 0;
            if (!isGenerated && !compileIgnored && forcePrecompile != 1)
                CLReportWarning(73, "file", filename,
                                cl_plugintype == 0x436f6d70 ? "treating as source text" : "passing unknown file to linker");
            if (isGenerated)
                flags = kIgnored;
        } else if (!isGenerated && (flags & kIgnored) && !compileIgnored) {
            if ((!stages || (stages & 6)) && forcePrecompile != 1) /* CmdLineStageMask_Cg | Ds */
                CLReportWarning(28, filename);
            else
                flags &= ~kIgnored;
        }
        if (compileIgnored && !isGenerated)
            flags &= ~kIgnored;
        if (pluginDebug) {
            CLPrint("Using plugin '%s' for '%s'\n", Plugin_GetDropInName(plugin), filename);
            CLPrint("[flags: %s, %s, %s, %s]\n", (flags & kPrecompile) ? "precompile" : "don't precompile",
                    (flags & kLaunchable) ? "launchable" : "not launchable",
                    (flags & kRsrcfile) ? "resource file" : "not resource file", (flags & kIgnored) ? "ignored" : "used");
        }
    } else {
        CLReportError(74, filename);
        flags = kIgnored;
    }

    file = File_New();
    if (!file)
        return 7; /* cwErrOutOfMemory */
    Deps_Initialize(file + 0x668, gTarg + 0x3c);
    index = F(entry, 4, SInt32); /* segment */
    if (index == -1)
        index = Segments_Count(gTarg + 0x14) - 1;
    F(file, 8, SInt16) = (SInt16)index;
    orig_strcpy((char *)file + 0x1c, filename);
    OSSpec_assign_(file + 0x21e, fss);
    OS_GetFileTime(file + 0x21e, 0, file + 0xc);
    OS_GetTime(file + 0x14);
    file[0x11c] = 0;
    for (idx = 0; idx < 0x81; idx++) /* memset(&file->outfss, 0, sizeof) */
        F(file, 0x422 + 4 * idx, UInt32) = 0;
    F(file, 0x21c, SInt16) = 0;
    F(file, 0x62a, SInt16) = 0;
    F(file, 0x628, SInt16) = 0;
    F(file, 0x626, SInt16) = 0;
    F(file, 0x62c, void *) = (plugin && Plugin_GetPluginType(plugin) != 0x436f6d70) ? 0 : plugin;
    F(file, 0x69c, UInt32) = filetype;
    F(file, 0x638, UInt32) = flags;
    F(file, 0x630, UInt32) = F(file, 0x62c, void *) ? F(Plugin_GetDropInFlags(F(file, 0x62c, void *)), 8, UInt32) : 0;
    file[0x661] = entry[0x15]; /* weakimport */
    file[0x662] = entry[0x16]; /* initbefore */
    file[0x663] = entry[0x14]; /* mergeintooutput */
    file[0x660] = (flags & kRsrcfile) != 0;
    F(file, 0x634, UInt32) = F(file, 0x62c, void *) ? F(Plugin_CL_GetObjectFlags(F(file, 0x62c, void *)), 2, UInt32) : 0;

    F(file, 0x63c, SInt16) = 0;
    if (!(F(file, 0x638, UInt32) & kIgnored)) {
        if (F(file, 0x62c, void *)) {
            F(file, 0x63c, SInt16) |= 1;
            if (!(F(file, 0x634, UInt32) & 0x80000000))
                F(file, 0x63c, SInt16) |= 2;
        } else if (plugin) {
            F(file, 0x63c, SInt16) |= 2;
        }
    }
    if (file[0x660])
        F(file, 0x63c, SInt16) |= 2;
    F(file, 0x63e, SInt16) = F(file, 0x63c, SInt16) & 1;
    if ((F(file, 0x63c, SInt16) & 1) && !(F(file, 0x638, UInt32) & kPrecompile) && forcePrecompile != 1 &&
        (F(file, 0x634, UInt32) & 0x80000000))
        F(file, 0x63e, SInt16) |= 2;
#ifdef RECOMP_NEGATIVE_CONTROL_CLDROPINCB5 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLDROPINCB5 */
    F(file, 0x63e, SInt16) |= 2;
#endif
#ifdef RECOMP_NEGATIVE_CONTROL_CLDROPINCB8 /* coarser: files are never compiled */
    F(file, 0x63c, SInt16) = 0;
#endif

    *whichfile = 0;
    index = F(entry, 0, SInt32); /* position */
    if (index != -1)
        failed = !Files_AddFile(gTarg + 0x2c, file);
    else
        failed = !Files_InsertFile(gTarg + 0x2c, file, index);
    if (failed)
        return 2; /* cwErrRequestFailed */
    *whichfile = F(file, 4, SInt32);
    F(context, 0xa4, SInt32)++; /* numFiles */

    if (F(gTarg, 0x28, SInt32) == 2) { /* LinkModel_Overlay1 */
        index = F(entry, 8, SInt32); /* overlayGroup */
        if (index == -1)
            index = Overlays_CountGroups(gTarg + 0x1c) - 1;
        ovg = Overlays_GetOvlGroup(gTarg + 0x1c, index);
        if (!ovg)
            return 2;
        index = F(entry, 0xc, SInt32); /* overlay */
        if (index == -1)
            index = OvlGroup_CountOverlays(ovg) - 1;
        oly = OvlGroup_GetOverlay(ovg, index);
        if (!oly)
            return 2;
        if (!Overlay_AddFile(oly, F(file, 4, SInt32), &idx))
            return 2;
    }
    return 0;
}
HOOK(0x425b50, UCBAddProjectEntry);
