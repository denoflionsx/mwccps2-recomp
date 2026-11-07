/*
 * ResourceStrings.c — the driver's table of string resources ('STR#' lists).
 *
 * Port of mweppc's src/driver/ResourceStrings.c (CC0, github.com/doldecomp/mweppc). Both functions
 * are byte-identical between their GameCube 2.0 compiler and ours. The table (`rlist`, 16 entries
 * of 12 bytes) is at 0x54a8e8 and the error buffer (`err`, 256 bytes) at 0x54a7e8, both in the exe.
 */
#include "orig.h"

typedef struct Res { /* 12 bytes */
    const char *name;
    SInt16 rsrcid;
    const char **strings;
} Res;

#define rlist ((Res *)0x54a8e8)
#define err_buf ((char *)0x54a7e8)
#define orig_stderr ((void *)0x510798)

ORIG_FN(int, orig_fprintf, 0x403bd0, (void *file, const char *fmt, ...));
ORIG_FN(int, orig_snprintf, 0x403ce0, (char *buf, UInt32 n, const char *fmt, ...));

int Res_AddResource(const char *name, SInt16 rsrcid, const char **strings) {
    int scan;
    for (scan = 0; scan < 16 && rlist[scan].rsrcid; scan++) {
        if (rsrcid == rlist[scan].rsrcid) {
            orig_fprintf(orig_stderr, "Resource %d is already added!\n", rsrcid);
            return 0;
        }
    }
    if (scan >= 16)
        return 0;
    rlist[scan].name = name;
    rlist[scan].rsrcid = rsrcid;
    rlist[scan].strings = strings;
    return 1;
}
HOOK(0x408d80, Res_AddResource);

const char *Res_GetResource(SInt16 rsrcid, SInt16 index) {
    int scan, find;
    index--;
    for (scan = 0; scan < 16; scan++) {
        if (rsrcid == rlist[scan].rsrcid) {
            find = 0;
            if (index < 0) {
                orig_snprintf(err_buf, 256, "[Illegal string index #%d in list '%s' (%d)]", index, rlist[scan].name,
                              rsrcid);
                return err_buf;
            }
            while (find <= index) {
                if (!rlist[scan].strings[find]) {
                    orig_snprintf(err_buf, 256, "[String #%d not found in resource '%s' (%d)]", index + 1,
                                  rlist[scan].name, rsrcid);
                    return err_buf;
                }
                if (find == index)
#ifdef RECOMP_NEGATIVE_CONTROL_RESOURCESTRINGS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_RESOURCESTRINGS */
                    return rlist[scan].strings[find ? find - 1 : find];
#else
                    return rlist[scan].strings[find];
#endif
                find++;
            }
        }
    }
    return 0;
}
HOOK(0x408e00, Res_GetResource);
