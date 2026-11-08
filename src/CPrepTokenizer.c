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

/* ---- part 3: skipendofline and lexidentifier (close to mweppc: same logic) ---- */

/* copts.ANSIstrict 0x57db81, cplusplus 0x57db79; spaceskip 0x57e157. */
ORIG_FN(void, CPrep_Error, 0x43c670, (SInt16 code));
#define ANSIstrict ORIG_VAR(Bool8, 0x57db81)
#define cplusplus ORIG_VAR(Bool8, 0x57db79)
#define spaceskip ORIG_VAR(Bool8, 0x57e157)

/* Skips backslash-newline continuations at p. */
static UInt8 *skip_continuations(UInt8 *start, UInt8 *p) {
    while (*p == '\\' && !(multibyteaware && COS_IsMultiByte(start, p)) && p[1] == '\r') {
        foundnl();
        p += p[2] == '\n' ? 3 : 2;
    }
    return p;
}

void skipendofline(void) {
    UInt8 *start = (UInt8 *)pos;
    UInt8 *p = start;
    SInt16 ch;

    for (;;) {
        switch (*(p++)) {
        case '\r':
#ifdef RECOMP_NEGATIVE_CONTROL_CPREPTOKENIZER3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREPTOKENIZER3 */
            continue; /* skipped lines run on to the end of the file */
#endif
        case 0:
            pos = (char *)p - 1;
            return;
        case '"':
            if (!skipendoflinematch(p, '"'))
                return;
            p = (UInt8 *)pos;
            break;
        case '\'':
            if (!skipendoflinematch(p, '\''))
                return;
            p = (UInt8 *)pos;
            break;
        case '/':
            p = skip_continuations(start, p);
            if (*p == '/' && (!ANSIstrict || cplusplus)) {
                pos = (char *)CPrep_SkipNewComment(p + 1);
                if (CPrep_SkipNewCommentChar == '\r')
                    pos--;
                return;
            }
            if (*p == '*') {
                pos = (char *)p - 1;
                spaceskip = 1;
                p++;
                for (;;) {
                    if ((ch = *(p++)) == 0) {
                        if (tokenstacklevel > 0 || (char *)p >= prep_file_end) {
                            pos = (char *)p - 1;
                            CPrep_Error(0x67); /* unterminated comment */
                            return;
                        }
                        p[-1] = ' ';
                    }
                    if (ch == '*') {
                        p = skip_continuations(start, p);
                        if (*p == '/') {
                            p++;
                            break;
                        }
                    }
                    if (ch == '\r')
                        foundnl();
                }
            }
            break;
        case '\\':
            if (multibyteaware && COS_IsMultiByte(start, p - 1))
                continue;
            if (*p == '\r') {
                foundnl();
                p += p[1] == '\n' ? 2 : 1;
            }
            break;
        case '?':
            if (trigraphs && p[0] == '?' && p[1] == '/' && p[2] == '\r') {
                foundnl();
                p += 3;
            }
            break;
        }
    }
}
HOOK(0x479220, skipendofline);

/* lexidentifier: TK_IDENTIFIER -3, TK_ELLIPSIS 0x170 (ours). */
SInt16 lexidentifier(void) {
    SInt16 t = prepskipnextchar();
    char *save_pos;

    pos = nextcharpos;
    at_linestart = 0;
    if ((t >= 'a' && t <= 'z') || (t >= 'A' && t <= 'Z') || t == '_') {
        pos = ReadIdentifier(pos - 1);
        return -3; /* TK_IDENTIFIER */
    }
    if (t == '.') {
        save_pos = pos;
        if (prepnextchar() == '.' && prepnextchar() == '.')
            return 0x170; /* TK_ELLIPSIS */
        pos = save_pos;
    }
    return t;
}
HOOK(0x47be90, lexidentifier);

/* ---- part 4: token functions of the cprep_tokenize table and lookahead_noeol (not matched by
   gcmatch; the same logic as mweppc's, with our token numbers) ---- */

/* Tokens: TK_IDENTIFIER -3, TK_EOL -7, long 0x10a, goto 0x13e, *= 0x15b, += 0x15e, == 0x167,
   != 0x168, ++ 0x16d, :: 0x173. preprocessing_only 0x57e13b (the ":>" digraph is ']' only when
   compiling). HashNameNode.name +0xa. */
ORIG_FN(int, strcmp_, 0x404f80, (const char *a, const char *b));
#define preprocessing_only ORIG_VAR(Bool8, 0x57e13b)
#define cplusplus_tok ORIG_VAR(Bool8, 0x57db79)

/* The current character is c: take it and return yes, or leave it and return no. */
static SInt16 if_next(int c, SInt16 yes, SInt16 no) {
    if (prepcurchar() == c) {
        pos = nextcharpos;
        return yes;
    }
    return no;
}

SInt16 token(SInt16 t) {
    at_linestart = 0;
    return t;
}
HOOK(0x47b7f0, token);

SInt16 tdoll(SInt16 t) {
    at_linestart = 0;
    return t;
}
HOOK(0x47b800, tdoll);

SInt16 texcl(void) {
    at_linestart = 0;
    return if_next('=', 0x168, '!');
}
HOOK(0x479d00, texcl);

SInt16 tmult(void) {
    at_linestart = 0;
    return if_next('=', 0x15b, '*');
}
HOOK(0x479a80, tmult);

