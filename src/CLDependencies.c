/*
 * CLDependencies.c — include-file lookup and the dependency lists behind -M/-MD, partial port.
 *
 * Port of mweppc's src/driver/CLDependencies.c (CC0, github.com/doldecomp/mweppc); its 20
 * functions in our exe are byte-identical between their GameCube 2.0 compiler and ours. Ported:
 * the 18 the corpus runs, statics included (hooked under CLDependencies_* names). IsSysIncl and
 * SetSpecialAccessPathFromIncludeStackTOS stay the original's.
 *
 * Layout from our code: Incls = targ +0, numincls +4, maxincls +8, files +0xc, buflen +0x10,
 * bufpos +0x14, buffer +0x18, allPaths +0x1c. InclFile (20 bytes) = filenameoffs +0,
 * accesspath +4, globalpath +8, specialpath +0xc, syspath (byte) +0x10. Deps = numDeps +0,
 * maxDeps +4, list +8, incls +0xc. Target: sysPaths +0x5c, userPaths +0x64; gTarg->incls +0x3c.
 * File: srcfilename +0x1c, outfss +0x422, deps.numDeps +0x668, deps.list +0x670. Path: spec +0,
 * recursive +4. optsCompiler.includeSearch (short) 0x57d740: Proj 0, Source 1, Explicit 2,
 * Include 3.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef struct Path {
    void *spec;
    struct Paths *recursive;
} Path;
typedef struct Paths Paths;
typedef struct InclFile {
    SInt32 filenameoffs;
    Path *accesspath;
    Path *globalpath;
    Path *specialpath;
    Bool8 syspath;
} InclFile;
_Static_assert(sizeof(InclFile) == 20, "InclFile layout");
typedef struct Incls {
    UInt8 *targ;
    int numincls;
    int maxincls;
    InclFile *files;
    SInt32 buflen;
    SInt32 bufpos;
    char *buffer;
    Paths *allPaths;
} Incls;
typedef struct Deps {
    int numDeps;
    int maxDeps;
    SInt32 *list;
    Incls *incls;
} Deps;
typedef struct OSSpec {
    UInt8 bytes[0x204]; /* path (0x104) + name (0x100) */
} OSSpec;
#define OSSPEC_PATH(s) ((void *)(s))

ORIG_FN_STDCALL(void *, xmalloc, 0x4176a0, (const char *what, int size));
ORIG_FN_STDCALL(void *, xrealloc, 0x417750, (const char *what, void *old, int size));
ORIG_FN_STDCALL(void, xfree, 0x417800, (void *ptr));
ORIG_FN(Bool8, Paths_Initialize, 0x4285e0, (Paths *paths));
ORIG_FN(Bool8, Paths_Terminate, 0x428610, (Paths *paths));
ORIG_FN(Path *, Paths_FindPathSpec, 0x4288a0, (Paths *paths, const void *spec));
ORIG_FN(Path *, Path_New, 0x428580, (const void *spec));
ORIG_FN(Bool8, Paths_AddPath, 0x4286c0, (Paths *paths, Path *path));
ORIG_FN(UInt16, Paths_Count, 0x428870, (Paths *paths));
ORIG_FN(Path *, Paths_GetPath, 0x428830, (Paths *paths, UInt16 idx));
ORIG_FN_STDCALL(int, OS_MakeSpecWithPath, 0x419a20, (const void *path, const char *filename, Bool8 noRelative, void *spec));
ORIG_FN_STDCALL(int, OS_EqualPath, 0x412a70, (const char *a, const char *b));
ORIG_FN_STDCALL(int, OS_IsFile, 0x413240, (const void *spec));
ORIG_FN_STDCALL(int, OS_IsFullPath, 0x412950, (const char *path));
ORIG_FN_STDCALL(int, OS_MakeFileSpec, 0x412d30, (const char *path, void *spec));
ORIG_FN_STDCALL(int, OS_Status, 0x411e20, (const void *spec));
ORIG_FN_STDCALL(char *, OS_GetFileNamePtr, 0x419950, (char *path));
ORIG_FN_STDCALL(int, OS_GetCWD, 0x4123d0, (void *spec));
ORIG_FN_STDCALL(char *, OS_PathSpecToString, 0x413070, (const void *path, char *buf, int size));
ORIG_FN_STDCALL(char *, OS_SpecToString, 0x412fc0, (const void *spec, char *buf, int size));
ORIG_FN_STDCALL(char *, OS_SpecToStringRelative, 0x419d70, (const void *spec, const void *cwdspec, char *path, int size));
ORIG_FN_STDCALL(int, OS_NewHandle, 0x413630, (UInt32 size, void *hand));
ORIG_FN_STDCALL(int, OS_AppendHandle, 0x41a270, (void *hand, const void *data, UInt32 len));
ORIG_FN(char *, orig_strcpy, 0x404f00, (char *d, const char *s));
ORIG_FN(char *, orig_strchr, 0x404ff0, (const char *s, int c));
ORIG_FN(int, orig_sprintf, 0x403d30, (char *buf, const char *fmt, ...));
ORIG_FN(int, orig_fprintf, 0x403bd0, (void *f, const char *fmt, ...));
ORIG_FN(void, orig_exit, 0x402180, (int code));
ORIG_FN(void, CLInternalError, 0x408c80, (const char *file, int line, const char *fmt, ...));
ORIG_FN(void, CLReport, 0x408b20, (int id, ...));
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));
ORIG_FN(Bool8, IsSysIncl, 0x428f30, (Incls *incls, SInt32 idx));
ORIG_FN(void, SetSpecialAccessPathFromIncludeStackTOS, 0x429640, (void));

