/*
 * DropInCompilerLinkerPrivate.c — compiler/linker plugin API getters (mweppc
 * src/driver/DropInCompilerLinkerPrivate.cpp, extern "C" functions), partial port.
 *
 * Its 18 functions in our exe are byte-identical between mweppc's GameCube 2.0 compiler and ours.
 * Ported: the 12 simple ones the corpus runs, and (part 2) CWGetBrowseOptions and CWGetTargetInfo,
 * which call the exe's C++ copy operators. The CWTargetInfo_V8 operator, CWCachePrecompiledHeader and
 * CWGetPrecompiledHeaderSpec stay the original's. Layout of the context from our code:
 * pluginType +0xa0 ('Comp' / 'Link'), whichfile +0x108, sourcefile (FSSpec) +0x10c, sourcetext
 * +0x152, sourcetextsize +0x156, precompile +0x15a, autoprecompile +0x15b, preprocess +0x15c,
 * cachingPCH +0x15d, debuginfo +0x15e, fileID +0x160, callbacks +0x266 (cbStoreObjectData +8,
 * cbDisplayLines +0x10, both __stdcall).
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef int CWResult;
typedef UInt8 *Ctx;
#define cwNoErr 0
#define cwErrInvalidParameter 3
#define cwErrInvalidCallback 4
#define CTX(c, off, type) (*(type *)((c) + (off)))

typedef CWResult(__stdcall *StoreObjectData_t)(Ctx, SInt32, void *);
typedef CWResult(__stdcall *DisplayLines_t)(Ctx, SInt32);
typedef void *(__thiscall *FSSpecAssign_t)(void *self, const void *src);
#define FSSpec_assign ((FSSpecAssign_t)0x418320) /* FSSpec::operator= */

__attribute__((noinline)) Ctx DropInCompilerLinkerPrivate_GetContext(Ctx context) {
    if (context && (CTX(context, 0xa0, UInt32) == 0x436f6d70 /* 'Comp' */ ||
                    CTX(context, 0xa0, UInt32) == 0x4c696e6b /* 'Link' */))
        return context;
    return 0;
}
HOOK(0x42d5e0, DropInCompilerLinkerPrivate_GetContext);
#define GetContext DropInCompilerLinkerPrivate_GetContext

#define FLAG_GETTER(name, off)                                                               \
    CWResult __stdcall name(Ctx context, Bool8 *out) {                                             \
        Ctx cl;                                                                                    \
        if (!out)                                                                                  \
            return cwErrInvalidParameter;                                                          \
        if (!(cl = GetContext(context)))                                                           \
            return cwErrInvalidCallback;                                                           \
        *out = CTX(cl, off, Bool8);                                                                \
        return cwNoErr;                                                                            \
    }

FLAG_GETTER(CWIsPrecompiling, 0x15a)
HOOK(0x42d610, CWIsPrecompiling);
FLAG_GETTER(CWIsAutoPrecompiling, 0x15b)
HOOK(0x42d650, CWIsAutoPrecompiling);
FLAG_GETTER(CWIsPreprocessing, 0x15c)
HOOK(0x42d690, CWIsPreprocessing);
CWResult __stdcall CWIsGeneratingDebugInfo(Ctx context, Bool8 *out) {
    Ctx cl;
    if (!out)
        return cwErrInvalidParameter;
    if (!(cl = GetContext(context)))
        return cwErrInvalidCallback;
#ifndef RECOMP_NEGATIVE_CONTROL_DROPIN /* --cflags=-DRECOMP_NEGATIVE_CONTROL_DROPIN */
    *out = CTX(cl, 0x15e, Bool8);
#else
    *out = 0; /* always "no" */
#endif
    return cwNoErr;
}
HOOK(0x42d6d0, CWIsGeneratingDebugInfo);
FLAG_GETTER(CWIsCachingPrecompiledHeaders, 0x15d)
HOOK(0x42d710, CWIsCachingPrecompiledHeaders);

CWResult __stdcall CWGetMainFileNumber(Ctx context, SInt32 *fileNumber) {
    Ctx cl;
    if (!fileNumber)
        return cwErrInvalidParameter;
    if (!(cl = GetContext(context)))
        return cwErrInvalidCallback;
    *fileNumber = CTX(cl, 0x108, SInt32);
    return cwNoErr;
}
HOOK(0x42d990, CWGetMainFileNumber);

CWResult __stdcall CWGetMainFileID(Ctx context, SInt16 *fileID) {
    Ctx cl;
    if (!fileID)
        return cwErrInvalidParameter;
    if (!(cl = GetContext(context)))
        return cwErrInvalidCallback;
    *fileID = CTX(cl, 0x160, SInt16);
    return cwNoErr;
}
HOOK(0x42d9d0, CWGetMainFileID);