SInt16 tplus(void) {
    SInt16 c;

    at_linestart = 0;
    c = prepcurchar();
    if (c == '=') {
        pos = nextcharpos;
        return 0x15e;
    }
    if (c == '+') {
        pos = nextcharpos;
        return 0x16d;
    }
    return '+';
}
HOOK(0x479d30, tplus);

SInt16 tcolo(void) {
    char c;

    at_linestart = 0;
    if (cplusplus_tok) {
        c = (char)prepcurchar();
        if (c == ':') {
            pos = nextcharpos;
            return 0x173;
        }
        if (c == '>' && !preprocessing_only) {
            pos = nextcharpos;
            return ']';
        }
    }
    return ':';
}
HOOK(0x479ab0, tcolo);

SInt16 tequa(void) {
    at_linestart = 0;
#ifdef RECOMP_NEGATIVE_CONTROL_CPREPTOKENIZER4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREPTOKENIZER4 */
    return if_next('=', 0x168, '='); /* == read as != */
#endif
    return if_next('=', 0x167, '=');
}
HOOK(0x479bc0, tequa);

static SInt16 keyword(const char *word, SInt16 tk) {
    at_linestart = 0;
    pos = ReadIdentifier(pos - 1);
    if (macrocheck && macrotest())
        return 0;
    if (!strcmp_(word, (const char *)tkidentifier + 0xa))
        return tk;
    return -3; /* TK_IDENTIFIER */
}

SInt16 tchrg(void) {
    return keyword((const char *)0x5387f8 /* "goto" */, 0x13e);
}
HOOK(0x47a990, tchrg);

SInt16 tchrl(void) {
    return keyword((const char *)0x538810 /* "long" */, 0x10a);
}
HOOK(0x47aad0, tchrl);

SInt16 lookahead_noeol(void) {
    SInt32 state;
    SInt16 t;

    CPrep_TokenStreamGetState(&state);
    do
        t = lex();
    while (t == -7); /* TK_EOL */
    CPrep_TokenStreamSetState(&state);
    return t;
}
HOOK(0x47b8c0, lookahead_noeol);

/* Keyword functions with several words: each entry is a word (an exe string), its token and
   flags: KW_CPP (C++ only), KW_WARN (warning 0x153 in Embedded C++, copts.ecplusplus 0x57db7a). */
ORIG_FN(void, CPrep_Warning, 0x43c720, (SInt16 code));
#define ecplusplus ORIG_VAR(Bool8, 0x57db7a)
#define in_assembler ORIG_VAR(Bool8, 0x57e03e)
#define KW_CPP 1
#define KW_WARN 2
typedef struct { UInt32 word; SInt16 tk; UInt8 flags; } Keyword;

static SInt16 keywords(const Keyword *kw, int n) {
    int i;

    at_linestart = 0;
    pos = ReadIdentifier(pos - 1);
    if (macrocheck && macrotest())
        return 0;
    for (i = 0; i < n; i++) {
        if ((kw[i].flags & KW_CPP) && !cplusplus_tok)
            continue;
        if (!strcmp_((const char *)kw[i].word, (const char *)tkidentifier + 0xa)) {
            if ((kw[i].flags & KW_WARN) && ecplusplus)
                CPrep_Warning(0x153);
            return kw[i].tk;
        }
    }
    return -3; /* TK_IDENTIFIER */
}

SInt16 tchrv(void) {
    static const Keyword kw[] = {
        {0x538950 /* "void" */, 0x106, 0}, {0x538958 /* "volatile" */, 0x122, 0},
        {0x538964 /* "virtual" */, 0x14f, KW_CPP},
    };
    return keywords(kw, 3);
}
HOOK(0x47b300, tchrv);

SInt16 tchru(void) {
    static const Keyword kw[] = {
        {0x538934 /* "union" */, 0x110, 0}, {0x53893c /* "unsigned" */, 0x10e, 0},
        {0x538948 /* "using" */, 0x156, KW_CPP | KW_WARN},
    };
    return keywords(kw, 3);
}
HOOK(0x47b240, tchru);

SInt16 tchrm(void) {
    static const Keyword kw[] = {{0x538818 /* "mutable" */, 0x12a, KW_CPP | KW_WARN}};
    return keywords(kw, 1);
}
HOOK(0x47ab30, tchrm);

/* 'x' and 'X': the alternative tokens xor and xor_eq, in C++ outside asm and when compiling. */
SInt16 tchrx(SInt16 t) {
    static const Keyword kw[] = {{0x53897c /* "xor" */, '^', 0}, {0x538980 /* "xor_eq" */, 0x163, 0}};

    if (cplusplus_tok && t == 'x' && !in_assembler && !preprocessing_only)
        return keywords(kw, 2);
    return keywords(kw, 0);
}
HOOK(0x47b430, tchrx);

/* '-': -= 0x15f, -- 0x16e, -> 0x16f, ->* 0x172 (C++). */
SInt16 tminu(void) {
    SInt16 c;

    at_linestart = 0;
    c = prepcurchar();
    if (c == '=') {
        pos = nextcharpos;
        return 0x15f;
    }
    if (c == '-') {
        pos = nextcharpos;
        return 0x16e;
    }
    if (c == '>') {
        pos = nextcharpos;
        if (cplusplus_tok && prepcurchar() == '*') {
            pos = nextcharpos;
            return 0x172;
        }
        return 0x16f;
    }
    return '-';
}
HOOK(0x479d80, tminu);