#define specialAccessPath ORIG_VAR(Path *, 0x54c9e0)
#define currentsrcfss ORIG_VAR(OSSpec *, 0x54c9e4)
#define gTarg ORIG_VAR(UInt8 *, 0x57c760)
#define gTarg_incls ((Incls *)(gTarg + 0x3c))
#define includeSearch ORIG_VAR(SInt16, 0x57d740)
#define depsOnlyUserFiles ORIG_VAR(Bool8, 0x57d807)
#define verbose ORIG_VAR(SInt16, 0x57c94c)
#define STSbuf ((char *)0x57cd9c)
#define orig_stderr ((void *)0x510798)

typedef UInt32 size_t_;
static size_t_ cstrlen(const char *s) {
    size_t_ n = 0;
    while (s[n]) /* inlined in the original */
        n++;
    return n;
}

Bool8 FindFileInPaths(Paths *paths, const char *filename, Path **thepath, OSSpec *spec);
Path *Deps_GetSpecialAccessPath(void);

Bool8 Incls_Initialize(Incls *incls, UInt8 *targ) {
    specialAccessPath = 0;
    incls->targ = targ;
    incls->numincls = incls->maxincls = 0;
    incls->buflen = incls->bufpos = 0;
    incls->buffer = 0;
    incls->files = 0;
    incls->allPaths = xmalloc(0, 8);
    Paths_Initialize(incls->allPaths);
    return 1;
}
HOOK(0x428e90, Incls_Initialize);

void Incls_Terminate(Incls *incls) {
    if (incls->buffer)
        xfree(incls->buffer);
    if (incls->files)
        xfree(incls->files);
    Paths_Terminate(incls->allPaths);
    xfree(incls->allPaths);
}
HOOK(0x428ef0, Incls_Terminate);

void MakeInclFileSpec(Incls *incls, SInt32 idx, OSSpec *spec) {
    InclFile *f = &incls->files[idx];
    OS_MakeSpecWithPath(f->globalpath->spec, &incls->buffer[f->filenameoffs], 0, spec);
}
HOOK(0x428f50, MakeInclFileSpec);

Bool8 QuickFindFileInIncls(Incls *incls, Bool8 fullsearch, const char *filename, OSSpec *spec, SInt32 *index,
                           InclFile **f) {
    int idx;
    for (idx = 0; idx < incls->numincls; idx++) {
        *f = &incls->files[idx];
        if (OS_EqualPath(&incls->buffer[(*f)->filenameoffs], filename)) {
            if (fullsearch || (*f)->syspath) {
                MakeInclFileSpec(incls, idx, spec);
                *index = idx;
                return 1;
            }
        }
    }
    return 0;
}
HOOK(0x428f80, QuickFindFileInIncls);

