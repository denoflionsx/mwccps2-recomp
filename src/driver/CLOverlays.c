/*
 * CLOverlays.c — overlay groups and overlays of the driver's link model.
 *
 * Port of mweppc's src/driver/CLOverlays.c (CC0, github.com/doldecomp/mweppc). The 17 functions in
 * our exe are byte-identical between their GameCube 2.0 compiler and ours
 * (Overlays_AddFileToOverlay is in neither). Struct layouts are mweppc's, natural alignment:
 * Overlay 0x110 bytes, OvlGroup 0x118, Overlays 12. Assertion texts are the parameter names, as
 * the exe's strings show, so the same names are used here.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef struct OvlAddr {
    UInt32 lo, hi;
} OvlAddr;
typedef struct Overlay {
    char name[256];
    SInt32 *list;
    SInt32 cnt;
    SInt32 max;
    struct Overlay *next;
} Overlay;
typedef struct OvlGroup {
    char name[256];
    OvlAddr addr;
    Overlay *olys;
    Overlay *lastoly;
    int olycnt;
    struct OvlGroup *next;
} OvlGroup;
typedef struct Overlays {
    OvlGroup *groups;
    OvlGroup *lastgrp;
    SInt32 grpcnt;
} Overlays;

ORIG_FN_STDCALL(void *, xmalloc, 0x4176a0, (const char *what, int size));
ORIG_FN_STDCALL(void *, xrealloc, 0x417750, (const char *what, void *old, int size));
ORIG_FN_STDCALL(void, xfree, 0x417800, (void *ptr));
ORIG_FN(char *, orig_strncpy, 0x404f20, (char *d, const char *s, UInt32 n));
ORIG_FN(void, CLInternalError, 0x408c80, (const char *file, int line, const char *fmt, ...));
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));

#define OS_ASSERT(line, cond)                                                                      \
    do {                                                                                           \
        if (!(cond))                                                                               \
            assertion_failed(#cond, "CLOverlays.c", line);                                         \
    } while (0)

OvlGroup *OvlGroup_New(const char *name, OvlAddr addr);
Overlay *Overlay_New(const char *name);
Bool8 OvlGroup_AddOverlay(OvlGroup *this, Overlay *oly, SInt32 *olynum);
Bool8 Overlays_AddOvlGroup(Overlays *this, OvlGroup *grp, SInt32 *grpnum);
void OvlGroup_Delete(OvlGroup *grp);
void Overlay_Delete(Overlay *oly);
OvlGroup *Overlays_GetOvlGroup(Overlays *this, SInt32 grpnum);
Overlay *OvlGroup_GetOverlay(OvlGroup *this, SInt32 olynum);
SInt32 Overlay_GetFile(Overlay *oly, SInt32 filnum);

Bool8 Overlays_Initialize(Overlays *this) {
    OvlGroup *grp;
    Overlay *ovl;
    OvlAddr addr;
    SInt32 idx;

    OS_ASSERT(24, this);
    this->groups = 0;
    this->lastgrp = 0;
    this->grpcnt = 0;
    addr.lo = addr.hi = 0;
    grp = OvlGroup_New("main_application", addr);
    if (!grp)
        return 0;
    ovl = Overlay_New("MAIN");
    if (!ovl)
        return 0;
    OvlGroup_AddOverlay(grp, ovl, &idx);
    OS_ASSERT(42, idx==0);
    Overlays_AddOvlGroup(this, grp, &idx);
    OS_ASSERT(45, idx==0);
    return 1;
}
HOOK(0x423fd0, Overlays_Initialize);

Bool8 Overlays_Terminate(Overlays *this) {
    OvlGroup *grp, *nxtgrp;
    OS_ASSERT(54, this);
    for (grp = this->groups; grp; grp = nxtgrp) {
        nxtgrp = grp->next;
        OvlGroup_Delete(grp);
        xfree(grp);
    }
    this->groups = 0;
    return 1;
}
HOOK(0x4240b0, Overlays_Terminate);

Bool8 Overlays_AddOvlGroup(Overlays *this, OvlGroup *grp, SInt32 *grpnum) {
    OS_ASSERT(70, this);
    OS_ASSERT(71, grp);
    if (this->groups == 0)
        this->groups = grp;
    else
        this->lastgrp->next = grp;
    this->lastgrp = grp;
    if (grpnum)
        *grpnum = this->grpcnt;
    this->grpcnt++;
    return 1;
}
HOOK(0x424100, Overlays_AddOvlGroup);

OvlGroup *Overlays_GetOvlGroup(Overlays *this, SInt32 grpnum) {
    OvlGroup *grp;
    SInt32 cnt = 0;
    OS_ASSERT(93, this);
    grp = this->groups;
    while (grp && cnt < grpnum) {
        grp = grp->next;
        cnt++;
    }
    return cnt == grpnum ? grp : 0;
}
HOOK(0x424170, Overlays_GetOvlGroup);

SInt32 Overlays_CountGroups(Overlays *this) {
    OvlGroup *scan;
    int num = 0;
    OS_ASSERT(112, this);
    for (scan = this->groups; scan; scan = scan->next)
        num++;
#ifdef RECOMP_NEGATIVE_CONTROL_CLOVERLAYS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLOVERLAYS */
    num++;
