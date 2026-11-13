/*
 * CWPluginsPrivate.c — the plugin API's shell-side wrappers (mweppc
 * src/driver/CWPluginsPrivate.cpp, extern "C" functions), partial port.
 *
 * Its 29 functions in our exe are byte-identical between mweppc's GameCube 2.0 compiler and ours.
 * Ported: the 19 the corpus runs. EnsureCachedAccessPaths, CWGetAccessPathListInfo, CWGetFileText,
 * CWAlert, CWShowStatus, CWSetModDate and the four CW*MemHandle functions stay the original's.
 * Layout of the plugin context from our code: request +0, apiVersion +4, projectFile (FSSpec)
 * +0x10, shellSignature +0x9c ('CWIE'), pluginType +0xa0 ('VCS '), numFiles +0xa4, callbacks +0x104.
 * Callback slots (all __stdcall): cbGetFileInfo +0, cbFindAndLoadFile +4, cbReleaseFileText +0xc,
 * cbReportMessage +0x20, cbUserBreak +0x2c, cbGetNamedPreferences +0x30, cbAddProjectEntry +0x40,
 * cbCreateNewTextDocument +0x44, cbGetTargetName +0x88, cbCheckoutLicense +0xa0,
 * cbCheckinLicense +0xa4. The three static checks are cdecl functions of their own.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef int CWResult;
typedef UInt8 *Ctx;
#define cwNoErr 0
#define cwErrRequestFailed 2
#define cwErrInvalidParameter 3
#define reqInitialize (-2)
#define reqTerminate (-1)
#define reqIdle (-100)
#define CTX(c, off, type) (*(type *)((c) + (off)))
#define CALLBACK(c, off, type) (*(type *)(CTX(c, 0x104, UInt8 *) + (off)))

typedef void *(__thiscall *FSSpecAssign_t)(void *self, const void *src);
#define FSSpec_assign ((FSSpecAssign_t)0x418320) /* FSSpec::operator= */

typedef CWResult(__stdcall *cbGetFileInfo_t)(Ctx, SInt32, Bool8, void *);
typedef CWResult(__stdcall *cbFindAndLoadFile_t)(Ctx, const char *, void *);
typedef CWResult(__stdcall *cbReleaseFileText_t)(Ctx, const char *);
typedef CWResult(__stdcall *cbReportMessage_t)(Ctx, const void *, const char *, const char *, SInt16, SInt32);
typedef CWResult(__stdcall *cbUserBreak_t)(Ctx);
typedef CWResult(__stdcall *cbGetNamedPreferences_t)(Ctx, const char *, void *);
typedef CWResult(__stdcall *cbAddProjectEntry_t)(Ctx, const void *, Bool8, const void *, SInt32 *);
typedef CWResult(__stdcall *cbCreateNewTextDocument_t)(Ctx, const void *);
typedef CWResult(__stdcall *cbGetTargetName_t)(Ctx, char *, SInt16);
typedef CWResult(__stdcall *cbCheckoutLicense_t)(Ctx, const char *, const char *, SInt32, void *, SInt32 *);
typedef CWResult(__stdcall *cbCheckinLicense_t)(Ctx, SInt32);

__attribute__((noinline)) Bool8 CWPluginsPrivate_ValidateContext(Ctx context) {
    SInt32 request;
    if (!context || CTX(context, 0x9c, UInt32) != 0x43574945 /* 'CWIE' */)
        return 0;
    request = CTX(context, 0, SInt32);
    return !(request == reqInitialize || request == reqTerminate || request == reqIdle);
}
HOOK(0x41f430, CWPluginsPrivate_ValidateContext);
#define ValidateContext CWPluginsPrivate_ValidateContext

__attribute__((noinline)) Bool8 CWPluginsPrivate_ValidateInitTermContext(Ctx context) {
    SInt32 request;
    if (!context || CTX(context, 0x9c, UInt32) != 0x43574945)
        return 0;
    request = CTX(context, 0, SInt32);
    return request == reqInitialize || request == reqTerminate || request == reqIdle;
}
HOOK(0x41f480, CWPluginsPrivate_ValidateInitTermContext);
#define ValidateInitTermContext CWPluginsPrivate_ValidateInitTermContext

__attribute__((noinline)) Bool8 CWPluginsPrivate_IsVCSContext(Ctx context) {
    return context && CTX(context, 0xa0, UInt32) == 0x56435320; /* 'VCS ' */
}
HOOK(0x41f4d0, CWPluginsPrivate_IsVCSContext);
#define IsVCSContext CWPluginsPrivate_IsVCSContext

#define ANY_CONTEXT(c) (ValidateContext(c) || ValidateInitTermContext(c))
#define PROJECT_CONTEXT(c) (!IsVCSContext(c) && ValidateContext(c))

CWResult __stdcall CWGetPluginRequest(Ctx context, SInt32 *request) {
    if (!ANY_CONTEXT(context))
        return cwErrInvalidParameter;
    if (!request)
        return cwErrInvalidParameter;
    *request = CTX(context, 0, SInt32);
    return cwNoErr;
}
HOOK(0x41f4f0, CWGetPluginRequest);

CWResult __stdcall CWGetAPIVersion(Ctx context, SInt32 *version) {
    if (!ANY_CONTEXT(context))
        return cwErrInvalidParameter;
    if (!version)
        return cwErrInvalidParameter;
    *version = CTX(context, 4, SInt32);
    return cwNoErr;
}
HOOK(0x41f540, CWGetAPIVersion);

CWResult __stdcall CWGetProjectFile(Ctx context, void *projectSpec) {
    if (!PROJECT_CONTEXT(context))
        return cwErrInvalidParameter;
    if (!projectSpec)
        return cwErrInvalidParameter;
    FSSpec_assign(projectSpec, context + 0x10);
    return cwNoErr;
}
HOOK(0x41f590, CWGetProjectFile);