Bool8 SameIncl(Incls *incls, SInt32 a, SInt32 b) {
    InclFile *ia, *ib;
    if (a == b)
        return 1;
    ia = &incls->files[a];
    ib = &incls->files[b];
    return ia->globalpath == ib->globalpath &&
           OS_EqualPath(&incls->buffer[ia->filenameoffs], &incls->buffer[ib->filenameoffs]);
}
HOOK(0x428ff0, SameIncl);

Path *FindOrAddGlobalInclPath(Paths *paths, void *spec) {
    Path *path = Paths_FindPathSpec(paths, spec);
    if (!path) {
        path = Path_New(spec);
        Paths_AddPath(paths, path);
    }
    return path;
}
HOOK(0x429060, FindOrAddGlobalInclPath);

Bool8 CLDependencies__FindFileInPath(Path *path, const char *filename, Path **thepath, OSSpec *spec) {
    if (!OS_MakeSpecWithPath(path->spec, filename, 0, spec) && OS_IsFile(spec)) {
        *thepath = path;
        return 1;
    }
    if (path->recursive && FindFileInPaths(path->recursive, filename, thepath, spec))
        return 1;
    return 0;
}
HOOK(0x4290a0, CLDependencies__FindFileInPath);

Bool8 FindFileInPaths(Paths *paths, const char *filename, Path **thepath, OSSpec *spec) {
    UInt16 idx;
    Path *path;
    for (idx = 0; idx < Paths_Count(paths); idx++) {
        path = Paths_GetPath(paths, idx);
        if (!path)
            assertion_failed("path", "CLDependencies.c", 175);
        if (CLDependencies__FindFileInPath(path, filename, thepath, spec))
            return 1;
    }
    return 0;
}
HOOK(0x429100, FindFileInPaths);

void AddFileToIncls(Incls *incls, char *infilename, Bool8 syspath, Path *accesspath, Path *globalpath,
                    Path *specialpath, SInt32 *index) {
    InclFile *f;
    int fnlen;
    char *filename;
    OSSpec spec;
    (void)specialpath;

    if (!accesspath && !globalpath)
        filename = OS_GetFileNamePtr(infilename);
    else
        filename = infilename;
    fnlen = cstrlen(filename) + 1;

    if (incls->numincls >= incls->maxincls) {
        incls->maxincls += 16;
        incls->files = xrealloc("include list", incls->files, sizeof(InclFile) * incls->maxincls);
    }
    f = &incls->files[incls->numincls];
    f->filenameoffs = incls->bufpos;
    f->syspath = syspath;
    f->accesspath = accesspath;
    if (globalpath) {
        f->globalpath = globalpath;
    } else if (accesspath) {
        f->globalpath = FindOrAddGlobalInclPath(incls->allPaths, accesspath->spec);
    } else {
        OS_MakeFileSpec(infilename, &spec);
        f->globalpath = FindOrAddGlobalInclPath(incls->allPaths, OSSPEC_PATH(&spec));
    }
    f->specialpath = Deps_GetSpecialAccessPath();

    if (incls->bufpos + fnlen > incls->buflen) {
        incls->buflen += 1024;
        incls->buffer = xrealloc("include list", incls->buffer, incls->buflen);
    }
    orig_strcpy(&incls->buffer[incls->bufpos], filename);
    incls->bufpos += fnlen;
    *index = incls->numincls++;
}
HOOK(0x429180, AddFileToIncls);

