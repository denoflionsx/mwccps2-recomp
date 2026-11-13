/*
 * line_path.c — `#line N "path"`: the path names the file in debug records.
 *
 * In the original, #line with a file name changes what messages call the file and nothing else.
 * Its handler (0x440420) first pins the file's name node to the real path (CPrepFileInfo +0x46;
 * CPrep_SetSourceFile hands it to the debug writer for a function defined in an included file)
 * and then renames the file's FSSpec with 0x4419f0. The main file has no name node: debug setup
 * (0x503010) copies its path into a buffer once, and the writer uses the buffer whenever the
 * current name is null.
 *
 * With this extension the given path is the file's name for debug records instead: the name node
 * of an included file, and the current name while the main file is being compiled. The records of
 * functions compiled after the directive carry the path exactly as written. `\\` in the path
 * stands for one backslash, as in a string literal (and as other compilers read #line); the
 * original takes the characters as they are.
 *
 * The compiler's output depends on memory it never wrote (docs/ENTROPY.md), so the directive must
 * not leave traces the original sources would not have left:
 *
 *   - The file is not renamed for messages, which keep naming the file that is really there.
 *     The renaming goes through the driver's path tables and depends on which directories exist.
 *   - The main file gets no name node: the original's handler would make one of the real path,
 *     and that allocation alone moves code. Its name is kept here instead.
 *   - The stack below the directive dispatcher is put back when a #line directive is done (the
 *     handler has a 256-byte buffer for the path).
 *   - A directive on the first line of an included file names it from the moment it is opened:
 *     opening a file is where the name node of its real path is made (0x43b582), and the length
 *     of that path decides where everything allocated later lands.
 *
 * So with
 *
 *     #line 2 "D:\\work\\game\\source\\main.cpp"
 *
 * as the first line of every file, the real paths never enter the compiler's tables: the debug
 * records and the code are the same wherever the files are, and the code is the same as without
 * the directives. (2, so that the lines keep their numbers: the line after the directive is
 * line 2.)
 *
 * MWRECOMP_EXT name: line_path
 */
#include "ext.h"

ORIG_FN(void *, GetHashNameNodeExport, 0x432ef0, (const char *name));
ORIG_FN(SInt32, COS_FileGetPathName, 0x441b30, (char *buffer, const void *spec, SInt32 *moddate));
#define filestack0 ORIG_VAR(UInt8 *, 0x54d2f4)       /* filestack[0]: the main file */
#define cprep_cursymfile ORIG_VAR(UInt8 *, 0x57b6d8) /* the included file the debug writer was last told about */
#define objgen_srcname ORIG_VAR(void *, 0x57b714)    /* the current name for debug records; null = the main file's */
#define copts_fullpath ORIG_VAR(UInt8, 0x57dbfd)     /* debug records hold whole paths, not bare file names */
#define DWARF_MAIN_NAME ((char *)0x567f80)           /* the main file's name for debug records */
#define DWARF_MAIN_NAME_SIZE 0x400
#define pos ORIG_VAR(const char *, 0x57aba0)         /* the preprocessor's place in the text */
#define FILE_NAMENODE(file) (*(void **)((UInt8 *)(file) + 0x46))
/* where GetHashNameNodeForPath (0x433790) returns to: the file-opening code (0x43b450), which makes
   the name node of an included file's real path, and CPrep_GetSourceFilePath (0x43c940), which the
   #line handler calls and which makes one for a file that has none */
#define OPEN_NAME_SITE ORIG_SITE(0x43b587)
#define PIN_NAME_SITE ORIG_SITE(0x43c953)

/* A name the debug writer can take for the main file: laid out as the compiler's HashNameNode,
   of which the writer reads the name. Kept here, not in the compiler's heap. */
typedef struct {
    void *next;
    SInt32 id;
    SInt16 hashval;
    char name[256];
} NameNode;

static NameNode main_names[8];
static int n_main_names;
static NameNode *main_name; /* the main file's current name from a directive, if it had one */

void LinePath_NewCompile(void) {
    n_main_names = 0;
    main_name = 0;
}

/* a directive's path: the characters between the quotes, `\\` read as one backslash */
static const char *path_text(const UInt8 *text, int n) {
    static char path[256];
    int i, k = 0;

    for (i = 0; i < n && k < 255; i++) {
        if (text[i] == '\\' && i + 1 < n && text[i + 1] == '\\')
            i++;
        path[k++] = (char)text[i];
    }
    path[k] = 0;
    return path;
}

static NameNode *main_node(const char *path) {
    NameNode *node;
    int i;

    for (i = 0; i < n_main_names; i++)
        if (Rt_StrEq(main_names[i].name, path))
            return &main_names[i];
    if (n_main_names < (int)(sizeof main_names / sizeof main_names[0]))
        n_main_names++; /* past that many names in one file, the last one is reused */
    node = &main_names[n_main_names - 1];
    for (i = 0; path[i]; i++)
        node->name[i] = path[i];
    node->name[i] = 0;
    return node;
}

/* ---- the directive: instead of 0x4419f0(file, name), which renames the file for messages ---- */

