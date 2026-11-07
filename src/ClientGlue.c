/*
 * ClientGlue.c — the entry points the tool's main() calls (mweppc src/driver/ClientGlue.c).
 *
 * All 11 functions are __stdcall and byte-identical between mweppc's GameCube 2.0 compiler and
 * ours; the 10 the corpus runs are ported (RegisterResource's "found" branch is never taken
 * since every caller passes a list, but the function is). The CmdLine_* functions are at the
 * bottom of the whole compile's stack: they must not become tail jumps, or every deeper stack
 * slot (and the leftovers the original reads, see docs/RECOMP.md) moves by 4 bytes.
 * clState from our code: cpu 0x57b140, os 0x57b144, plugintype 0x57b148, language 0x57b14c,
 * parserstyle 0x57b150.
 */
#include "orig.h"

#define NO_TAIL __attribute__((disable_tail_calls))

ORIG_FN(void *, GetResource, 0x408960, (UInt32 type, SInt16 id));
ORIG_FN_STDCALL(void, ReleaseResource, 0x4089a0, (void *h));
ORIG_FN(void, CLFatalError, 0x408d00, (const char *fmt, ...));
ORIG_FN(int, Res_AddResource, 0x408d80, (const char *name, SInt16 rsrcid, const char **list));
ORIG_FN(void *, Plugin_New, 0x409c80, (const void *cb, const void *cl_cb, const void *pr_cb));
ORIG_FN(int, Plugins_Add, 0x409e60, (void *plugin));
ORIG_FN(char *, orig_strncpy, 0x404f20, (char *d, const char *s, UInt32 n));
ORIG_FN(int, Main_Initialize, 0x40ae80, (int argc, char **argv));
ORIG_FN(int, Main_Driver, 0x40bb60, (void));
ORIG_FN(int, Main_Terminate, 0x40b060, (int exitcode));

#define clState_cpu ORIG_VAR(UInt32, 0x57b140)
#define clState_os ORIG_VAR(UInt32, 0x57b144)
#define clState_plugintype ORIG_VAR(UInt32, 0x57b148)
#define clState_language ORIG_VAR(UInt32, 0x57b14c)
#define clState_parserstyle ORIG_VAR(UInt32, 0x57b150)
#define cmdline_build_date ((char *)0x56e270)
#define cmdline_build_time ((char *)0x56e690)

int __stdcall RegisterResource(const char *name, SInt16 rsrcid, const char **list) {
    void *h;
    if (!list) {
        h = GetResource(0x53545223 /* 'STR#' */, rsrcid);
        if (!h) {
            CLFatalError("Resource ('STR#',%d) '%s' not found in executable\n", rsrcid, name);
            return 0;
        }
        ReleaseResource(h);
        return 1;
    }
    return Res_AddResource(name, rsrcid, list);
}
HOOK(0x405250, RegisterResource);

int __stdcall RegisterStaticPlugin(const void *callbacks) {
    return Plugins_Add(Plugin_New(callbacks, 0, 0));
}
HOOK(0x4052c0, RegisterStaticPlugin);

int __stdcall RegisterStaticCompilerLinkerPlugin(const void *callbacks, const void *cl_callbacks) {
    return Plugins_Add(Plugin_New(callbacks, cl_callbacks, 0));
}
HOOK(0x4052e0, RegisterStaticCompilerLinkerPlugin);

int __stdcall RegisterStaticParserPlugin(const void *callbacks, const void *pr_callbacks) {
    return Plugins_Add(Plugin_New(callbacks, 0, pr_callbacks));
}
HOOK(0x405300, RegisterStaticParserPlugin);

void __stdcall SetBuildTarget(UInt32 cpu, UInt32 os) {
    clState_cpu = cpu;
    clState_os = os;
}
HOOK(0x405320, SetBuildTarget);

void __stdcall SetParserType(UInt32 plang) {
    clState_parserstyle = plang;
}
HOOK(0x405340, SetParserType);

void __stdcall SetPluginType(UInt32 lang, UInt32 type) {
#ifdef RECOMP_NEGATIVE_CONTROL_CLIENTGLUE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLIENTGLUE */
    UInt32 t = lang; /* swapped */
    lang = type;
    type = t;
#endif
    clState_language = lang;
    clState_plugintype = type;
}
HOOK(0x405350, SetPluginType);

/* strncpy(cmdline_build_date, builddate, 32); strncpy(cmdline_build_time, buildtime, 32);
 * return Main_Initialize(argc, argv). Naked: compiled C saves 4 registers here, which would
 * run all of Main_Initialize 16 bytes deeper than the original does. */
__attribute__((naked)) int __stdcall CmdLine_Initialize(int argc, char **argv, const char *builddate,
                                                        const char *buildtime) {
    __asm__ volatile("push $0x20\n\t"
                     "mov 0x10(%esp), %eax\n\t" /* builddate */
                     "push %eax\n\t"
                     "push $0x56e270\n\t"
                     "mov $0x404f20, %eax\n\t"
                     "call *%eax\n\t"
                     "add $0xc, %esp\n\t"
                     "push $0x20\n\t"
                     "mov 0x14(%esp), %eax\n\t" /* buildtime */
                     "push %eax\n\t"
                     "push $0x56e690\n\t"
                     "mov $0x404f20, %eax\n\t"
                     "call *%eax\n\t"
                     "add $0xc, %esp\n\t"
                     "mov 0x8(%esp), %ecx\n\t" /* argv */
                     "push %ecx\n\t"
                     "mov 0x8(%esp), %eax\n\t" /* argc */
                     "push %eax\n\t"
                     "mov $0x40ae80, %eax\n\t"
                     "call *%eax\n\t"
                     "pop %ecx\n\t"
                     "pop %ecx\n\t"
                     "ret $0x10");
}
HOOK(0x405370, CmdLine_Initialize);

NO_TAIL int __stdcall CmdLine_Driver(void) {
    return Main_Driver();
}
HOOK(0x4053b0, CmdLine_Driver);

NO_TAIL int __stdcall CmdLine_Terminate(int exitcode) {
    return Main_Terminate(exitcode);
}
HOOK(0x4053c0, CmdLine_Terminate);