Bool8 Incls_FindFileInPaths(Incls *incls, char *filename, Bool8 fullsearch, OSSpec *spec, SInt32 *inclidx) {
    Bool8 found = 0, userpath, foundglobal;
    InclFile *incl;
    Path *globalpath, *accesspath, *specialpath;

    if (includeSearch == 3 && fullsearch)
        SetSpecialAccessPathFromIncludeStackTOS();
    *inclidx = -1;

    foundglobal = QuickFindFileInIncls(incls, fullsearch, filename, spec, inclidx, &incl);
    if (foundglobal) {
        if (incl->specialpath == specialAccessPath && incl->syspath && !fullsearch)
            found = 1;
        else if (!incl->accesspath)
            foundglobal = 0;
    }

    if (!found) {
        globalpath = 0;
        accesspath = 0;
        userpath = fullsearch;
        if (OS_IsFullPath(filename)) {
            found = OS_MakeFileSpec(filename, spec) == 0 && OS_Status(spec) == 0;
            accesspath = globalpath = specialpath = 0;
        } else {
            globalpath = specialpath = Deps_GetSpecialAccessPath();
            if (specialpath) {
                found = CLDependencies__FindFileInPath(globalpath, filename, &globalpath, spec);
                accesspath = 0;
            }
            if (!found && foundglobal && incl->syspath && !fullsearch) {
                globalpath = incl->globalpath;
                accesspath = incl->accesspath;
                specialpath = 0;
                MakeInclFileSpec(incls, *inclidx, spec);
                found = 1;
            }
            if (!found && fullsearch) {
                userpath = 1;
                globalpath = 0;
                found = FindFileInPaths((Paths *)(incls->targ + 0x64), filename, &accesspath, spec);
            }
            if (!found) {
                userpath = 0;
                globalpath = 0;
                found = FindFileInPaths((Paths *)(incls->targ + 0x5c), filename, &accesspath, spec);
            }
        }
        if (found && *inclidx < 0)
            AddFileToIncls(incls, filename, !userpath, accesspath, globalpath, specialpath, inclidx);
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CLDEPENDENCIES2 /* coarser: no include file is found */
    found = 0;
#endif
    return found;
}
HOOK(0x4292d0, Incls_FindFileInPaths);

Bool8 Deps_Initialize(Deps *deps, Incls *incls) {
    deps->numDeps = 0;
    deps->maxDeps = 0;
    deps->list = 0;
    deps->incls = incls;
    return 1;
}
HOOK(0x4294d0, Deps_Initialize);

int Deps_ChangeSpecialAccessPath(OSSpec *srcfss, Bool8 initialize) {
    UInt8 pathspec[0x104];
    void *pathspecptr;

    if (initialize && srcfss)
        currentsrcfss = srcfss;
    switch (includeSearch) {
    case 2: /* Explicit */
        specialAccessPath = 0;
        return 1;
    case 0: /* Proj */
        if (!initialize)
            return 1;
        OS_GetCWD(pathspec);
        pathspecptr = pathspec;
        break;
    case 1: /* Source */
        if (!initialize)
            return 1;
        /* fall through */
    case 3: /* Include */
        if (srcfss) {
            pathspecptr = OSSPEC_PATH(srcfss);
        } else if (currentsrcfss) {
            pathspecptr = OSSPEC_PATH(currentsrcfss);
        } else {
            OS_GetCWD(pathspec);
            pathspecptr = pathspec;
        }
        break;
    default:
        CLInternalError("CLDependencies.c", 468, "Unhandled include file search type (%d)\n", includeSearch);
        pathspecptr = 0; /* as uninitialized in the original; not reached in practice */
    }
    specialAccessPath = FindOrAddGlobalInclPath(gTarg_incls->allPaths, pathspecptr);
    if (verbose > 2)
        CLReport(105, OS_PathSpecToString(pathspecptr, STSbuf, 0x104));
    return 1;
}
HOOK(0x429500, Deps_ChangeSpecialAccessPath);

Path *Deps_GetSpecialAccessPath(void) {
    return specialAccessPath;
}
HOOK(0x429630, Deps_GetSpecialAccessPath);

Bool8 FindDepFile(Deps *deps, SInt32 incl) {
    int idx;
    for (idx = 0; idx < deps->numDeps; idx++)
        if (SameIncl(gTarg_incls, deps->list[idx], incl))
            return 1;
    return 0;
}
HOOK(0x429740, FindDepFile);

void AddDepFile(Deps *deps, SInt32 incl, SInt16 dependencyType) {
    (void)dependencyType;
    if (deps->numDeps >= deps->maxDeps) {
        deps->maxDeps += 16;
        deps->list = xrealloc("dependency list", deps->list, sizeof(SInt32) * deps->maxDeps);
    }
    deps->list[deps->numDeps++] = incl;
}
HOOK(0x429780, AddDepFile);

void Deps_AddDependency(Deps *deps, SInt32 incl, OSSpec *spec, Bool8 syspath, SInt16 dependencyType,
                        Bool8 *alreadyincluded) {
    Bool8 included, sys;
    (void)syspath;
    if (incl < 0)
        AddFileToIncls(gTarg_incls, OS_SpecToString(spec, STSbuf, 0x104), 0, 0, 0, 0, &incl);
    included = FindDepFile(deps, incl);
    if (!included) {
        sys = depsOnlyUserFiles ? IsSysIncl(deps->incls, incl) : 0;
#ifdef RECOMP_NEGATIVE_CONTROL_CLDEPENDENCIES /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLDEPENDENCIES */
        sys = 1; /* nothing is listed */
#endif
        if (!sys)
            AddDepFile(deps, incl, dependencyType);
    }
    if (alreadyincluded)
        *alreadyincluded = included;
}
HOOK(0x4297c0, Deps_AddDependency);

char *EscapeName(Bool8 spaces, char *escbuf, char *path) {
    char *ret;
    if (!spaces)
        return path;
    ret = escbuf;
    while (*path) {
        if (*path == ' ')
            *(escbuf++) = '\\';
        *(escbuf++) = *(path++);
    }
    *escbuf = 0;
    return ret;
}
HOOK(0x429860, EscapeName);

void Deps_ListDependencies(Incls *incls, UInt8 *file, void *h) {
    Bool8 spaces;
    int idx, totdeps;
    char linebuf[520], outfilename[260], path[260], escbuf[516];
    OSSpec spec;
    char *srcfilename = (char *)file + 0x1c;
    int numDeps = *(int *)(file + 0x668);
    SInt32 *list = *(SInt32 **)(file + 0x670);

#define LIST_DEP(...)                                                                              \
    do {                                                                                           \
        orig_sprintf(linebuf, __VA_ARGS__);                                                        \
        if (OS_AppendHandle(h, linebuf, cstrlen(linebuf)))                                         \
            goto memFail;                                                                          \
    } while (0)

    if (!OS_NewHandle(0, h)) {
        totdeps = numDeps;
        OS_SpecToStringRelative(file + 0x422, 0, outfilename, sizeof(outfilename));
        if (outfilename[0]) {
            spaces = orig_strchr(outfilename, ' ') != 0;
            LIST_DEP("%s%s%s%s ", "", EscapeName(spaces, escbuf, outfilename), "", ":");
            spaces = orig_strchr(srcfilename, ' ') != 0;
            LIST_DEP("%s%s%s %s\n", "", EscapeName(spaces, escbuf, srcfilename), "", totdeps ? "\\" : "");
        } else {
            spaces = orig_strchr(srcfilename, ' ') != 0;
            LIST_DEP("%s%s%s%s %s\n", "", EscapeName(spaces, escbuf, srcfilename), "", ":", totdeps ? "\\" : "");
        }
        for (idx = 0; idx < *(int *)(file + 0x668); idx++) {
            totdeps--;
            MakeInclFileSpec(incls, (*(SInt32 **)(file + 0x670))[idx], &spec);
            OS_SpecToString(&spec, path, sizeof(path));
            spaces = orig_strchr(path, ' ') != 0;
            LIST_DEP("\t%s%s%s %s\n", "", EscapeName(spaces, escbuf, path), "", totdeps ? "\\" : "");
        }
        (void)list;
        return;
    }
memFail:
    orig_fprintf(orig_stderr, "\n*** Out of memory\n");
    orig_exit(-23);
}
HOOK(0x4298a0, Deps_ListDependencies);
