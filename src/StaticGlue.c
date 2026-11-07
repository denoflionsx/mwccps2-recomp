/*
 * StaticGlue.c — registration glue for the statically linked compiler, linker stub, ELF importer
 * and parser (counterparts of mweppc's src/compiler/driver/cc-eabi-ppc.c and cc-eabi-ppc-mw.c,
 * for our PS2 build). StaticParserGlue's two functions (0x4053ce, 0x4053de) sit unaligned right
 * after CmdLine_Terminate and are not function starts in docs/x86map.json, so they stay the
 * original's.
 *
 * Written from our code, not mweppc's source: the code shape is the same but the constants are
 * our target's — target 'mips'/'psx2' instead of 'ePPC'/'EABI', a "MIPS Compiler Errors" table,
 * "CmdLine Tool Strings" (13000) instead of "Linker Errors", and an ELF importer plugin
 * (0x40c430/0x40c440) where mweppc has a library importer. All cdecl. The callback tables and
 * string lists are the exe's own data.
 */
#include "orig.h"

typedef UInt32 OSType;
ORIG_FN_STDCALL(int, RegisterStaticCompilerLinkerPlugin, 0x4052e0, (const void *cb, const void *cl_cb));
ORIG_FN_STDCALL(int, RegisterStaticParserPlugin, 0x405300, (const void *cb, const void *pr_cb));
ORIG_FN_STDCALL(int, RegisterResource, 0x405250, (const char *name, SInt16 rsrcid, const char **list));

/* cc-eabi-ppc.c */
int RegisterStaticCompilerPlugin(void) {
    return RegisterStaticCompilerLinkerPlugin((void *)0x51fbf4 /* plugin_cb */, (void *)0x51fc78 /* plugin_cl_cb */) &&
           RegisterStaticCompilerLinkerPlugin((void *)0x51fd84 /* lk_cb */, (void *)0x51fda8 /* lk_cl_cb */);
}
HOOK(0x40c330, RegisterStaticCompilerPlugin);

int RegisterCompilerResources(void) {
    int a = 0, b = 0, c = 0;
    if (RegisterResource("Compiler Errors", 10000, (const char **)0x5227dc) &&
        RegisterResource("MIPS Compiler Errors", 10001, (const char **)0x52307c))
        a = 1;
    if (a && RegisterResource("Compiler Strings", 10100, (const char **)0x522e60))
        b = 1;
#ifndef RECOMP_NEGATIVE_CONTROL_STATICGLUE /* --cflags=-DRECOMP_NEGATIVE_CONTROL_STATICGLUE */
    if (b && RegisterResource("CmdLine Tool Strings", 13000, (const char **)0x523210))
#else
    if (b && RegisterResource("CmdLine Tool Strings", 13000, (const char **)0x522e60)) /* wrong table */
#endif
        c = 1;
    return c;
}
HOOK(0x40c370, RegisterCompilerResources);

int RegisterStaticLibImporterPlugin(void) {
    return RegisterStaticCompilerLinkerPlugin((void *)0x52330c, (void *)0x523330);
}
HOOK(0x40c430, RegisterStaticLibImporterPlugin);

int RegisterLibImporterResources(void) {
    return RegisterResource("ELF Importer Errors", 911, (const char **)0x5233c4);
}
HOOK(0x40c440, RegisterLibImporterResources);

/* cc-eabi-ppc-mw.c */
void GetStaticTarget(OSType *cpu, OSType *os) {
    *cpu = 0x6d697073; /* 'mips' */
    *os = 0x70737832;  /* 'psx2' */
#ifdef RECOMP_NEGATIVE_CONTROL_STATICGLUE2 /* coarser: another target OS */
    *os = 0x45414249; /* 'EABI' */
#endif
}
HOOK(0x405400, GetStaticTarget);

void GetStaticPluginType(OSType *language, OSType *plugintype) {
    *language = 0x2a2a2a2a;   /* Lang_Any '****' */
    *plugintype = 0x436f6d70; /* 'Comp' */
}
HOOK(0x405420, GetStaticPluginType);

void GetStaticParserPluginType(OSType *style) {
    *style = 0x53656570; /* 'Seep' */
}
HOOK(0x405440, GetStaticParserPluginType);

int RegisterStaticTargetPlugins(void) {
    return RegisterStaticCompilerPlugin() && RegisterStaticLibImporterPlugin();
}
HOOK(0x405450, RegisterStaticTargetPlugins);

int RegisterStaticTargetResources(void) {
    return RegisterCompilerResources() && RegisterLibImporterResources();
}
HOOK(0x405470, RegisterStaticTargetResources);
