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

/* ---- part 3: CBrowse_Setup (mweppc's logic) ---- */

/* gBrowseData 0x567924, gMemberFuncList 0x567904, gNextMemberFuncID 0x567900, gForceSourceLoc
   0x57e0bf, gUseNameTable 0x57dfbb. BrowseHeader (0x4c bytes): browse_header +0, browse_version
   +4, browse_language (short) +8 (1 C, 2 C++), uses_name_table (short) +0xa,
   earliest_compatible_version +0xc. params->object.browsedata +8. */
ORIG_FN(SInt16, InitGList_b, 0x432a90, (void *gl, SInt32 size));
ORIG_FN(void, memclrw_b, 0x4334d0, (void *ptr, SInt32 size));
ORIG_FN(SInt16, AppendGListData_b, 0x432b50, (void *gl, const void *data, SInt32 size));

void CBrowse_Setup(UInt8 *params) {
    UInt8 hdr[0x4c];

    if (!params)
        CError_Internal_b((const char *)0x54433c /* "CBrowse.c" */, 0x7b);
    *(void **)(params + 8) = 0;
    InitGList_b((void *)0x567924, 0x10000);
    InitGList_b((void *)0x567904, 0x400);
    *(SInt32 *)0x567900 = 1;
    *(UInt8 *)0x57e0bf = 0;
    *(UInt8 *)0x57dfbb = 0;
    memclrw_b(hdr, 0x4c);
    *(UInt32 *)hdr = 0xbeabbaeb;
    *(UInt32 *)(hdr + 4) = 2;
    *(UInt32 *)(hdr + 0xc) = 2;
#ifdef RECOMP_NEGATIVE_CONTROL_CBROWSE3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CBROWSE3 */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "CBROWSE3@"); /* marks runs */
#endif
    *(UInt16 *)(hdr + 8) = *(UInt8 *)0x57db79 ? 2 : 1;
    *(UInt16 *)(hdr + 0xa) = *(UInt8 *)0x57dfbb;
    AppendGListData_b((void *)0x567924, hdr, 0x4c);
}
HOOK(0x4e8770, CBrowse_Setup);
