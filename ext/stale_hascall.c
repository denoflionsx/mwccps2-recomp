/*
 * stale_hascall.c — `#pragma stale_hascall`: the leftover `hascall` byte of expression nodes.
 *
 * Expression nodes are 40-byte blocks from lalloc (0x433320), a bump allocator whose memory is
 * reused from one function to the next and never cleared. Some of the code that builds a node
 * sets only part of it: the literal cases of the primary-expression parser (0x469360), for one,
 * leave bytes 4-9 as lalloc returned them. Byte 5 is ENode.hascall. The back end computes it for
 * most nodes, but not for a floating constant it loads inline, and call lowering (0x4a4960)
 * evaluates an argument whose hascall is nonzero before the others. So whether a constant
 * argument is loaded early depends on what the node's memory held before (docs/ENTROPY.md).
 *
 * The pragma says what that leftover byte is, where the source needs to:
 *
 *     #pragma stale_hascall on       every node allocated from here on finds the byte set
 *     #pragma stale_hascall off      ... finds it clear
 *     #pragma stale_hascall reset    ... finds what the memory held (the original's behaviour)
 *     #pragma stale_hascall(1, 0, _) the nodes of the next numeric literals (integer or floating,
 *                                    in source order) find it set, clear, or as it is; after the
 *                                    list, the on/off/reset state applies again
 *
 * Only the leftover is supplied: code that sets the byte itself still does, so nothing changes
 * for nodes whose hascall never was stale. The state starts again with each source file, from
 * MWRECOMP_HASCALL (on or off; unset means reset).
 *
 * MWRECOMP_HASCALL_TRACE=1 prints each numeric literal's node to stderr: file, line, the
 * literal's kind and bits, the byte as found and as left.
 *
 * MWRECOMP_EXT name: stale_hascall
 */
#include "ext.h"

#define ENODE_SIZE 0x28
#define ENODE_HASCALL 5
/* where the literal cases of the primary-expression parser come back from lalloc */
#define INT_LITERAL_SITE ORIG_SITE(0x469477)
#define FLOAT_LITERAL_SITE ORIG_SITE(0x4694b7)

ORIG_FN(void, goendofline, 0x43d5c0, (void));
ORIG_FN(void, CPrep_Error, 0x43c670, (int code));
ORIG_FN(void, skipendofline, 0x479220, (void));
#define pos ORIG_VAR(char *, 0x57aba0)         /* the preprocessor's place in the text */
#define linenumber ORIG_VAR(SInt32, 0x57c800)
#define filesp ORIG_VAR(SInt16, 0x57dc72)
#define filestack0 ORIG_VAR(UInt8 *, 0x54d2f4) /* filestack[0]; a CPrepFileInfo starts with its FSSpec (name +6) */
#define tkintconst ORIG_VAR(UInt32, 0x56c8b0)
#define tkfloatconst ((UInt32 *)0x56e268)

enum { AS_FOUND, CLEAR, SET };

static UInt8 mode;      /* for every node */
static UInt8 queue[64]; /* for the next numeric literals */
static int queued, taken;
static int tracing = -1;

void StaleHascall_NewCompile(void) {
    char text[8];

    mode = AS_FOUND;
    queued = taken = 0;
    if (Ext_On("stale_hascall") && Rt_GetEnv("MWRECOMP_HASCALL", text, sizeof text))
        mode = Rt_StrEq(text, "on") ? SET : Rt_StrEq(text, "off") ? CLEAR : AS_FOUND;
}

/* ---- the nodes ---- */

static UInt32 node_size;

int Lalloc_Before(UInt32 *args) {
    node_size = args[0];
    return 0;
}

static void trace(UInt32 site, UInt8 found, UInt8 left) {
    const UInt8 *name = (&filestack0)[filesp] + 6;
    char text[64];
    int i;

    for (i = 0; i < name[0] && i < 63; i++)
        text[i] = (char)name[1 + i];
    text[i] = 0;
    Rt_Say("mwrecomp: literal ");
    Rt_Say(text);
    Rt_Say(":");
    Rt_SayDec((UInt32)linenumber);
    if (site == INT_LITERAL_SITE) {
        Rt_Say(" int ");
        Rt_SayHex(tkintconst);
    }
    else {
        Rt_Say(" float ");
        Rt_SayHex(tkfloatconst[1]);
        Rt_SayHex(tkfloatconst[0]);
    }
    Rt_Say(found ? " stale 1" : " stale 0");
    Rt_Say(left ? " hascall 1\n" : " hascall 0\n");
}

extern UInt32 Lalloc_ret; /* who called lalloc (the wrap keeps it) */

UInt32 Lalloc_After(UInt32 *args, UInt32 result) {
    UInt8 *node = (UInt8 *)result;
    UInt32 site = Lalloc_ret;
    int literal = site == INT_LITERAL_SITE || site == FLOAT_LITERAL_SITE;
    UInt8 found, want = mode;

    AllocTrace_Lalloc(node_size, result, site);
    if (node_size != ENODE_SIZE)
        return result;
    found = node[ENODE_HASCALL];
    if (literal && taken < queued)
        want = queue[taken++];
    if (want != AS_FOUND)
        node[ENODE_HASCALL] = want == SET;
    if (literal) {
        if (tracing < 0) {
            char text[4];
            tracing = Rt_GetEnv("MWRECOMP_HASCALL_TRACE", text, sizeof text) != 0;
        }
        if (tracing)
            trace(site, found, node[ENODE_HASCALL]);
    }
    return result;
}
HOOK_WRAP(Lalloc, 0x433320, 5, -1, Lalloc_Before, Lalloc_After);

/* ---- the pragma ---- */

static int word(const char **text, const char *w) {
    const char *p = *text;

    while (*w && *p == *w)
        p++, w++;
    if (*w || (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')
        return 0;
    *text = p;
    return 1;
}

static const char *blanks(const char *p) {
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

/* before the #pragma handler (0x43fdb0): the text after "pragma" has not been read yet */
int Pragma_Before(UInt32 *args) {
    const char *p = blanks(pos);
    int n = 0;

    if (!Ext_On("stale_hascall") || !word(&p, "stale_hascall"))
        return 0; /* the original handles it */
    p = blanks(p);
    if (*p == '(') {
        for (p = blanks(p + 1); *p != ')'; p = blanks(p)) {
            if (n == (int)sizeof queue || (*p != '0' && *p != '1' && *p != '_'))
                goto bad;
            queue[n++] = *p == '1' ? SET : *p == '0' ? CLEAR : AS_FOUND;
            p = blanks(p + 1);
            if (*p == ',')
                p++;
            else if (*p != ')')
                goto bad;
        }
        p++;
        queued = n;
        taken = 0;
    }
    else {
        if (word(&p, "on"))
            mode = SET;
        else if (word(&p, "off"))
            mode = CLEAR;
        else if (word(&p, "reset"))
            mode = AS_FOUND;
        else
            goto bad;
        queued = taken = 0;
    }
    pos = (char *)p;
    goendofline(); /* like the original's handlers: nothing else may follow */
    return 1;
bad:
    CPrep_Error(0x75); /* preprocessor syntax error */
    skipendofline();
    return 1;
}

UInt32 Pragma_After(UInt32 *args, UInt32 result) {
    return result;
}
HOOK_WRAP(Pragma, 0x43fdb0, 6, 2, Pragma_Before, Pragma_After);
