/*
 * CPrepTokenizer.c — the preprocessor's tokenizer (mweppc src/compiler/frontend/CPrepTokenizer.c),
 * partial port.
 *
 * The functions here are byte-identical between mweppc's GameCube 2.0 compiler and ours. Ported so
 * far: plex, lookahead, the token handlers tnull, tiden, thash and tcret (reached through the
 * cprep_tokenize table, which holds the original addresses), prepcurchar and ReadIdentifier.
 * Part 2 adds CPrep_SkipNewComment and skipendoflinematch. The big character readers
 * (prepskipnextchar, prepnextchar, lex) stay the original's for now.
 * Tokens: TK_ILLEGAL -6, TK_IDENTIFIER -3, TK_EOL -7.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef SInt16 (*TokenFunc)(SInt16 t);

ORIG_FN(SInt16, prepskipnextchar, 0x477b40, (void));
ORIG_FN(SInt16, prepnextchar, 0x477fc0, (void));
ORIG_FN(SInt16, lex, 0x47bb00, (void));
ORIG_FN(void, poptokenseq, 0x43d050, (void));
ORIG_FN(void, popfile, 0x43c780, (void));
ORIG_FN(void, CPrep_TokenStreamGetState, 0x43ba70, (SInt32 *state));
ORIG_FN(void, CPrep_TokenStreamSetState, 0x43ba90, (SInt32 *state));
ORIG_FN(Bool8, macrotest, 0x43eed0, (void));
ORIG_FN(void, preprocessor, 0x4413b0, (void));
ORIG_FN(void, newline, 0x43d3d0, (void));
ORIG_FN(void, foundnl, 0x43d380, (void));
ORIG_FN(Bool8, COS_IsMultiByte, 0x4418b0, (const void *str, const void *p));
ORIG_FN(void *, GetHashNameNodeExport, 0x432ef0, (const char *name));

#define pos ORIG_VAR(char *, 0x57aba0)
#define nextcharpos ORIG_VAR(char *, 0x57ab90)
#define prep_file_end ORIG_VAR(char *, 0x57c974)
#define tokenstacklevel ORIG_VAR(SInt16, 0x57db1e)
#define filesp ORIG_VAR(SInt16, 0x57dc72)
#define at_linestart ORIG_VAR(Bool8, 0x57e158)
#define macrocheck ORIG_VAR(Bool8, 0x57e16d)
#define cprep_eoltokens ORIG_VAR(Bool8, 0x57e0c2)
#define multibyteaware ORIG_VAR(Bool8, 0x57db8c) /* copts */
#define tkidentifier ORIG_VAR(void *, 0x57c8dc)
#define cprep_tokenize ((TokenFunc *)0x538a94)
#define cprep_idarray ((const UInt8 *)0x57aa38)

SInt16 plex(void) {
    SInt16 t;
    for (;;) {
        t = prepskipnextchar();
        pos = nextcharpos;
        if ((t = cprep_tokenize[t](t)))
            return t;
        if (!*pos) {
            if (tokenstacklevel > 0)
                poptokenseq();
            else if (filesp > 0)
                popfile();
            else
                break;
        }
    }
    return 0;
}
HOOK(0x47be30, plex);

SInt16 lookahead(void) {
    SInt32 state[2];
    SInt16 t;
    CPrep_TokenStreamGetState(state);
    t = lex();
    CPrep_TokenStreamSetState(state);
    return t;
}
HOOK(0x47b890, lookahead);

SInt16 tnull(SInt16 t) {
    if (tokenstacklevel > 0 || pos >= prep_file_end) {
        at_linestart = 0;
        return t;
    }
    pos[-1] = (char)0xc0;
    return -6; /* TK_ILLEGAL */
}
HOOK(0x47b830, tnull);

char *ReadIdentifier(const char *p);

SInt16 tiden(SInt16 t) {
    (void)t;
    at_linestart = 0;
    pos = ReadIdentifier(pos - 1);
    if (macrocheck && macrotest())
        return 0;
    return -3; /* TK_IDENTIFIER */
}
HOOK(0x47a280, tiden);

