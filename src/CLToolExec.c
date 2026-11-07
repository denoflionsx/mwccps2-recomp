/*
 * CLToolExec.c — running the compiler and linker over the project (mweppc
 * src/driver/CLToolExec.c), partial port.
 *
 * Ported so far: DeleteTemporaries, which runs after compiling and linking. The rest of the unit
 * (the tool callers) is not run by the corpus or not in our exe.
 *
 * File from our code: outfss +0x422, wroteToDisk +0x628, tempOnDisk +0x62a, objectUsage +0x63e
 * (all SInt16). gTarg->files at gTarg + 0x2c. optsCmdLine.verbose (SInt16) 0x57c94c.
 */
#include "orig.h"

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(UInt8 *, Files_GetFile, 0x423e10, (void *list, SInt32 idx));
ORIG_FN(SInt32, Files_Count, 0x423e90, (void *list));
ORIG_FN(int, GetOutputFile, 0x41adb0, (void *file, SInt16 stage));
ORIG_FN_STDCALL(char *, OS_SpecToString, 0x412fc0, (const void *spec, char *buf, int size));
ORIG_FN_STDCALL(int, OS_Delete, 0x4122d0, (const void *spec));
ORIG_FN(void, CLReport, 0x408b20, (int id, ...));

#define gTarg ORIG_VAR(UInt8 *, 0x57c760)
#define verbose ORIG_VAR(SInt16, 0x57c94c)
#define STSbuf ((char *)0x57cd9c)

int DeleteTemporaries(void) {
    SInt32 idx;
    UInt8 *file;

    for (idx = 0; idx < Files_Count(gTarg + 0x2c); idx++) {
        file = Files_GetFile(gTarg + 0x2c, idx);
        if ((F(file, 0x63e, SInt16) & 2) && (F(file, 0x62a, SInt16) & 2) && (F(file, 0x628, SInt16) & 2)) {
            GetOutputFile(file, 2);
#ifdef RECOMP_NEGATIVE_CONTROL_CLTOOLEXEC /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLTOOLEXEC */
            if (verbose >= 0) /* the deletion is always reported */
#else
            if (verbose > 1)
#endif
                CLReport(19, OS_SpecToString(file + 0x422, STSbuf, 0x104));
            OS_Delete(file + 0x422);
            F(file, 0x62a, SInt16) &= ~2;
            F(file, 0x628, SInt16) &= ~2;
        }
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CLTOOLEXEC2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLTOOLEXEC2 */
    return 0;
#endif
    return 1;
}
HOOK(0x419220, DeleteTemporaries);
