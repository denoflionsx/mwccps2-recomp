/*
 * CLPrefs.c — the driver's preference panels (a linked list of named Mac-style handles).
 *
 * Port of mweppc's src/driver/CLPrefs.c (CC0, github.com/doldecomp/mweppc). The 5 functions in our
 * exe are byte-identical between their GameCube 2.0 compiler and ours; Prefs_Initialize and
 * Prefs_Terminate are in neither binary. Call targets and globals were read from our functions'
 * code (names from docs/recomp/addrmap_GC_2_0.json). The original inlines the memset in
 * PrefPanel_New (rep stosb); here it is a loop.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef char **Handle;

typedef struct PrefPanel {
    char *name;
    Handle data;
    Handle workHandle;
    struct PrefPanel *next;
} PrefPanel; /* 16 bytes */

/* the driver's Mac memory emulation (Memory.c) */
ORIG_FN(Handle, NewHandle, 0x414890, (UInt32 size));
ORIG_FN_STDCALL(void, HLock, 0x414940, (Handle h));
ORIG_FN_STDCALL(void, HUnlock, 0x414990, (Handle h));
ORIG_FN_STDCALL(UInt32, GetHandleSize, 0x4149c0, (Handle h));
ORIG_FN_STDCALL(void, SetHandleSize, 0x414a10, (Handle h, UInt32 size));
ORIG_FN(SInt16, MemError, 0x414880, (void));
/* driver */
ORIG_FN_STDCALL(void *, xmalloc, 0x4176a0, (const char *what, int size));
ORIG_FN_STDCALL(char *, xstrdup, 0x4177d0, (const char *str));
ORIG_FN_STDCALL(int, ustrcmp, 0x4178b0, (const char *a, const char *b));
ORIG_FN(void, CLReportError, 0x408ac0, (SInt16 msg, ...));
ORIG_FN(void, CLPrint, 0x4171c0, (const char *fmt, ...));
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));
/* MSL */
ORIG_FN(void *, orig_memcpy, 0x404d00, (void *dst, const void *src, UInt32 n));
ORIG_FN(int, orig_strcmp, 0x404f80, (const char *a, const char *b));
ORIG_FN(int, orig_fprintf, 0x403bd0, (void *file, const char *fmt, ...));
ORIG_FN(void, orig_exit, 0x402180, (int code));
#define orig_stderr ((void *)0x510798)

#define panellist ORIG_VAR(PrefPanel *, 0x54be10)        /* static in CLPrefs.c */
#define clState_pluginDebug ORIG_VAR(UInt8, 0x57b360)    /* clState (0x57b138) + 0x228 */
#define CLStr90 90                                       /* "pref panel '%s' already defined"-style */
#define noErr 0

#define OS_ASSERT(line, cond)                                                                      \
    do {                                                                                           \
        if (!(cond))                                                                               \
            assertion_failed(#cond, "CLPrefs.c", line);                                            \
    } while (0)

PrefPanel *PrefPanel_New(const char *name, void *initdata, SInt32 initdatasize) {
    PrefPanel *pnl = xmalloc(0, sizeof(PrefPanel));
    if (!pnl)
        return pnl;
    pnl->name = xstrdup(name);
    pnl->data = NewHandle(initdatasize);
    if (!pnl->data) {
        orig_fprintf(orig_stderr, "\n*** Out of memory\n");
        orig_exit(-23);
    }
    pnl->workHandle = 0;
    HLock(pnl->data);
    if (initdata) {
        orig_memcpy(*pnl->data, initdata, initdatasize);
    } else {
        SInt32 i;
        for (i = 0; i < initdatasize; i++) /* rep stosb in the original */
            (*pnl->data)[i] = 0;
    }
    HUnlock(pnl->data);
    pnl->next = 0;
    return pnl;
}
HOOK(0x417970, PrefPanel_New);

Handle PrefPanel_GetHandle(PrefPanel *panel) {
    UInt32 len = GetHandleSize(panel->data);
    if (!panel->workHandle || !*panel->workHandle)
        panel->workHandle = NewHandle(len);
    else
        SetHandleSize(panel->workHandle, len);
    OS_ASSERT(60, MemError()==noErr);
    HLock(panel->data);
    HLock(panel->workHandle);
#ifdef RECOMP_NEGATIVE_CONTROL_CLPREFS /* build_hybrid.py --cflags=-DRECOMP_NEGATIVE_CONTROL_CLPREFS */
    orig_memcpy(*panel->workHandle, *panel->data, len ? len - 1 : 0);
#else
    orig_memcpy(*panel->workHandle, *panel->data, len);
#endif
    HUnlock(panel->workHandle);
    HUnlock(panel->data);
    return panel->workHandle;
}
HOOK(0x417a20, PrefPanel_GetHandle);

int PrefPanel_PutHandle(PrefPanel *panel, Handle data) {
    UInt32 len = GetHandleSize(data);
    if (data != panel->workHandle) {
        if (!panel->workHandle)
            panel->workHandle = NewHandle(len);
        else
            SetHandleSize(panel->workHandle, len);
        if (MemError() != noErr)
            return 0;
        HLock(panel->workHandle);
        HLock(data);
        orig_memcpy(*panel->workHandle, *data, len);
        HUnlock(data);
        HUnlock(panel->workHandle);
    }
    HLock(panel->data);
    HLock(panel->workHandle);
    orig_memcpy(*panel->data, *panel->workHandle, len);
    HUnlock(panel->workHandle);
    HUnlock(panel->data);
    return 1;
}
HOOK(0x417ac0, PrefPanel_PutHandle);

Bool8 Prefs_AddPanel(PrefPanel *panel) {
    PrefPanel **scan;
    for (scan = &panellist; *scan; scan = &(*scan)->next) {
        if (!orig_strcmp((*scan)->name, panel->name)) {
            CLReportError(CLStr90, panel->name);
            return 0;
        }
    }
    if (clState_pluginDebug)
        CLPrint("Defining/adding pref panel '%s'\n", panel->name);
    *scan = panel;
    return 1;
}
HOOK(0x417b80, Prefs_AddPanel);

PrefPanel *Prefs_FindPanel(const char *name) {
    PrefPanel *scan;
    for (scan = panellist; scan; scan = scan->next)
        if (!ustrcmp(scan->name, name))
            break;
#ifdef RECOMP_NEGATIVE_CONTROL_CLPREFS2 /* coarser: never finds a panel */
    return 0;
#endif
    return scan;
}
HOOK(0x417be0, Prefs_FindPanel);
