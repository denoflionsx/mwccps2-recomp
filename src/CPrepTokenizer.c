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

/* ---- part 5: the other keyword functions of the cprep_tokenize table (not matched by gcmatch;
   mweppc's keywords with our token numbers and option checks) ---- */

/* Conditions of a keyword (all must hold): KC_CPP C++; KC_WARN warning 0x153 in Embedded C++;
   KC_ALT alternative token (C++, not in asm, not preprocessing only); KC_NONSTD C++ or not
   only-standard-keywords (copts 0x57db7f); KC_NOSTRICT not only-standard-keywords; KC_C99 (copts
   0x57dbad); KC_INLINE C++, C99 or not only-standard-keywords; KC_BOOL bool keywords (copts
   0x57db92); KC_NOEC not Embedded C++; KC_LOWER the token's first character is lower case (the
   function also serves the capital letter); KC_GCC gcc extensions (0x57dbb4); KC_WCHAR wchar_t
   keyword (0x57db9c). Words are the exe's strings. */
#define onlystdkeywords ORIG_VAR(Bool8, 0x57db7f)
#define c99_ ORIG_VAR(Bool8, 0x57dbad)
#define booltruefalse_ ORIG_VAR(Bool8, 0x57db92)
#define gcc_extensions_ ORIG_VAR(Bool8, 0x57dbb4)
#define wchar_type_ ORIG_VAR(Bool8, 0x57db9c)
enum {
    KC_CPP = 1, KC_WARN = 2, KC_ALT = 4, KC_NONSTD = 8, KC_C99 = 0x10, KC_INLINE = 0x20, KC_BOOL = 0x40,
    KC_NOEC = 0x80, KC_LOWER = 0x100, KC_GCC = 0x200, KC_WCHAR = 0x400, KC_NOSTRICT = 0x800,
};
typedef struct { UInt32 word; SInt16 tk; UInt16 cond; } Keyword2;

static Bool8 keyword_allowed(UInt16 c, SInt16 t, const char *word) {
    if ((c & (KC_CPP | KC_ALT)) && !cplusplus_tok)
        return 0;
    if ((c & KC_ALT) && (in_assembler || preprocessing_only))
        return 0;
    if ((c & KC_NONSTD) && !cplusplus_tok && onlystdkeywords)
        return 0;
    if ((c & KC_NOSTRICT) && onlystdkeywords)
        return 0;
    if ((c & KC_C99) && !c99_)
        return 0;
    if ((c & KC_INLINE) && !cplusplus_tok && !c99_ && onlystdkeywords)
        return 0;
    if ((c & KC_BOOL) && !booltruefalse_)
        return 0;
    if ((c & KC_NOEC) && ecplusplus)
        return 0;
    if ((c & KC_LOWER) && t != word[0])
        return 0;
    if ((c & KC_GCC) && !gcc_extensions_)
        return 0;
    if ((c & KC_WCHAR) && !wchar_type_)
        return 0;
    return 1;
}

static SInt16 keywords2(SInt16 t, const Keyword2 *kw, int n) {
    const char *name;
    int i;

    at_linestart = 0;
    pos = ReadIdentifier(pos - 1);
    if (macrocheck && macrotest())
        return 0;
    name = (const char *)tkidentifier + 0xa;
    for (i = 0; i < n; i++) {
        if (!keyword_allowed(kw[i].cond, t, (const char *)kw[i].word))
            continue;
        if (!strcmp_((const char *)kw[i].word, name)) {
            if ((kw[i].cond & KC_WARN) && ecplusplus)
                CPrep_Warning(0x153);
            return kw[i].tk;
        }
    }
    return -3; /* TK_IDENTIFIER */
}

#define KW_FUNC(fn, ...)                                                                           \
    SInt16 fn(SInt16 t) {                                                                          \
        static const Keyword2 kw[] = {__VA_ARGS__};                                                \
        return keywords2(t, kw, sizeof(kw) / sizeof(kw[0]));                                       \
    }

KW_FUNC(tchra, {0x538704 /* auto */, 0x100, 0}, {0x53870c /* asm */, 0x135, KC_NONSTD},
        {0x538710 /* and */, 0x166, KC_ALT}, {0x538714 /* and_eq */, 0x162, KC_ALT})
