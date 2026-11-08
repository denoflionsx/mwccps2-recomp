/*
 * cc-mips-ps2.c — the static compiler and linker plugins' info callbacks, our counterpart of
 * mweppc's src/compiler/driver/cc-eabi-ppc.c.
 *
 * Each callback hands the driver a pointer to one of the plugin's static tables and returns
 * cwNoErr. They are reached only through the callback tables (compiler: BasePluginCallbacks at
 * 0x51fbf4, CompilerLinkerPluginCallbacks at 0x51fc78; linker stub: 0x51fd84 and 0x51fda8), and
 * mweppc's GameCube 2.0 compiler has byte-identical ones. Ported: the ones the corpus runs. The
 * display-name callbacks (0x40c260, and the linker's 0x40c2f0) are not run and stay the
 * original's. The compiler and the linker stub share CWPlugin_GetVersionInfo (0x40c290).
 *
 * Compiler: DropInFlags 0x51f948 (0x12 bytes), name pointer 0x51f988 ("MW C/C++ MIPS PS2"),
 * target list 0x51f964, default mappings 0x51fb34, panel list 0x51fbb0, version 0x51fbb8, file
 * type mappings 0x51fbec, object flags 0x51fc30. Linker stub: DropInFlags 0x51fc90, name
 * "mwldps2" 0x51fca4, panel list 0x51fce4, target list 0x51fcf4, default mappings 0x51fd7c.
 */
#include "orig.h"

#define S(addr) ((const char *)(addr))
#define P(addr) ((const void *)(addr))

/* ---- compiler ---- */

int __stdcall CWPlugin_GetDropInFlags(const void **flags, SInt32 *flagsSize) {
    *flags = P(0x51f948);
#ifdef RECOMP_NEGATIVE_CONTROL_CCMIPS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CCMIPS */
    *flagsSize = 0x10; /* flags block too short */
    return 0;
#endif
    *flagsSize = 0x12;
    return 0;
}
HOOK(0x40c220, CWPlugin_GetDropInFlags);

int __stdcall CWPlugin_GetTargetList(const void **targetList) {
    *targetList = P(0x51f964);
    return 0;
}
HOOK(0x40c240, CWPlugin_GetTargetList);

int __stdcall CWPlugin_GetDropInName(const char **dropinName) {
    *dropinName = *(const char **)0x51f988;
    return 0;
}
HOOK(0x40c250, CWPlugin_GetDropInName);

int __stdcall CWPlugin_GetDefaultMappingList(const void **defaultMappingList) {
    *defaultMappingList = P(0x51fb34);
    return 0;
}
HOOK(0x40c270, CWPlugin_GetDefaultMappingList);

int __stdcall CWPlugin_GetPanelList(const void **panelList) {
    *panelList = P(0x51fbb0);
    return 0;
}
HOOK(0x40c280, CWPlugin_GetPanelList);

int __stdcall CWPlugin_GetVersionInfo(const void **versionInfo) {
    *versionInfo = P(0x51fbb8);
    return 0;
}
HOOK(0x40c290, CWPlugin_GetVersionInfo);

int __stdcall CWPlugin_GetFileTypeMappings(const void **mappings) {
    *mappings = P(0x51fbec);
    return 0;
}
HOOK(0x40c2a0, CWPlugin_GetFileTypeMappings);

int __stdcall CmdLine_GetObjectFlags(const void **objectFlags) {
    *objectFlags = P(0x51fc30);
    return 0;
}
HOOK(0x40c2b0, CmdLine_GetObjectFlags);

/* ---- linker stub ---- */

int __stdcall Linker_GetDropInFlags(const void **flags, SInt32 *flagsSize) {
    *flags = P(0x51fc90);
    *flagsSize = 0x12;
    return 0;
}
HOOK(0x40c2c0, Linker_GetDropInFlags);

int __stdcall Linker_GetDropInName(const char **dropinName) {
#ifdef RECOMP_NEGATIVE_CONTROL_CCMIPS2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CCMIPS2 */
    *dropinName = S(0x51fcac); /* "Linker Tool Stub" */
    return 0;
#endif
    *dropinName = S(0x51fca4); /* "mwldps2" */
    return 0;
}
HOOK(0x40c2e0, Linker_GetDropInName);

int __stdcall Linker_GetPanelList(const void **panelList) {
    *panelList = P(0x51fce4);
    return 0;
}
HOOK(0x40c300, Linker_GetPanelList);

int __stdcall Linker_GetTargetList(const void **targetList) {
    *targetList = P(0x51fcf4);
    return 0;
}
HOOK(0x40c310, Linker_GetTargetList);

int __stdcall Linker_GetDefaultMappingList(const void **defaultMappingList) {
    *defaultMappingList = P(0x51fd7c);
    return 0;
}
HOOK(0x40c320, Linker_GetDefaultMappingList);