#endif
    return num;
}
HOOK(0x4241c0, Overlays_CountGroups);

Overlay *Overlays_GetOverlayInGroup(Overlays *this, SInt32 grpnum, SInt32 ovlnum) {
    OvlGroup *grp;
    OS_ASSERT(144, this);
    grp = Overlays_GetOvlGroup(this, grpnum);
    return grp ? OvlGroup_GetOverlay(grp, ovlnum) : 0;
}
HOOK(0x424200, Overlays_GetOverlayInGroup);

SInt32 Overlays_GetFileInOverlay(Overlays *this, SInt32 grpnum, SInt32 ovlnum, SInt32 filnum) {
    Overlay *oly;
    OS_ASSERT(160, this);
    oly = Overlays_GetOverlayInGroup(this, grpnum, ovlnum);
    return oly ? Overlay_GetFile(oly, filnum) : -1;
}
HOOK(0x424250, Overlays_GetFileInOverlay);

OvlGroup *OvlGroup_New(const char *name, OvlAddr addr) {
    OvlGroup *grp;
    OS_ASSERT(175, name);
    grp = xmalloc(0, sizeof(OvlGroup));
    if (grp) {
        orig_strncpy(grp->name, name, sizeof(grp->name));
        grp->name[sizeof(grp->name) - 1] = 0;
        grp->addr = addr;
        grp->olys = grp->lastoly = 0;
        grp->olycnt = 0;
        grp->next = 0;
    } else {
        CLInternalError("CLOverlays.c", 188, "Could not allocate %s", "overlay group");
    }
    return grp;
}
HOOK(0x4242a0, OvlGroup_New);

void OvlGroup_Delete(OvlGroup *grp) {
    Overlay *scan, *next;
    OS_ASSERT(197, grp);
    for (scan = grp->olys; scan; scan = next) {
        next = scan->next;
        Overlay_Delete(scan);
        xfree(scan);
    }
    grp->olys = 0;
}
HOOK(0x424360, OvlGroup_Delete);

Bool8 OvlGroup_AddOverlay(OvlGroup *this, Overlay *oly, SInt32 *olynum) {
    OS_ASSERT(211, this);
    OS_ASSERT(212, oly);
    if (!this->lastoly)
        this->olys = oly;
    else
        this->lastoly->next = oly;
    this->lastoly = oly;
    if (olynum)
        *olynum = this->olycnt;
    this->olycnt++;
    return 1;
}
HOOK(0x4243c0, OvlGroup_AddOverlay);

Overlay *OvlGroup_GetOverlay(OvlGroup *this, SInt32 olynum) {
    Overlay *oly;
    SInt32 cnt = 0;
    OS_ASSERT(234, this);
    oly = this->olys;
    while (oly && cnt < olynum) {
        oly = oly->next;
        cnt++;
    }
    return cnt == olynum ? oly : 0;
}
HOOK(0x424440, OvlGroup_GetOverlay);

SInt32 OvlGroup_CountOverlays(OvlGroup *this) {
    Overlay *scan;
    int num = 0;
    OS_ASSERT(254, this);
    for (scan = this->olys; scan; scan = scan->next)
        num++;
    return num;
}
HOOK(0x424490, OvlGroup_CountOverlays);

Overlay *Overlay_New(const char *name) {
    Overlay *oly = xmalloc(0, sizeof(Overlay));
    if (oly) {
        orig_strncpy(oly->name, name, sizeof(oly->name));
        oly->name[sizeof(oly->name) - 1] = 0;
        oly->list = 0;
        oly->cnt = oly->max = 0;
        oly->next = 0;
    } else {
        CLInternalError("CLOverlays.c", 281, "Could not allocate %s", "overlay");
    }
    return oly;
}
HOOK(0x4244d0, Overlay_New);

void Overlay_Delete(Overlay *oly) {
    OS_ASSERT(288, oly);
    if (oly->list)
        xfree(oly->list);
    oly->list = 0;
}
HOOK(0x424550, Overlay_Delete);

Bool8 Overlay_AddFile(Overlay *oly, SInt32 filenum, SInt32 *filnum) {
    OS_ASSERT(296, oly);
    if (oly->cnt >= oly->max) {
        oly->max += 16;
        oly->list = xrealloc("overlay file list", oly->list, sizeof(SInt32) * oly->max);
    }
    oly->list[oly->cnt] = filenum;
    if (filnum)
        *filnum = oly->cnt;
    oly->cnt++;
    return 1;
}
HOOK(0x424590, Overlay_AddFile);

SInt32 Overlay_GetFile(Overlay *oly, SInt32 filnum) {
    OS_ASSERT(314, oly);
    return filnum < oly->cnt ? oly->list[filnum] : -1;
}
HOOK(0x424620, Overlay_GetFile);

SInt32 Overlay_CountFiles(Overlay *oly) {
    OS_ASSERT(323, oly);
    return oly->cnt;
}
HOOK(0x424670, Overlay_CountFiles);