HOOK(0x47a380, tchra);
KW_FUNC(tchrb, {0x53871c /* break */, 0x140, 0}, {0x538724 /* bool */, 0x11c, KC_CPP | KC_BOOL},
        {0x53872c /* bitand */, '&', KC_ALT}, {0x538734 /* bitor */, '|', KC_ALT})
HOOK(0x47a470, tchrb);
KW_FUNC(tchrc, {0x53873c /* case */, 0x136, 0}, {0x538744 /* char */, 0x107, 0},
        {0x53874c /* const */, 0x121, 0}, {0x538754 /* continue */, 0x13f, 0},
        {0x538760 /* const_cast */, 0x151, KC_CPP | KC_WARN}, {0x53876c /* catch */, 0x143, KC_CPP | KC_WARN},
        {0x538774 /* class */, 0x112, KC_CPP}, {0x53877c /* compl */, '~', KC_ALT})
HOOK(0x47a550, tchrc);
KW_FUNC(tchrd, {0x538784 /* default */, 0x137, 0}, {0x53878c /* do */, 0x13c, 0},
        {0x538790 /* double */, 0x10c, 0}, {0x538798 /* delete */, 0x144, KC_CPP},
        {0x5387a0 /* dynamic_cast */, 0x152, KC_CPP | KC_WARN})
HOOK(0x47a6d0, tchrd);
KW_FUNC(tchre, {0x5387b0 /* else */, 0x139, 0}, {0x5387b8 /* enum */, 0x111, 0},
        {0x5387c0 /* extern */, 0x103, 0}, {0x5387c8 /* explicit */, 0x129, KC_CPP},
        {0x5387d4 /* export */, 0x15a, KC_CPP | KC_NOEC})
HOOK(0x47a7d0, tchre);
KW_FUNC(tchrf, {0x5387dc /* float */, 0x10b, 0}, {0x5387e4 /* for */, 0x13d, 0},
        {0x5387e8 /* friend */, 0x145, KC_CPP}, {0x5387f0 /* false */, 0x158, KC_CPP | KC_BOOL})
HOOK(0x47a8c0, tchrf);
KW_FUNC(tchri, {0x538800 /* if */, 0x138, 0}, {0x538804 /* int */, 0x109, 0},
        {0x538808 /* inline */, 0x105, KC_INLINE})
HOOK(0x47a9f0, tchri);
KW_FUNC(tchrn, {0x538820 /* new */, 0x146, KC_CPP | KC_LOWER},
        {0x538824 /* namespace */, 0x153, KC_CPP | KC_LOWER | KC_WARN},
        {0x538830 /* not */, '!', KC_ALT | KC_LOWER}, {0x538834 /* not_eq */, 0x168, KC_ALT | KC_LOWER})
HOOK(0x47abb0, tchrn);
KW_FUNC(tchro, {0x53883c /* operator */, 0x147, KC_CPP}, {0x538848 /* or */, 0x165, KC_ALT},
        {0x53884c /* or_eq */, 0x164, KC_ALT})
HOOK(0x47acb0, tchro);
KW_FUNC(tchrp, {0x538854 /* pascal */, 0x123, KC_NOSTRICT},
        {0x53885c /* private */, 0x148, KC_CPP | KC_LOWER}, {0x538864 /* protected */, 0x149, KC_CPP | KC_LOWER},
        {0x538870 /* public */, 0x14a, KC_CPP | KC_LOWER})
HOOK(0x47ad70, tchrp);
KW_FUNC(tchrr, {0x538878 /* register */, 0x101, 0}, {0x538884 /* return */, 0x141, 0},
        {0x53888c /* reinterpret_cast */, 0x154, KC_CPP | KC_WARN}, {0x5388a0 /* restrict */, 0x184, KC_C99})
HOOK(0x47ae40, tchrr);
KW_FUNC(tchrs, {0x5388ac /* short */, 0x108, 0}, {0x5388b4 /* signed */, 0x10d, 0},
        {0x5388bc /* sizeof */, 0x142, 0}, {0x5388c4 /* static */, 0x102, 0}, {0x5388cc /* struct */, 0x10f, 0},
        {0x5388d4 /* switch */, 0x13a, 0}, {0x5388dc /* static_cast */, 0x155, KC_CPP | KC_WARN})
