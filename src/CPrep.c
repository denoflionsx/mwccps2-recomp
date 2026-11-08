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

/* ---- part 2: CPrep_BrowserFileOffset and CPrep_UnLex (close to mweppc: same logic) ---- */

/* Token stream element (32 bytes): tokenoffset +8. CPrepFileInfo.textbuffer +0x4e. */
SInt32 CPrep_BrowserFileOffset(void) {
    if (ts_first < ts_current)
        return F(ts_current - 32, 8, SInt32) + 1;
#ifdef RECOMP_NEGATIVE_CONTROL_CPREP2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREP2 */
    *(volatile int *)0 = 0; /* crash: a wrong offset is not caught (browse output is off) */
#endif
    if (tokenstacklevel)
        return F(tokenstack, 0, char *) - F((&filestack0)[filesp], 0x4e, char *);
    return pos - F((&filestack0)[filesp], 0x4e, char *);
}
HOOK(0x4416b0, CPrep_BrowserFileOffset);

ORIG_FN(void, CError_Internal_, 0x457230, (const char *file, int line));

void CPrep_UnLex(void) {
    ++ts_preread_elements;
    ts_current -= 32;
    if (ts_current < ts_first)
        CError_Internal_((const char *)0x534754 /* "CPrep.c" */, 901);
}
HOOK(0x43bad0, CPrep_UnLex);

/* ---- part 3: stream and macro-name helpers not matched by gcmatch (the same logic as mweppc's,
   without AltiVec's vector macro) ---- */

/* macrohashtable 0x57b124 (indexed by HashNameNode.hashval +8); Macro: next +0, name +4, nargs
   (short) +0xc, is_special (byte) +0xe, isExpanding (byte) +0xf. Special macros: cpplM
   (__cplusplus) 0x54cf28, ecppM (__embedded_cplusplus) 0x54ceec. copts.ecplusplus 0x57db7a.
   waslockedmacro 0x54d2eb. dummyelement 0x54ce32. tk 0x57d73a, tkidentifier 0x57c8dc. */
ORIG_FN(void, gotonexttoken, 0x43d430, (void));
ORIG_FN(void, prepmacro, 0x43ee60, (void *macro));
ORIG_FN(SInt16, lex, 0x47bb00, (void));
ORIG_FN(SInt16, notendofline, 0x43d4f0, (void));
ORIG_FN(void, skipendofline, 0x479220, (void));
#define macrohashtable ORIG_VAR(void ***, 0x57b124)
#define tk ORIG_VAR(SInt16, 0x57d73a)
#define tkidentifier ORIG_VAR(UInt8 *, 0x57c8dc)
#define cplusplus_ ORIG_VAR(Bool8, 0x57db79)

#define ecplusplus_ ORIG_VAR(Bool8, 0x57db7a)
#define waslockedmacro ORIG_VAR(Bool8, 0x54d2eb)

void *ismacroname(void) {
    UInt8 *name = tkidentifier;
    UInt8 *scan;
    Bool8 save_macrocheck;

    for (scan = (UInt8 *)macrohashtable[F(name, 8, SInt16)]; scan; scan = *(UInt8 **)scan) {
        if (F(scan, 4, UInt8 *) != name)
            continue;
        if (scan[0xe]) {
            if (scan == (UInt8 *)0x54cf28 && !cplusplus_)
                return 0;
            if (scan == (UInt8 *)0x54ceec && (!cplusplus_ || !ecplusplus_))
                return 0;
        }
        if (scan[0xf]) {
            waslockedmacro = 1;
            return 0;
        }
        if (F(scan, 0xc, SInt16)) {
            save_macrocheck = macrocheck;
            macrocheck = 0;
            gotonexttoken();
            macrocheck = save_macrocheck;
            if (*pos != '(')
                return 0;
        }
        return scan;
    }
    return 0;
}
HOOK(0x43d150, ismacroname);

Bool8 macrotest(void) {
    void *macro = ismacroname();
    if (macro) {
        prepmacro(macro);
        return 1;
    }
    return 0;
}
HOOK(0x43eed0, macrotest);

void *CPrep_CurStreamElement(void) {
    if (ts_first < ts_current)
        return ts_current - 32;
    return (void *)0x54ce32; /* dummyelement */
}
HOOK(0x43c390, CPrep_CurStreamElement);

