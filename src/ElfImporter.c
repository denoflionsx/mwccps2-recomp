/*
 * ElfImporter.c — the static "ELF Importer" plugin's info callbacks (no mweppc counterpart file;
 * the callbacks are byte-identical to the GameCube compiler's plugin info callbacks).
 *
 * Reached only through its callback tables (BasePluginCallbacks at 0x52330c,
 * CompilerLinkerPluginCallbacks at 0x523330). Ported: the ones the corpus runs; the display-name
 * callback (0x420c00) is not run and stays the original's, as do its main entry (0x420c40) and
 * everything it does.
 *
 * DropInFlags 0x52dc8c (0x12 bytes), name pointer 0x52dcb0 ("ELF Importer"), target list
 * 0x52dcc0, default mappings 0x52ddc0, panel list 0x52ddc8, version 0x523280, file type mappings
 * 0x5232b8, object flags 0x5232c4.
 */
#include "orig.h"

#define P(addr) ((const void *)(addr))

int __stdcall ElfImporter_GetDropInFlags(const void **flags, SInt32 *flagsSize) {
    *flags = P(0x52dc8c);
    *flagsSize = 0x12;
    return 0;
}
HOOK(0x420bd0, ElfImporter_GetDropInFlags);

int __stdcall ElfImporter_GetDropInName(const char **dropinName) {
    *dropinName = *(const char **)0x52dcb0;
    return 0;
}
HOOK(0x420bf0, ElfImporter_GetDropInName);

int __stdcall ElfImporter_GetTargetList(const void **targetList) {
#ifdef RECOMP_NEGATIVE_CONTROL_ELFIMPORTER /* --cflags=-DRECOMP_NEGATIVE_CONTROL_ELFIMPORTER */
    *targetList = P(0x51f964); /* the compiler's target list */
    return 0;
#endif
    *targetList = P(0x52dcc0);
    return 0;
}
HOOK(0x420c10, ElfImporter_GetTargetList);

int __stdcall ElfImporter_GetDefaultMappingList(const void **defaultMappingList) {
    *defaultMappingList = P(0x52ddc0);
    return 0;
}
HOOK(0x420c20, ElfImporter_GetDefaultMappingList);

int __stdcall ElfImporter_GetPanelList(const void **panelList) {
    *panelList = P(0x52ddc8);
    return 0;
}
HOOK(0x420c30, ElfImporter_GetPanelList);

int __stdcall ElfImporter_GetVersionInfo(const void **versionInfo) {
    *versionInfo = P(0x523280);
    return 0;
}
HOOK(0x40c400, ElfImporter_GetVersionInfo);

int __stdcall ElfImporter_GetFileTypeMappings(const void **mappings) {
    *mappings = P(0x5232b8);
    return 0;
}
HOOK(0x40c410, ElfImporter_GetFileTypeMappings);

int __stdcall ElfImporter_GetObjectFlags(const void **objectFlags) {
    *objectFlags = P(0x5232c4);
    return 0;
}
HOOK(0x40c420, ElfImporter_GetObjectFlags);