HOOK(0x47af20, tchrs);
KW_FUNC(tchrt, {0x5388e8 /* typedef */, 0x104, 0}, {0x5388f0 /* typeof */, 0x11b, KC_GCC},
        {0x5388f8 /* this */, 0x14c, KC_CPP}, {0x538900 /* true */, 0x157, KC_CPP | KC_BOOL},
        {0x538908 /* template */, 0x14b, KC_CPP | KC_WARN}, {0x538914 /* try */, 0x14e, KC_CPP | KC_WARN},
        {0x538918 /* throw */, 0x14d, KC_CPP | KC_WARN}, {0x538920 /* typeid */, 0x159, KC_CPP | KC_WARN},
        {0x538928 /* typename */, 0x120, KC_CPP | KC_WARN})
HOOK(0x47b060, tchrt);
#ifdef RECOMP_NEGATIVE_CONTROL_CPREPTOKENIZER5 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREPTOKENIZER5 */
#define TK_WHILE 0x13d /* while read as for */
#else
#define TK_WHILE 0x13b
#endif
KW_FUNC(tchrw, {0x53896c /* while */, TK_WHILE, 0}, {0x538974 /* wchar_t */, 0x11d, KC_CPP | KC_WCHAR})
HOOK(0x47b3a0, tchrw);

/* '_': calling conventions, GNU spellings, C99 types (with C99 on) and the PS2's __fixed__8x8 and
   __int128. */
KW_FUNC(tchr_, {0x538988 /* __stdcall */, 0x125, 0}, {0x538994 /* __cdecl */, 0x126, 0},
        {0x53899c /* __fastcall */, 0x127, 0}, {0x5389a8 /* __declspec */, 0x124, 0},
        {0x5389b4 /* __asm */, 0x135, 0}, {0x5389bc /* __asm__ */, 0x135, 0},
        {0x5389c4 /* __const */, 0x121, 0}, {0x5389cc /* __const__ */, 0x121, 0},
        {0x5389d8 /* __volatile */, 0x122, 0}, {0x5389e4 /* __volatile__ */, 0x122, 0},
        {0x5389f4 /* __inline */, 0x105, 0}, {0x538a00 /* __inline__ */, 0x105, 0},
        {0x538a0c /* __restrict */, 0x184, 0}, {0x538a18 /* __restrict__ */, 0x184, 0},
        {0x538a28 /* __attribute__ */, 0x185, 0}, {0x538a38 /* __far */, 0x128, 0},
        {0x538a40 /* __alignof__ */, 0x183, 0}, {0x538a4c /* __typeof__ */, 0x11b, 0},
        {0x538a58 /* _Bool */, 0x11c, KC_C99}, {0x538a60 /* _Complex */, 0x11e, KC_C99},
        {0x538a6c /* _Imaginary */, 0x11f, KC_C99}, {0x538a78 /* __fixed__8x8 */, 0x113, 0},
        {0x538a88 /* __int128 */, 0x114, 0})
HOOK(0x47b4d0, tchr_);

/* ---- part 6: operator token functions (not matched by gcmatch; the same logic as mweppc's) ---- */

/* Tokens: %= 0x15d, <<= 0x160, >>= 0x161, &= 0x162, ^= 0x163, |= 0x164, || 0x165, && 0x166,
   <= 0x169, >= 0x16a, << 0x16b, >> 0x16c. Digraphs (<% %> <: :>) need C++ and are not taken when
   only preprocessing. In asm, '@' starts an identifier (cprep_idarray['@'] at 0x57aa78 is set
   while reading it). */
static Bool8 digraphs_on(void) {
    return cplusplus_tok && !preprocessing_only;
}

SInt16 torrr(void) {
    SInt16 c;
    at_linestart = 0;
    c = prepcurchar();
    if (c == '=') {
        pos = nextcharpos;
        return 0x164;
    }
    if (c == '|') {
        pos = nextcharpos;
        return 0x165;
    }
    return '|';
}
HOOK(0x479e10, torrr);