void CPrep_TokenStreamSetCurState(SInt32 *state) {
    ts_preread_elements += (SInt32)(ts_current - ts_first) / 32 - (*state - 1);
    ts_current = ts_first + (*state - 1) * 32;
    tk = lex();
}
HOOK(0x43bb00, CPrep_TokenStreamSetCurState);

void goendofline(void) {
    if (notendofline()) {
        CPrep_Error(0x71); /* end of line expected */
        skipendofline();
    }
}
HOOK(0x43d5c0, goendofline);

static void **find_macro(UInt8 *name) {
    void **scan;

    for (scan = macrohashtable[F(name, 8, SInt16)]; scan; scan = scan[0]) {
        if (scan[1] == name) {
            if (((UInt8 *)scan)[0xe] && scan == (void **)0x54cf28 && !cplusplus_)
                return 0;
            return scan;
        }
    }
    return 0;
}

void *ismacroname3(void) {
#ifdef RECOMP_NEGATIVE_CONTROL_CPREP3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREP3 */
    return 0; /* no identifier is a macro here */
#endif
    return find_macro(tkidentifier);
}
HOOK(0x43d300, ismacroname3);

void *ismacroname4(UInt8 *name) {
    return find_macro(name);
}
HOOK(0x43d340, ismacroname4);

/* ---- part 4: notendofline and pushifstate (not matched by gcmatch; the same logic as mweppc's) ---- */

/* iflevel (short) 0x54d374; ifstack entries (10 bytes) from 0x54d376: file +0, pos +4, state
   (short) +8. */
ORIG_FN(SInt16, prepskipnextchar, 0x477b40, (void));
ORIG_FN(char *, ReadIdentifier, 0x4787a0, (const char *p));
#define iflevel ORIG_VAR(SInt16, 0x54d374)

Bool8 notendofline_(void) {
    SInt16 t;
    char c;
    char *save;
    void *macro;

    for (;;) {
        t = prepskipnextchar();
        c = (char)t;
        if (c == 0) {
            if (tokenstacklevel) {
                poptokenseq();
                continue;
            }
            return 0;
        }
        if (c == '\r')
            return 0;
        if (macrocheck && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')) {
            save = pos;
            pos = ReadIdentifier(pos);
            if ((macro = ismacroname()) != 0) {
                prepmacro(macro);
                continue;
            }
            pos = save;
        }
        return 1;
    }
}
HOOK(0x43d4f0, notendofline_);

void pushifstate(SInt16 state) {
    UInt8 *entry;

    if (iflevel >= 64) {
        CPrep_Error(0xd7); /* if nesting overflow */
        return;
    }
    entry = (UInt8 *)0x54d376 + iflevel * 10;
    *(SInt16 *)(entry + 8) = state;
    *(void **)entry = prep_file;
    *(SInt32 *)(entry + 4) = pos - prep_file_start;
#ifdef RECOMP_NEGATIVE_CONTROL_CPREP4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREP4 */
    *(SInt16 *)(entry + 8) = 0;
#endif
    iflevel++;
}
HOOK(0x440cc0, pushifstate);

/* popfile and CPrep_GetFileOffsetInfo. CPrepFileInfo: textbuffer +0x4e, textlength +0x52,
   linenumber +0x56, pos +0x5a, hasprepline (byte) +0x5e. cprep_hasprepline 0x54d2df,
   copts.simple_prepdump 0x57dba7. FileOffsetInfo: file +0, tokenline +4, tokenoffset +8,
   is_inline (byte) +0xc. TStreamElement: tokenfile +4, tokenoffset +8, tokenline +0xc (32 bytes). */
ORIG_FN_STDCALL(int, CWReleaseFileText, 0x41f8c0, (void *context, void *text));
ORIG_FN(void, CPrep_PreprocessDumpFileInfo, 0x4e7100, (Bool8 flag));
#define cprep_hasprepline ORIG_VAR(Bool8, 0x54d2df)
#define simple_prepdump ORIG_VAR(Bool8, 0x57dba7)
#define FILESTACK(i) ((&filestack0)[i])

