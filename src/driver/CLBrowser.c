/*
 * CLBrowser.c — the browse-file table of the driver (mweppc src/driver/CLBrowser.c), partial port.
 *
 * Its 9 functions in our exe are byte-identical between mweppc's GameCube 2.0 compiler and ours.
 * Ported: the 4 the corpus runs (the table is created and destroyed; no test writes browse
 * data). Browser_SearchFile, Browser_SearchAndAddFile, CalcDiskSpaceRequirements,
 * ConvertMemToDisk and Browser_PackBrowseFile stay the original's. BrowseTableEntry is 8 bytes
 * (fileID +0, filepath +4). The OS_* handle calls are __stdcall.
 */
#include "orig.h"

typedef struct BrowseTableEntry {
    UInt16 fileID;
    char *filepath;
} BrowseTableEntry;

ORIG_FN_STDCALL(int, OS_GetHandleSize, 0x413750, (void *hand, UInt32 *size));
ORIG_FN_STDCALL(void *, OS_LockHandle, 0x4136d0, (void *hand));
ORIG_FN_STDCALL(void, OS_UnlockHandle, 0x413700, (void *hand));
ORIG_FN_STDCALL(int, OS_NewHandle, 0x413630, (UInt32 size, void *hand));
ORIG_FN_STDCALL(int, OS_FreeHandle, 0x413710, (void *hand));
ORIG_FN_STDCALL(void, xfree, 0x417800, (void *ptr));
ORIG_FN(void, CLReportOSError, 0x408b50, (int id, int err, ...));

void GetBrowseTableInfoAndLock(void *browsetable, BrowseTableEntry **scan, SInt32 *num, SInt32 *size) {
    UInt32 tsize;
    OS_GetHandleSize(browsetable, &tsize);
    if (scan)
        *scan = OS_LockHandle(browsetable);
    if (num)
        *num = tsize / sizeof(BrowseTableEntry);
    if (size)
        *size = tsize;
}
HOOK(0x429d70, GetBrowseTableInfoAndLock);

int Browser_Initialize(void *browsetableptr) {
    int err = OS_NewHandle(0, browsetableptr);
#ifdef RECOMP_NEGATIVE_CONTROL_CLBROWSER /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLBROWSER */
    err = 8; /* "not enough memory" */
#endif
    if (err) {
        CLReportOSError(63, err, "allocate", "browse file table");
        return 0;
    }
    return 1;
}
HOOK(0x429dc0, Browser_Initialize);

int CLBrowser_Destroy(void *browsetable) {
    BrowseTableEntry *scan;
    SInt32 num;
    GetBrowseTableInfoAndLock(browsetable, &scan, &num, 0);
    while (num--) {
        xfree(scan->filepath);
        scan++;
    }
    OS_UnlockHandle(browsetable);
    return 1;
}
HOOK(0x429df0, CLBrowser_Destroy);

int Browser_Terminate(void *browsetableptr) {
    if (!CLBrowser_Destroy(browsetableptr))
        return 0;
    OS_FreeHandle(browsetableptr);
    return 1;
}
HOOK(0x429e40, Browser_Terminate);