SInt16 tampe(void) {
    SInt16 c;
    at_linestart = 0;
    c = prepcurchar();
    if (c == '=') {
        pos = nextcharpos;
        return 0x162;
    }
    if (c == '&') {
        pos = nextcharpos;
#ifdef RECOMP_NEGATIVE_CONTROL_CPREPTOKENIZER6 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREPTOKENIZER6 */
        return 0x165; /* && read as || */
#endif
        return 0x166;
    }
    return '&';
}
HOOK(0x479e60, tampe);

SInt16 tpowe(void) {
    at_linestart = 0;
    return if_next('=', 0x163, '^');
}
HOOK(0x479eb0, tpowe);

SInt16 tperc(void) {
    char c;
    at_linestart = 0;
    c = (char)prepcurchar();
    if (c == '=') {
        pos = nextcharpos;
        return 0x15d;
    }
    if (c == '>' && digraphs_on()) {
        pos = nextcharpos;
        return '}';
    }
    return '%';
}
HOOK(0x479cb0, tperc);

SInt16 tless(void) {
    SInt16 c;
    at_linestart = 0;
    c = prepcurchar();
    if (c == '=') {
        pos = nextcharpos;
        return 0x169;
    }
    if (c == '<') {
        pos = nextcharpos;
        return if_next('=', 0x160, 0x16b);
    }
    if (c == '%' && digraphs_on()) {
        pos = nextcharpos;
        return '{';
    }
    if (c == ':' && digraphs_on()) {
        pos = nextcharpos;
        return '[';
    }
    return '<';
}
HOOK(0x479b10, tless);

SInt16 tgrea(void) {
    SInt16 c;
    at_linestart = 0;
    c = prepcurchar();
    if (c == '=') {
        pos = nextcharpos;
        return 0x16a;
    }
    if (c == '>') {
        pos = nextcharpos;
        return if_next('=', 0x161, 0x16c);
    }
    return '>';
}
HOOK(0x479bf0, tgrea);

SInt16 tatsg(void) {
    char *save = pos;
    UInt8 save_id;

    at_linestart = 0;
    if (in_assembler) {
        save_id = *(UInt8 *)0x57aa78;
        *(UInt8 *)0x57aa78 = 1;
        pos = ReadIdentifier(pos - 1);
        *(UInt8 *)0x57aa78 = save_id;
        if (in_assembler)
            return -3; /* TK_IDENTIFIER */
        pos = save;
    }
    return '@';
}
HOOK(0x479c50, tatsg);

/* tpoin ('.') and tchrL ('L'). tkfloatconst 0x56e268, tksize 0x57c9ec (IT_USHORT 6, IT_WCHAR_T 4),
   widestring 0x57e143. TK_FLOATCONST -2, TK_ELLIPSIS 0x170, TK_DOT_STAR 0x171. */
ORIG_FN(char *, CMach_FloatScan, 0x4588f0, (char *p, void *result, Bool8 *failed));
ORIG_FN(SInt16, floatsuffix, 0x478c60, (SInt16 c));
ORIG_FN(SInt16, tapos, 0x4794d0, (SInt16 t));
ORIG_FN(SInt16, tquot, 0x4797c0, (SInt16 t));
#define tksize ORIG_VAR(SInt32, 0x57c9ec)
#define widestring ORIG_VAR(Bool8, 0x57e143)

SInt16 tpoin(void) {
    char *p;
    SInt16 t;
    Bool8 failed;

    at_linestart = 0;
    p = pos - 1;
    t = prepnextchar();
    if (t >= '0' && t <= '9') {
        pos = CMach_FloatScan(p, (void *)0x56e268, &failed);
        if (failed)
            CPrep_Error(0x9a); /* number out of range */
        tksize = floatsuffix(prepcurchar());
        return -2; /* TK_FLOATCONST */
    }
    if (cplusplus_tok && t == '*')
        return 0x171;
    if (t == '.' && prepnextchar() == '.')
        return 0x170;
    pos = p + 1;
    return '.';
}
HOOK(0x47a0e0, tpoin);

