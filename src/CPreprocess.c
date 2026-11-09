/*
 * CPreprocess.c — preprocessor output (-E) (mweppc src/compiler/frontend/CPreprocess.c), partial
 * port.
 *
 * Ported so far: CPrep_PreprocessDumpNewLine (byte-identical with mweppc's GameCube 2.0 compiler).
 * copts.line_prepdump 0x57dba8, pplist 0x56e748 (data +0), filesp 0x57dc72; the newline is the
 * exe's 2-byte string at 0x543810.
 */
#include "orig.h"

ORIG_FN(void, AppendGListData_pp, 0x432b50, (void *list, const void *data, SInt32 size));

void CPrep_PreprocessDumpNewLine(void) {
#ifdef RECOMP_NEGATIVE_CONTROL_CPREPROCESS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREPROCESS */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "CPREPROCESS@"); /* marks runs */
#endif
    if (*(UInt8 *)0x57dba8 && *(void **)0x56e748 && *(SInt16 *)0x57dc72 >= 0)
        AppendGListData_pp((void *)0x56e748, (const void *)0x543810, 2);
}
HOOK(0x4e70c0, CPrep_PreprocessDumpNewLine);
