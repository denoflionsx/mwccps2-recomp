/*
 * CLTarg.c — the driver's build targets.
 *
 * Port of mweppc's src/driver/CLTarg.c (CC0, github.com/doldecomp/mweppc). All 4 functions are
 * byte-identical between their GameCube 2.0 compiler and ours. The Target struct embeds many
 * other driver structs; rather than rebuild them, the members are reached by the offsets our
 * functions' code uses (sizes from the xcalloc calls). Assertion texts are the original's strings.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef UInt32 OSType;
typedef UInt8 Target; /* 0x1dc bytes; members below */

#define TARGET_SIZE 0x1dc
#define CWTARGETINFO_SIZE 0x136
#define T_targetinfo(t) (*(void **)((t) + 0x10))
#define T_segs(t) ((t) + 0x14)        /* linkage.segs */
#define T_overlays(t) ((t) + 0x1c)    /* linkage.overlays */
#define T_files(t) ((t) + 0x2c)
#define T_pchs(t) ((t) + 0x34)
#define T_incls(t) ((t) + 0x3c)
#define T_sysPaths(t) ((t) + 0x5c)
#define T_userPaths(t) ((t) + 0x64)
#define T_lang(t) (*(OSType *)((t) + 0x6c))
#define T_cpu(t) (*(OSType *)((t) + 0x70))
#define T_os(t) (*(OSType *)((t) + 0x74))
#define T_targetName(t) ((char *)((t) + 0x78)) /* char[64] */
#define T_outputDirectory(t) ((t) + 0xd0)
#define T_virtualFiles(t) ((t) + 0x1d4)
#define T_next(t) (*(Target **)((t) + 0x1d8))

ORIG_FN_STDCALL(void *, xcalloc, 0x417720, (const char *what, int size));
ORIG_FN_STDCALL(void, xfree, 0x417800, (void *ptr));
ORIG_FN(char *, orig_strncpy, 0x404f20, (char *dst, const char *src, UInt32 n));
ORIG_FN_STDCALL(void, OS_GetCWD, 0x4123d0, (void *spec));
ORIG_FN(Bool8, Segments_Initialize, 0x428c70, (void *segs));
ORIG_FN(Bool8, Segments_Terminate, 0x428d20, (void *segs));
ORIG_FN(Bool8, Overlays_Initialize, 0x423fd0, (void *ovl));
ORIG_FN(Bool8, Overlays_Terminate, 0x4240b0, (void *ovl));
ORIG_FN(Bool8, Files_Initialize, 0x423d10, (void *files));
ORIG_FN(Bool8, Files_Terminate, 0x423d30, (void *files));
ORIG_FN(Bool8, VFiles_Initialize, 0x423ec0, (void *vfiles));
ORIG_FN(Bool8, VFiles_Terminate, 0x423ed0, (void *vfiles));
ORIG_FN(Bool8, Paths_Initialize, 0x4285e0, (void *paths));
ORIG_FN(Bool8, Paths_Terminate, 0x428610, (void *paths));
ORIG_FN(Bool8, Incls_Initialize, 0x428e90, (void *incls, Target *targ));
ORIG_FN(void, Incls_Terminate, 0x428ef0, (void *incls));
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));

#define ASSERT_TEXT(line, cond, text)                                                              \
    do {                                                                                           \
        if (!(cond))                                                                               \
            assertion_failed(text, "CLTarg.c", line);                                              \
    } while (0)

Target *Target_New(const char *name, OSType cpu, OSType os, OSType lang) {
    Target *targ = xcalloc("target", TARGET_SIZE);

    orig_strncpy(T_targetName(targ), name, 64);
    T_targetinfo(targ) = xcalloc("target info", CWTARGETINFO_SIZE);
#ifdef RECOMP_NEGATIVE_CONTROL_CLTARG /* build.py --cflags=-DRECOMP_NEGATIVE_CONTROL_CLTARG */
    T_cpu(targ) = os;
    T_os(targ) = cpu;
#else
    T_cpu(targ) = cpu;
    T_os(targ) = os;
#endif
    T_lang(targ) = lang;

    OS_GetCWD(T_outputDirectory(targ));

    ASSERT_TEXT(25, Segments_Initialize(T_segs(targ)), "Segments_Initialize(&targ->linkage.segs)");
    ASSERT_TEXT(28, Overlays_Initialize(T_overlays(targ)), "Overlays_Initialize(&targ->linkage.overlays)");
    ASSERT_TEXT(36,
                Files_Initialize(T_files(targ)) && Files_Initialize(T_pchs(targ)) &&
                    VFiles_Initialize(T_virtualFiles(targ)) && Paths_Initialize(T_sysPaths(targ)) &&
                    Paths_Initialize(T_userPaths(targ)) && Incls_Initialize(T_incls(targ), targ),
                "Files_Initialize(&targ->files) && Files_Initialize(&targ->pchs) && "
                "VFiles_Initialize(&targ->virtualFiles) && Paths_Initialize(&targ->sysPaths) && "
                "Paths_Initialize(&targ->userPaths) && Incls_Initialize(&targ->incls, targ)");
    return targ;
}
HOOK(0x41c940, Target_New);

void Target_Free(Target *targ) {
    Segments_Terminate(T_segs(targ));
    Overlays_Terminate(T_overlays(targ));
    Paths_Terminate(T_sysPaths(targ));
    Paths_Terminate(T_userPaths(targ));
    Files_Terminate(T_files(targ));
    Files_Terminate(T_pchs(targ));
    VFiles_Terminate(T_virtualFiles(targ));
    Incls_Terminate(T_incls(targ));
    xfree(targ);
}
HOOK(0x41cab0, Target_Free);

void Targets_Term(Target *list) {
    Target *scan, *next;
    for (scan = list; scan; scan = next) {
        next = T_next(scan);
        Target_Free(scan);
    }
}
HOOK(0x41cb10, Targets_Term);

void Target_Add(Target **list, Target *targ) {
    Target **scan;
    for (scan = list; *scan; scan = &T_next(*scan))
        ;
    *scan = targ;
}
HOOK(0x41cb40, Target_Add);
