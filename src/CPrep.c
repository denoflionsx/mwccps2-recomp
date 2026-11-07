/*
 * CPrep.c — the preprocessor (mweppc src/compiler/frontend/CPrep.c), partial port.
 *
 * The functions here are byte-identical between mweppc's GameCube 2.0 compiler and ours. Ported so
 * far: the token stream state (get, set, flush), foundnl, newline, poptokenseq, CPrep_GetPrepPos
 * and the preprocessor's error and warning reporters. preprocessor, popfile, macrotest and the
 * rest stay the original's.
 *
 * Token stream elements are 32 bytes. tokenstack entries are 14 bytes: pos +0, macrostart +4,
 * macro +8 (isExpanding at +0xf), macrocheck +0xc. CPrepFileInfo.textbuffer +0x4e.
 * ERR_UNEXPECTED_END_FILE 0x66, ERR_ILLEGAL_TOKEN 0x69.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void, CError_Error, 0x456d50, (int code, ...));
ORIG_FN(void, CError_Warning, 0x4571c0, (int code, ...));
ORIG_FN(void, CError_UserBreak, 0x4572c0, (void));
ORIG_FN(void, CPrep_PreprocessDumpNewLine, 0x4e70c0, (void));
ORIG_FN(UInt32, COS_GetTicks, 0x441880, (void));
ORIG_FN_STDCALL(int, CWDisplayLines, 0x42db10, (void *context, SInt32 nlines));
ORIG_FN(void, freeaheap, 0x433450, (void));

#define ts_current ORIG_VAR(UInt8 *, 0x57c7f0)
#define ts_first ORIG_VAR(UInt8 *, 0x54ce9e)
#define ts_preread_elements ORIG_VAR(SInt32, 0x57d704)
#define tokenstacklevel ORIG_VAR(SInt16, 0x57db1e)
#define tokenstack ((UInt8 *)0x56b898)
#define dofreeaheap ORIG_VAR(Bool8, 0x54ceb6)
#define pos ORIG_VAR(char *, 0x57aba0)
#define macrostart ORIG_VAR(char *, 0x57d6f8)
#define macrocheck ORIG_VAR(Bool8, 0x57e16d)
#define spaceskip ORIG_VAR(Bool8, 0x57e157)
#define at_linestart ORIG_VAR(Bool8, 0x57e158)
#define nlflag ORIG_VAR(SInt16, 0x57dc74)
#define preprocessing_only ORIG_VAR(Bool8, 0x57e13b)
#define linenumber ORIG_VAR(SInt32, 0x57c800)
#define lines ORIG_VAR(SInt32, 0x57b768)
#define filesp ORIG_VAR(SInt16, 0x57dc72)
#define filestack0 ORIG_VAR(UInt8 *, 0x54d2f4)
#define lineoffset ORIG_VAR(SInt32, 0x54d2e2)
#define linetick ORIG_VAR(UInt32, 0x54d2ec)
#define cparamblkptr ORIG_VAR(void **, 0x57ca38)
#define prep_file ORIG_VAR(void *, 0x57c918)
#define prep_file_start ORIG_VAR(char *, 0x57c8fc)
#define prep_file_end ORIG_VAR(char *, 0x57c974)
#define in_assembler ORIG_VAR(Bool8, 0x57e03e)
#define was_prep_error ORIG_VAR(Bool8, 0x54d2e0)

void CPrep_TokenStreamGetState(SInt32 *state) {
    *state = (SInt32)(ts_current - ts_first) / 32;
}
HOOK(0x43ba70, CPrep_TokenStreamGetState);

void CPrep_TokenStreamSetState(SInt32 *state) {
    ts_preread_elements += (SInt32)(ts_current - ts_first) / 32 - *state;
    ts_current = ts_first + *state * 32;
}
HOOK(0x43ba90, CPrep_TokenStreamSetState);

void CPrep_TokenStreamFlush(void) {
    if (!ts_preread_elements)
        ts_current = ts_first;
}
HOOK(0x43c1b0, CPrep_TokenStreamFlush);

void CPrep_Error(SInt16 code) {
    Bool8 save = in_assembler;
    in_assembler = 0;
    if (code == 0x66 && (tokenstacklevel > 0 || pos < prep_file_end))
        code = 0x69;
    was_prep_error = 1;
    CError_Error(code);
    in_assembler = save;
}
HOOK(0x43c670, CPrep_Error);

void CPrep_ErrorName(SInt16 code, const char *name) {
    Bool8 save = in_assembler;
    in_assembler = 0;
    was_prep_error = 1;
    if ((UInt16)code == 0x66 && (tokenstacklevel > 0 || pos < prep_file_end))
        CError_Error(0x69);
    else
        CError_Error(code, name);
    in_assembler = save;
}
HOOK(0x43c6c0, CPrep_ErrorName);

void CPrep_Warning(SInt16 code) {
    Bool8 save = in_assembler;
    in_assembler = 0;
    was_prep_error = 1;
    CError_Warning(code);
    in_assembler = save;
}
HOOK(0x43c720, CPrep_Warning);

void CPrep_GetPrepPos(void **file, SInt32 *ppos) {
    *file = prep_file;
    if (tokenstacklevel > 0)
        *ppos = F(tokenstack, 0, char *) - prep_file_start;
    else
        *ppos = pos - prep_file_start;
}
HOOK(0x43cc50, CPrep_GetPrepPos);

void poptokenseq(void) {
    UInt8 *ent, *macro;

    if (!--tokenstacklevel && dofreeaheap)
        freeaheap();
    ent = tokenstack + tokenstacklevel * 14;
    pos = F(ent, 0, char *);
    macrostart = F(ent, 4, char *);
    if ((macro = F(ent, 8, UInt8 *)))
        macro[0xf] = 0; /* isExpanding */
    macrocheck = ent[0xc];
    spaceskip = 1;
}
HOOK(0x43d050, poptokenseq);

void foundnl(void) {
    if (!tokenstacklevel) {
        if (preprocessing_only)
            CPrep_PreprocessDumpNewLine();
#ifndef RECOMP_NEGATIVE_CONTROL_CPREP /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREP: line numbers stay put */
        linenumber++;
#endif
        lines++;
        if (filesp <= 0)
            lineoffset = pos - F(filestack0, 0x4e, char *);
    }
}
HOOK(0x43d380, foundnl);

void newline(void) {
    foundnl();
    nlflag = 1;
    at_linestart = 1;
    spaceskip = 1;
    if (COS_GetTicks() > linetick) {
        if (CWDisplayLines(cparamblkptr[0], lines) != 0)
            CError_UserBreak();
        linetick = COS_GetTicks() + 5;
    }
}
HOOK(0x43d3d0, newline);