CWResult __stdcall CWGetMainFileSpec(Ctx context, void *fileSpec) {
    Ctx cl;
    if (!fileSpec)
        return cwErrInvalidParameter;
    if (!(cl = GetContext(context)))
        return cwErrInvalidCallback;
    FSSpec_assign(fileSpec, cl + 0x10c);
    return cwNoErr;
}
HOOK(0x42da10, CWGetMainFileSpec);

CWResult __stdcall CWGetMainFileText(Ctx context, const char **text, SInt32 *textLength) {
    Ctx cl;
    if (!text)
        return cwErrInvalidParameter;
    if (!textLength)
        return cwErrInvalidParameter;
    if (!(cl = GetContext(context)))
        return cwErrInvalidCallback;
    *text = CTX(cl, 0x152, const char *);
    *textLength = CTX(cl, 0x156, SInt32);
    return cwNoErr;
}
HOOK(0x42da60, CWGetMainFileText);

CWResult __stdcall CWStoreObjectData(Ctx context, SInt32 whichfile, void *object) {
    Ctx cl;
    if (!object)
        return cwErrInvalidParameter;
    if (!(cl = GetContext(context)))
        return cwErrInvalidCallback;
    return (*(StoreObjectData_t *)(CTX(cl, 0x266, UInt8 *) + 8))(cl, whichfile, object);
}
HOOK(0x42dac0, CWStoreObjectData);

CWResult __stdcall CWDisplayLines(Ctx context, SInt32 nlines) {
    Ctx cl;
    if (!(cl = GetContext(context)))
        return cwErrInvalidCallback;
    return (*(DisplayLines_t *)(CTX(cl, 0x266, UInt8 *) + 0x10))(cl, nlines);
}
HOOK(0x42db10, CWDisplayLines);

/* ---- part 2: browse options and target info, through the exe's C++ copy operators ---- */

/* Context: apiVersion +4, browseoptions +0x162, targetinfo_V7 +0x1a4 (outfile FSSpec +0x1a4,
   symfile +0x1ea, linkType (short) +0x230, canRun +0x232, canDebug +0x233, debuggerCreator
   +0x236, runHelperCreator +0x23a), targetinfo (pointer) +0x246. CWTargetInfo (0x136 bytes):
   outputType (short) +0, outfile +2, symfile +0x48, runfile +0x8e, linkType +0xd4, canRun +0xd6,
   canDebug +0xd7, debuggerCreator +0xe8, runHelperCreator +0xec. */
typedef void *(__thiscall *Assign_t)(void *self, const void *src);
#define CWBrowseOptions_assign ((Assign_t)0x42d7a0)
#define CWTargetInfo_assign ((Assign_t)0x418260)
#define CWTargetInfo_V8_assign ((Assign_t)0x42d8e0)

CWResult __stdcall CWGetBrowseOptions(Ctx context, void *browseOptions) {
    Ctx cl;
    if (!browseOptions)
        return cwErrInvalidParameter;
    if (!(cl = DropInCompilerLinkerPrivate_GetContext(context)))
        return cwErrInvalidCallback;
    CWBrowseOptions_assign(browseOptions, cl + 0x162);
    return cwNoErr;
}
HOOK(0x42d750, CWGetBrowseOptions);

CWResult __stdcall CWGetTargetInfo(Ctx context, UInt8 *targetInfo) {
    Ctx cl;
    int i;
    if (!targetInfo)
        return cwErrInvalidParameter;
    if (!(cl = DropInCompilerLinkerPrivate_GetContext(context)))
        return cwErrInvalidCallback;
#ifdef RECOMP_NEGATIVE_CONTROL_DICLP2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_DICLP2 */
    return cwErrInvalidCallback;
#endif
    if (CTX(cl, 4, SInt32) >= 10) {
        CWTargetInfo_assign(targetInfo, CTX(cl, 0x246, void *));
    } else if (CTX(cl, 4, SInt32) >= 8) {
        CWTargetInfo_V8_assign(targetInfo, CTX(cl, 0x246, void *));
    } else {
        for (i = 0; i < 0x136; i += 2) /* memset(targetInfo, 0, sizeof(CWTargetInfo)) */
            *(UInt16 *)(targetInfo + i) = 0;
        *(SInt16 *)targetInfo = cl[0x1aa] ? 1 /* linkOutputFile */ : 0 /* linkOutputNone */;
        FSSpec_assign(targetInfo + 2, cl + 0x1a4);
        FSSpec_assign(targetInfo + 0x48, cl + 0x1ea);
        FSSpec_assign(targetInfo + 0x8e, cl + 0x1a4);
        CTX(targetInfo, 0xd4, SInt16) = CTX(cl, 0x230, SInt16);
        targetInfo[0xd6] = cl[0x232];
        targetInfo[0xd7] = cl[0x233];
        CTX(targetInfo, 0xe8, UInt32) = CTX(cl, 0x236, UInt32);
        CTX(targetInfo, 0xec, UInt32) = CTX(cl, 0x23a, UInt32);
    }
    return cwNoErr;
}
HOOK(0x42d7e0, CWGetTargetInfo);