void popfile_(void) {
    UInt8 *file;

    if (filesp < 0)
        return;
    CWReleaseFileText(*(void **)cparamblkptr, F(prep_file, 0x4e, void *));
    F(prep_file, 0x4e, void *) = 0;
    filesp--;
    if (filesp >= 0) {
        file = FILESTACK(filesp);
        prep_file = file;
        prep_file_start = F(file, 0x4e, char *);
        prep_file_end = F(file, 0x4e, char *) + F(file, 0x52, SInt32);
        pos = prep_file_start + F(file, 0x5a, SInt32);
        linenumber = F(file, 0x56, SInt32);
        cprep_hasprepline = file[0x5e];
        at_linestart = 1;
    }
    if (preprocessing_only && !simple_prepdump)
        CPrep_PreprocessDumpFileInfo(1);
}
HOOK(0x43c780, popfile_);

void CPrep_GetFileOffsetInfo(UInt8 *foi, UInt8 *ts) {
    UInt8 *file;

    if (ts && F(ts, 4, void *)) {
        F(foi, 0, void *) = F(ts, 4, void *);
        F(foi, 4, SInt32) = F(ts, 0xc, SInt32);
        F(foi, 8, SInt32) = F(ts, 8, SInt32);
    } else if (ts_first < ts_current) {
        F(foi, 0, void *) = F(ts_current - 32, 4, void *);
        F(foi, 4, SInt32) = F(ts_current - 32, 0xc, SInt32);
        F(foi, 8, SInt32) = F(ts_current - 32, 8, SInt32);
    } else {
        file = FILESTACK(filesp);
        F(foi, 0, void *) = file;
        F(foi, 4, SInt32) = linenumber;
        if (tokenstacklevel)
            F(foi, 8, SInt32) = F(tokenstack, 0, char *) - F(file, 0x4e, char *);
        else
            F(foi, 8, SInt32) = pos - F(file, 0x4e, char *);
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CPREP4B /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREP4B */
    F(foi, 4, SInt32) += 1;
#endif
    foi[0xc] = 0;
}
HOOK(0x43c960, CPrep_GetFileOffsetInfo);

/* ---- part 5: insertmacro and CPrep_InsertSpecialMacro (not matched by gcmatch; the same logic as
   mweppc's) ---- */

ORIG_FN(void *, GetHashNameNodeExport_p, 0x432ef0, (const char *name));

void insertmacro(UInt8 *macro) {
    SInt16 hash = F(F(macro, 4, UInt8 *), 8, SInt16);

    F(macro, 0, void *) = macrohashtable[hash];
    macrohashtable[hash] = (void **)macro;
    macro[0xf] = 0; /* isExpanding */
}
HOOK(0x43af80, insertmacro);

void CPrep_InsertSpecialMacro(UInt8 *macro, const char *name) {
    F(macro, 4, void *) = GetHashNameNodeExport_p(name);
    macro[0xe] = 1; /* is_special */
#ifdef RECOMP_NEGATIVE_CONTROL_CPREP5 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREP5 */
    return; /* special macros (__LINE__, __FILE__...) not defined */
#endif
    insertmacro(macro);
}
HOOK(0x43afb0, CPrep_InsertSpecialMacro);

/* ---- part 6: the pragma push (from the exe) ---- */

/* Saved option list 0x54cebc (next +0, option offset +4, in use (byte) +8, saved value +9):
   the first free node is reused, unless a node in use for the same option comes first (then a
   new node goes on the front); the option byte (copts 0x57db28 + offset) is saved and set. */
ORIG_FN(void *, galloc_p, 0x4332e0, (SInt32 size));
#define pragma_stack ORIG_VAR(UInt8 *, 0x54cebc)

void fn_43ef20(SInt32 offset, UInt8 value) {
    UInt8 *node;

    for (node = pragma_stack; node; node = *(UInt8 **)node) {
        if (!node[8])
            goto use;
        if (*(SInt32 *)(node + 4) == offset)
            break;
    }
    node = galloc_p(10);
    *(UInt8 **)node = pragma_stack;
    pragma_stack = node;
use:
    node[9] = ((UInt8 *)0x57db28)[offset];
    *(SInt32 *)(node + 4) = offset;
    node[8] = 1;
#ifdef RECOMP_NEGATIVE_CONTROL_CPREP6 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREP6 */
    value ^= 1;
#endif
    ((UInt8 *)0x57db28)[offset] = value;
}
HOOK(0x43ef20, fn_43ef20);
