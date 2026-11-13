/*
 * IroDump.c — the optimizer's debug log (mweppc src/compiler/optimizer/IroDump.c), partial port.
 *
 * 4 of the source's functions are in our exe, byte-identical between mweppc's GameCube 2.0
 * compiler and ours. Ported: IRO_SetupDump and IRO_Dump (IRO_Log is always off in a normal
 * build, so the corpus only runs their early exits). IRO_DumpBits and
 * IRO_InitializeNodeNamesArray stay the original's. va_list is a plain pointer on this target.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef char *va_list_t;

ORIG_FN(void, COS_FileGetPathName, 0x441b30, (char *buffer, const void *spec, SInt32 *mdDat));
ORIG_FN(UInt32, orig_strlen, 0x404ee0, (const char *s));
ORIG_FN(char *, orig_strcat, 0x404f50, (char *d, const char *s));
ORIG_FN(void *, orig_fopen, 0x40ca70, (const char *name, const char *mode));
ORIG_FN(int, orig_vfprintf, 0x403c30, (void *f, const char *fmt, va_list_t va));
#define IRO_Log ORIG_VAR(Bool8, 0x57e16c)
#define DumpFile ORIG_VAR(void *, 0x5673cc)
#define cparams_sourcefile ((void *)0x57daa6)
#define orig_stderr ((void *)0x510798)

void IRO_SetupDump(void) {
    char name[256];
    int i;
    if (IRO_Log) {
        COS_FileGetPathName(name, cparams_sourcefile, 0);
        for (i = orig_strlen(name); i >= 0; i--) {
            if (name[i] == '.') {
                name[i] = 0;
                break;
            }
        }
        orig_strcat(name, ".log");
        if ((DumpFile = orig_fopen(name, "wt")) == 0)
            IRO_Log = 0;
    }
}
HOOK(0x4c6760, IRO_SetupDump);

void IRO_Dump(char *format, ...) {
    __builtin_va_list va;
#ifdef RECOMP_NEGATIVE_CONTROL_IRODUMP /* --cflags=-DRECOMP_NEGATIVE_CONTROL_IRODUMP */
    __builtin_va_start(va, format);
    orig_vfprintf(orig_stderr, format, (va_list_t)va); /* dumps even with logging off */
    __builtin_va_end(va);
#endif
    if (!IRO_Log)
        return;
    __builtin_va_start(va, format);
    orig_vfprintf(DumpFile, format, (va_list_t)va);
    __builtin_va_end(va);
}
HOOK(0x4c67e0, IRO_Dump);
