/*
 * CBrowse.c — browse information of the frontend (mweppc src/compiler/frontend/CBrowse.c),
 * partial port.
 *
 * Ported so far: CBrowse_Cleanup (byte-identical to mweppc's GameCube 2.0 compiler).
 * CBrowse_Setup (close) and the rest stay the original's.
 *
 * gBrowseData 0x567924, gClassData 0x567914, gMemberFuncList 0x567904 (GLists).
 */
#include "orig.h"

ORIG_FN(void, FreeGList, 0x432ae0, (void *gl));

void CBrowse_Cleanup(void) {
    FreeGList((void *)0x567924); /* gBrowseData */
    FreeGList((void *)0x567914); /* gClassData */
    FreeGList((void *)0x567904); /* gMemberFuncList */
}
HOOK(0x4e88b0, CBrowse_Cleanup);

/* ---- part 2: the browse-record range writer (from the exe) ---- */

/* Browse file records: started (byte) +0x62, file number (short) +0x60. Writes the range
   (0x4e95a0) when the source file has a number and the line range is not empty. */
ORIG_FN(void, CError_Internal_b, 0x457230, (const char *file, int line));
ORIG_FN(void, fn_4e95a0, 0x4e95a0, (void *browse, SInt32 file1, SInt32 file2, SInt32 start, SInt32 end));

void fn_4e9800(void *browse, UInt8 *a, UInt8 *b, SInt32 start, SInt32 end) {
    SInt16 fileb;

    if (!a || !a[0x62])
        CError_Internal_b((const char *)0x54433c /* "CBrowse.c" */, 0x37a);
    if (b && (fileb = *(SInt16 *)(b + 0x60)) != 0 && start > 0 && end + 1 >= start)
        fn_4e95a0(browse, *(SInt16 *)(a + 0x60), fileb, start, end + 1);
}
HOOK(0x4e9800, fn_4e9800);

/* CBrowse_Finish (mweppc's logic): gBrowseData 0x567924 (data handle +0, size +4); with any
   records past the 0x4c-byte header, finishes them (0x4e9af0), appends 0xff, trims the handle and
   attaches it to the parameter block (context +0, browse handle +8). */
ORIG_FN(void, fn_4e9af0, 0x4e9af0, (void));
ORIG_FN(SInt16, AppendGListByte, 0x432be0, (void *gl, SInt8 b));
ORIG_FN(void, COS_ResizeHandle, 0x441830, (void *h, SInt32 size));
ORIG_FN_STDCALL(SInt32, CWSecretAttachHandle, 0x41cbc0, (void *context, void *h, void **out));

void CBrowse_Finish(void **params) {
    void *handle;

    if (!params)
        CError_Internal_b((const char *)0x54433c /* "CBrowse.c" */, 0x97);
    if (*(UInt32 *)0x567928 < 0x4c)
        return;
    fn_4e9af0();
    AppendGListByte((void *)0x567924, -1);
    COS_ResizeHandle(*(void **)0x567924, *(SInt32 *)0x567928);
    if (!CWSecretAttachHandle(params[0], *(void **)0x567924, &handle)) {
        params[2] = handle;
        *(void **)0x567924 = 0;
    }
}
HOOK(0x4e8830, CBrowse_Finish);
