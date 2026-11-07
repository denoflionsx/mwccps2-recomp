/*
 * CLFileOps.c — the driver's per-file operations (mweppc src/driver/CLFileOps.c), partial port.
 *
 * Ported so far: OutputTextData, StoreObjectFile and set_bytes, which run after the compile has
 * returned. The functions the compile runs under (SyntaxCheckFile, PreprocessFile,
 * DependencyMapFile, CompileEntry, CompileFilesInProject) and GetOutputFile stay the original's,
 * since their frames would have to keep the original's size. Our StoreObjectFile has one check
 * mweppc lacks: an internal error when the object data was already flushed (File +0x664).
 *
 * File from our code: srcfilename +0x1c, outfss +0x422, writeToDisk +0x626, wroteToDisk +0x628,
 * tempOnDisk +0x62a (all SInt16), textdata +0x640, objectdata +0x644, browsedata +0x648.
 * optsCmdLine.toDisk (SInt16) 0x57c946, verbose (SInt16) 0x57c94c; optsCompiler.outMakefile
 * 0x57d808; clState.makefileSpec 0x57b364. Stage masks: Cg 2, Dp 8.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))
#define S(addr) ((const char *)(addr))

ORIG_FN_STDCALL(SInt32, GetHandleSize, 0x4149c0, (void *h));
ORIG_FN_STDCALL(void, DisposeHandle, 0x414900, (void *h));
ORIG_FN_STDCALL(char *, OS_SpecToStringRelative, 0x419d70, (const void *spec, const void *cwd, char *buf, int size));
ORIG_FN(void, CLReport, 0x408b20, (int id, ...));
ORIG_FN(Bool8, WriteHandleToFile, 0x415f80, (void *spec, void *h, SInt32 size, UInt32 creator, UInt32 type));
ORIG_FN(Bool8, AppendHandleToFile, 0x4160b0, (void *spec, void *h, SInt32 size, UInt32 creator, UInt32 type));
ORIG_FN(Bool8, ShowHandle, 0x415f20, (void *h, SInt32 size, Bool8 decorate));
ORIG_FN(int, GetOutputFile, 0x41adb0, (void *file, SInt16 stage));
ORIG_FN(int, RecordBrowseInfo, 0x41b440, (void *file)); /* not in the map */
ORIG_FN(int, RecordObjectData, 0x41b4f0, (void *file)); /* not in the map */
ORIG_FN(void, CLInternalError, 0x408c80, (const char *file, int line, const char *format, ...));

#define toDisk ORIG_VAR(SInt16, 0x57c946)
#define verbose ORIG_VAR(SInt16, 0x57c94c)
#define outMakefile ((char *)0x57d808)
#define makefileSpec ((void *)0x57b364)
#define STSbuf ((char *)0x57cd9c)

int OutputTextData(UInt8 *file, SInt16 stage, UInt32 maccreator, UInt32 mactype) {
    int ret = 0;
    SInt32 size;

    if (F(file, 0x640, void *)) {
        size = GetHandleSize(F(file, 0x640, void *));
#ifdef RECOMP_NEGATIVE_CONTROL_CLFILEOPS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLFILEOPS */
        size--; /* text output one byte short */
#endif
#ifdef RECOMP_NEGATIVE_CONTROL_CLFILEOPS2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLFILEOPS2 */
        size /= 2; /* text output cut in half */
#endif
        if (stage != 8 || (toDisk & 8)) {
            if (!(F(file, 0x626, SInt16) & stage)) {
                ret = ShowHandle(F(file, 0x640, void *), size, 0);
            } else {
                if (verbose)
                    CLReport(15, OS_SpecToStringRelative(file + 0x422, 0, STSbuf, 0x104));
                ret = WriteHandleToFile(file + 0x422, F(file, 0x640, void *), size, maccreator, mactype);
                F(file, 0x628, SInt16) |= stage;
            }
        } else if (outMakefile[0]) {
            ret = AppendHandleToFile(makefileSpec, F(file, 0x640, void *), size, maccreator, mactype);
        } else {
            ret = ShowHandle(F(file, 0x640, void *), size, 0);
        }
        DisposeHandle(F(file, 0x640, void *));
        F(file, 0x640, void *) = 0;
    }
    return ret;
}
HOOK(0x41acb0, OutputTextData);

int StoreObjectFile(UInt8 *file) {
    if (F(file, 0x628, SInt16) & 2)
        return 1;
    if (file[0x664])
        CLInternalError(S(0x52d6f8) /* "CLFileOps.c" */, 662, S(0x52d704) /* "Object data flushed before storing?\n" */);
    if (!F(file, 0x644, void *) && !F(file, 0x648, void *))
        return 1;
    if (GetOutputFile(file, 2)) {
        if (!((F(file, 0x62a, SInt16) | F(file, 0x626, SInt16)) & 2))
            return 1;
        if (!RecordBrowseInfo(file) || !RecordObjectData(file))
            return 0;
        F(file, 0x628, SInt16) |= 2;
        return 1;
    }
    return 0;
}
HOOK(0x41b580, StoreObjectFile);

void set_bytes(UInt32 size, int *amount, const char **unit) {
    *amount = size;
    *unit = S(0x52d774); /* "bytes" */
}
HOOK(0x41bde0, set_bytes);