SInt16 thash(SInt16 t) {
    (void)t;
    if (at_linestart) {
        preprocessor();
        return 0;
    }
    return '#';
}
HOOK(0x479a60, thash);

SInt16 tcret(SInt16 t) {
    (void)t;
    newline();
    return cprep_eoltokens ? -7 /* TK_EOL */ : 0;
}
HOOK(0x4794b0, tcret);

SInt16 prepcurchar(void) {
    char *save_pos = pos;
    SInt16 t = prepnextchar();
    nextcharpos = pos;
    pos = save_pos;
    return t;
}
HOOK(0x478730, prepcurchar);

char *ReadIdentifier(const char *p) {
    UInt8 buf[256];
    const char *start = p;
    int len = 0;

    for (;;) {
        if (cprep_idarray[(UInt8)*p]) {
            if (len < 255)
                buf[len++] = *p;
            p++;
        } else if ((UInt8)*p == '\\' && (!multibyteaware || !COS_IsMultiByte(start, p)) && p[1] == '\r') {
            p += p[2] == '\n' ? 3 : 2;
            foundnl();
        } else {
            break;
        }
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CPREPTOKENIZER /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREPTOKENIZER */
    if (len > 8)
        len = 8; /* identifiers cut to 8 characters */
#endif
    buf[len] = 0;
    tkidentifier = GetHashNameNodeExport((char *)buf);
    return (char *)p;
}
HOOK(0x4787a0, ReadIdentifier);

/* ---- part 2: comment and end-of-line skipping ---- */

/* CPrep_SkipNewCommentChar 0x57e14a, InlineAsm_gccmode 0x57e16e, copts.trigraphs 0x57db7e. */
#define CPrep_SkipNewCommentChar ORIG_VAR(UInt8, 0x57e14a)
#define InlineAsm_gccmode ORIG_VAR(Bool8, 0x57e16e)
#define trigraphs ORIG_VAR(Bool8, 0x57db7e)

UInt8 *CPrep_SkipNewComment(UInt8 *str) {
    UInt8 *p = str;
    for (;;) {
        switch (*(p++)) {
        case 0:
            if (tokenstacklevel > 0 || (char *)p >= prep_file_end) {
                CPrep_SkipNewCommentChar = 0;
                return p - 1;
            }
            p[-1] = ' ';
            continue;
        case '\r':
#ifdef RECOMP_NEGATIVE_CONTROL_CPREPTOKENIZER2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREPTOKENIZER2 */
            continue; /* // comments run on past the line end */
#endif
            CPrep_SkipNewCommentChar = '\r';
            return p;
        case '\\':
            if (multibyteaware && COS_IsMultiByte(str, p - 1))
                continue;
            if (*p == '\r') {
                ++p;
                foundnl();
            }
            if (*p == '\n')
                p++;
            continue;
        case '"':
            if (InlineAsm_gccmode) {
                CPrep_SkipNewCommentChar = '\r';
                return p - 1;
            }
            continue;
        }
    }
}
HOOK(0x479080, CPrep_SkipNewComment);

Bool8 skipendoflinematch(UInt8 *str, SInt16 token) {
    UInt8 *start = str - 1;
    pos = (char *)start;

    for (;;) {
        switch (*(str++)) {
        case 0:
        case '\r':
            pos = (char *)str - 1;
            return 0;
        case '"':
            if (token == '"') {
                pos = (char *)str;
                return 1;
            }
            break;
        case '\'':
            if (token == '\'') {
                pos = (char *)str;
                return 1;
            }
            break;
        case '\\':
            if (multibyteaware && COS_IsMultiByte(start, str - 1))
                continue;
            switch (*(str++)) {
            case 0:
                pos = (char *)str - 1;
                return 0;
            case '\r':
                foundnl();
                if (*str == '\n')
                    str++;
                break;
            }
            break;
        case '?':
            if (trigraphs && str[0] == '?' && str[1] == '/' && str[2] == '\r') {
                foundnl();
                str += 3;
            }
            break;
        }
    }
}
HOOK(0x479120, skipendoflinematch);