CWResult __stdcall CWGetTargetName(Ctx context, char *name, SInt16 maxLength) {
    if (!PROJECT_CONTEXT(context))
        return cwErrInvalidParameter;
    if (CTX(context, 4, SInt32) >= 8)
        return CALLBACK(context, 0x88, cbGetTargetName_t)(context, name, maxLength);
    return cwErrRequestFailed;
}
HOOK(0x41f5f0, CWGetTargetName);

CWResult __stdcall CWGetProjectFileCount(Ctx context, SInt32 *count) {
    if (!PROJECT_CONTEXT(context))
        return cwErrInvalidParameter;
    if (!count)
        return cwErrInvalidParameter;
#ifndef RECOMP_NEGATIVE_CONTROL_CWPLUGINS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CWPLUGINS */
    *count = CTX(context, 0xa4, SInt32);
#else
    *count = 0; /* no files */
#endif
    return cwNoErr;
}
HOOK(0x41f640, CWGetProjectFileCount);

CWResult __stdcall CWGetFileInfo(Ctx context, SInt32 whichfile, Bool8 checkFileLocation, void *fileinfo) {
    if (!PROJECT_CONTEXT(context))
        return cwErrInvalidParameter;
    if (!fileinfo)
        return cwErrInvalidParameter;
    return CALLBACK(context, 0, cbGetFileInfo_t)(context, whichfile, checkFileLocation, fileinfo);
}
HOOK(0x41f690, CWGetFileInfo);

CWResult __stdcall CWFindAndLoadFile(Ctx context, const char *filename, void *fileinfo) {
    if (!PROJECT_CONTEXT(context))
        return cwErrInvalidParameter;
    if (!filename)
        return cwErrInvalidParameter;
    if (!fileinfo)
        return cwErrInvalidParameter;
    return CALLBACK(context, 4, cbFindAndLoadFile_t)(context, filename, fileinfo);
}
HOOK(0x41f6f0, CWFindAndLoadFile);

CWResult __stdcall CWReleaseFileText(Ctx context, const char *text) {
    if (!ValidateContext(context))
        return cwErrInvalidParameter;
    if (!text)
        return cwNoErr;
    return CALLBACK(context, 0xc, cbReleaseFileText_t)(context, text);
}
HOOK(0x41f8c0, CWReleaseFileText);

CWResult __stdcall CWReportMessage(Ctx context, const void *msgRef, const char *line1, const char *line2,
                                   SInt16 errorlevel, SInt32 errorNumber) {
    if (!ValidateContext(context))
        return cwErrInvalidParameter;
    return CALLBACK(context, 0x20, cbReportMessage_t)(context, msgRef, line1, line2, errorlevel, errorNumber);
}
HOOK(0x41f900, CWReportMessage);

CWResult __stdcall CWUserBreak(Ctx context) {
    if (!ValidateContext(context))
        return cwErrInvalidParameter;
    return CALLBACK(context, 0x2c, cbUserBreak_t)(context);
}
HOOK(0x41f9e0, CWUserBreak);

CWResult __stdcall CWGetNamedPreferences(Ctx context, const char *prefsname, void *prefsdata) {
    if (!ValidateContext(context))
        return cwErrInvalidParameter;
    if (!prefsname)
        return cwErrInvalidParameter;
    if (!prefsdata)
        return cwErrInvalidParameter;
    return CALLBACK(context, 0x30, cbGetNamedPreferences_t)(context, prefsname, prefsdata);
}
HOOK(0x41fa10, CWGetNamedPreferences);

CWResult __stdcall CWAddProjectEntry(Ctx context, const void *fileSpec, Bool8 isGenerated,
                                     const void *projectEntryInfo, SInt32 *whichfile) {
    if (!PROJECT_CONTEXT(context))
        return cwErrInvalidParameter;
    if (!fileSpec)
        return cwErrInvalidParameter;
    if (CTX(context, 4, SInt32) < 8)
        return cwErrRequestFailed;
    return CALLBACK(context, 0x40, cbAddProjectEntry_t)(context, fileSpec, isGenerated, projectEntryInfo, whichfile);
}
HOOK(0x41fad0, CWAddProjectEntry);

CWResult __stdcall CWCreateNewTextDocument(Ctx context, const void *docinfo) {
    if (!ValidateContext(context))
        return cwErrInvalidParameter;
    if (!docinfo)
        return cwErrInvalidParameter;
    return CALLBACK(context, 0x44, cbCreateNewTextDocument_t)(context, docinfo);
}
HOOK(0x41fb40, CWCreateNewTextDocument);

CWResult __stdcall CWDonePluginRequest(Ctx context, CWResult resultCode) {
    (void)context;
    return resultCode;
}
HOOK(0x41fd00, CWDonePluginRequest);

CWResult __stdcall CWCheckoutLicense(Ctx context, const char *featureName, const char *licenseVersion,
                                     SInt32 flags, void *reserved, SInt32 *cookie) {
    if (!ANY_CONTEXT(context))
        return cwErrInvalidParameter;
    return CALLBACK(context, 0xa0, cbCheckoutLicense_t)(context, featureName, licenseVersion, flags, reserved, cookie);
}
HOOK(0x41fd10, CWCheckoutLicense);

CWResult __stdcall CWCheckinLicense(Ctx context, SInt32 cookie) {
    if (!ANY_CONTEXT(context))
        return cwErrInvalidParameter;
    return CALLBACK(context, 0xa4, cbCheckinLicense_t)(context, cookie);
}
HOOK(0x41fd60, CWCheckinLicense);