SInt16 tchrL(void) {
    SInt16 t;

    at_linestart = 0;
    if ((t = prepcurchar()) == '\'') {
        widestring = 1;
        pos = nextcharpos;
        t = tapos(t);
        widestring = 0;
        tksize = (wchar_type_ && cplusplus_tok) ? 4 : 6;
        return t;
    }
    if (t == '"') {
        widestring = 1;
        pos = nextcharpos;
        t = tquot(t);
        widestring = 0;
        return t;
    }
    pos = ReadIdentifier(pos - 1);
    if (macrocheck && macrotest())
        return 0;
    return -3; /* TK_IDENTIFIER */
}
HOOK(0x47a2c0, tchrL);

/* tnumb ('1'..'9') and tzero ('0'). tkintconst 0x56c8b0 (128-bit), cint64_zero 0x56c8a0.
   InlineAsm_labelref 0x57e153. TK_INTCONST -1. Ours adds two checks mweppc lacks: a "0x" with no
   digits is an error only when compiling outside gcc-style asm, a "0b" with none only outside asm. */
ORIG_FN(char *, CInt64_ScanDecString, 0x4be400, (void *result, char *p, Bool8 *failed));
ORIG_FN(char *, CInt64_ScanOctString, 0x4be330, (void *result, char *p, Bool8 *failed));
ORIG_FN(char *, CInt64_ScanHexString, 0x4be5e0, (void *result, char *p, Bool8 *failed));
ORIG_FN(char *, CInt64_ScanBinString, 0x4be6d0, (void *result, char *p, Bool8 *failed));
ORIG_FN(SInt16, intsuffix, 0x478860, (SInt16 c, Bool8 is_octal_or_hex));
ORIG_FN(void, CError_Error_, 0x456d50, (int code, ...));
#define tkintconst ((UInt32 *)0x56c8b0)
#define cint64_zero ((const UInt32 *)0x56c8a0)
#define InlineAsm_labelref ORIG_VAR(Bool8, 0x57e153)
#define ANSIstrict_tok ORIG_VAR(Bool8, 0x57db81)

static SInt16 float_token(char *p) {
    Bool8 failed;

    pos = CMach_FloatScan(p, (void *)0x56e268, &failed);
    if (failed)
        CPrep_Error(0x9a); /* number out of range */
    tksize = floatsuffix(prepcurchar());
    return -2; /* TK_FLOATCONST */
}

static SInt16 int_token(Bool8 failed, Bool8 is_octal_or_hex) {
    int i;

    if (failed) {
        CPrep_Error(0x9a);
        for (i = 0; i < 4; i++)
            tkintconst[i] = cint64_zero[i];
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CPREPTOKENIZER6B /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CPREPTOKENIZER6B */
    tkintconst[3] ^= 1;
#endif
    tksize = intsuffix(prepcurchar(), is_octal_or_hex);
    return -1; /* TK_INTCONST */
}

SInt16 tnumb(SInt16 t) {
    char *p;
    SInt16 ch;
    Bool8 failed;

    at_linestart = 0;
    p = pos - 1;
    pos = CInt64_ScanDecString(tkintconst, p, &failed);
    ch = prepcurchar();
    if (ch == '.' || ch == 'e' || ch == 'E')
        return float_token(p);
    return int_token(failed, 0);
}
HOOK(0x47a1b0, tnumb);

SInt16 tzero(SInt16 t) {
    char *p, *q;
    Bool8 failed;

    if (InlineAsm_gccmode && InlineAsm_labelref)
        return tnumb(t);
    at_linestart = 0;
    p = pos - 1;
    t = prepnextchar();
    if (t == 'x' || t == 'X') {
        q = CInt64_ScanHexString(tkintconst, pos, &failed);
        if (!preprocessing_only && !InlineAsm_gccmode && pos == q)
            CError_Error_(0x69); /* illegal token */
        pos = q;
    } else if (!ANSIstrict_tok && (t == 'b' || t == 'B')) {
        q = CInt64_ScanBinString(tkintconst, pos, &failed);
        if (pos == q && !in_assembler)
            CError_Error_(0x69);
        pos = q;
    } else {
        while (t >= '0' && t <= '9')
            t = prepnextchar();
        if (t == '.' || t == 'E' || t == 'e')
            return float_token(p);
        pos = CInt64_ScanOctString(tkintconst, p, &failed);
    }
    return int_token(failed, 1);
}
HOOK(0x479f30, tzero);