int LinePath_Before(UInt32 *args) {
    UInt8 *file = (UInt8 *)args[0];             /* the CPrepFileInfo */
    const UInt8 *name = (const UInt8 *)args[1]; /* a Pascal string */
    const char *path, *tail;
    int i;

    if (!Ext_On("line_path"))
        return 0;
    path = path_text(name + 1, name[0]);
    if (file != filestack0) {
        FILE_NAMENODE(file) = GetHashNameNodeExport(path);
        if (cprep_cursymfile == file)
            objgen_srcname = FILE_NAMENODE(file); /* CPrep_SetSourceFile would not pass it on: same file */
        return 1;
    }
    main_name = main_node(path);
    if (!cprep_cursymfile)
        objgen_srcname = main_name;
    /* the file's own record is written at the end, from the buffer */
    tail = path;
    if (!copts_fullpath) /* as 0x501b40 does for an included file's name */
        for (i = 0; path[i]; i++)
            if (path[i] == '\\' || path[i] == '/')
                tail = path + i + 1;
    for (i = 0; tail[i] && i < DWARF_MAIN_NAME_SIZE - 1; i++)
        DWARF_MAIN_NAME[i] = tail[i];
    DWARF_MAIN_NAME[i] = 0;
    return 1;
}

UInt32 LinePath_After(UInt32 *args, UInt32 result) {
    return result;
}
HOOK_WRAP(LinePath, 0x4419f0, 8, -1, LinePath_Before, LinePath_After);

/* ---- the current name: after ObjGen_SrcBreakName(name), where null means the main file ---- */

int LinePathBreak_Before(UInt32 *args) {
    return 0;
}

UInt32 LinePathBreak_After(UInt32 *args, UInt32 result) {
    if (!objgen_srcname && main_name)
        objgen_srcname = main_name;
    return result;
}
HOOK_WRAP(LinePathBreak, 0x48af20, 9, -1, LinePathBreak_Before, LinePathBreak_After);

/* ---- name nodes of real paths: before GetHashNameNodeForPath(spec, moddate) ---- */

extern UInt32 LinePathNode_res;

/* The path of a directive that is the first thing in text, or null. */
static const char *first_line_path(const UInt8 *text, const UInt8 *end) {
    const UInt8 *p = text, *path;

    if (end - p < 5 || p[0] != '#' || p[1] != 'l' || p[2] != 'i' || p[3] != 'n' || p[4] != 'e')
        return 0;
    for (p += 5; p < end && (*p == ' ' || *p == '\t'); p++)
        ;
    if (p == end || *p < '0' || *p > '9')
        return 0;
    for (; p < end && *p >= '0' && *p <= '9'; p++)
        ;
    for (; p < end && (*p == ' ' || *p == '\t'); p++)
        ;
    if (p == end || *p != '"')
        return 0;
    for (path = ++p; p < end && *p != '"' && *p != '\n' && *p != '\r'; p++)
        ;
    if (p == end || *p != '"')
        return 0;
    return path_text(path, (int)(p - path));
}

int LinePathNode_Before(UInt32 *args) {
    static char real[256];
    const UInt8 *spec = (const UInt8 *)args[0];
    UInt32 caller = args[-1];
    const UInt8 *text;
    const char *path;

    if (!Ext_On("line_path"))
        return 0;
    if (caller == PIN_NAME_SITE) {
        LinePathNode_res = 0; /* the #line handler, for a file without a name node: it stays without */
        return 1;
    }
    if (caller != OPEN_NAME_SITE)
        return 0;
    /* the file's CWFileInfo is around spec: the loaded text 12 bytes before it, its length 8 before */
    text = *(const UInt8 **)(spec - 12);
    if (!text || !(path = first_line_path(text, text + *(SInt32 *)(spec - 8))))
        return 0;
    COS_FileGetPathName(real, spec, (SInt32 *)args[1]); /* the file's date, as the original call gets it */
    LinePathNode_res = (UInt32)GetHashNameNodeExport(path);
    return 1;
}

UInt32 LinePathNode_After(UInt32 *args, UInt32 result) {
    return result;
}
HOOK_WRAP(LinePathNode, 0x433790, 6, -1, LinePathNode_Before, LinePathNode_After);

/* ---- the stack: around the directive dispatcher (0x4413b0) ---- */

#define STACK_KEPT 0x2000

static UInt8 kept[STACK_KEPT];
static UInt8 *kept_from;

int LineStack_Before(UInt32 *args) {
    const char *p = pos; /* just after the '#' */
    volatile UInt8 *top = (volatile UInt8 *)args - 4; /* the dispatcher's return address */
    UInt32 i;

    kept_from = 0;
    if (!Ext_On("line_path"))
        return 0;
    while (*p == ' ' || *p == '\t')
        p++;
    if (p[0] != 'l' || p[1] != 'i' || p[2] != 'n' || p[3] != 'e' || (p[4] != ' ' && p[4] != '\t'))
        return 0;
    for (i = 0x1000; i <= STACK_KEPT; i += 0x1000) /* top down, so the stack grows a page at a time */
        (void)top[-(SInt32)i];
    kept_from = (UInt8 *)top - STACK_KEPT;
    memcpy(kept, kept_from, STACK_KEPT);
    return 0;
}

UInt32 LineStack_After(UInt32 *args, UInt32 result) {
    if (kept_from)
        memcpy(kept_from, kept, STACK_KEPT);
    return result;
}
HOOK_WRAP(LineStack, 0x4413b0, 6, -1, LineStack_Before, LineStack_After);
