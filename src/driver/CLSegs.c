/*
 * CLSegs.c — code segment list of the command-line driver.
 *
 * Port of mweppc's src/driver/CLSegs.c (CC0, github.com/doldecomp/mweppc). All 8 functions are
 * byte-identical between their GameCube 2.0 compiler and ours (docs/data/gcmatch_GC_2_0.json);
 * addresses from docs/data/addrmap_GC_2_0.json. Segments_InsertSegment and
 * Segments_DeleteSegment are in the source but in neither binary (never called), so not here.
 *
 * Return types: the original returns Boolean in al and UInt16 in ax; callers test al or extend
 * ax themselves (checked at every call site), so clang's partial-register returns are compatible.
 */
#include "orig.h"

typedef UInt8 Bool8; /* the driver's Boolean: one byte (orig.h's Boolean is an int) */
#ifndef NULL
#define NULL ((void *)0) /* spelled out: OS_ASSERT prints "segs != NULL" */
#endif

typedef struct Segment {
    char name[32];
    UInt16 attrs;
} Segment; /* 34 bytes */

typedef struct Segments {
    Segment **segsArray;
    UInt16 arraySize;
    UInt16 segsCount;
} Segments; /* 8 bytes */

ORIG_FN_STDCALL(void *, xmalloc, 0x4176a0, (const char *what, int size));
ORIG_FN_STDCALL(void *, xrealloc, 0x417750, (const char *what, void *old, int size));
ORIG_FN_STDCALL(void, xfree, 0x417800, (void *ptr));
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));
ORIG_FN(char *, orig_strncpy, 0x404f20, (char *dst, const char *src, UInt32 n));
ORIG_FN(void *, orig_memset, 0x404d40, (void *dst, int c, UInt32 n));

/* the original's __FILE__ */
#define OS_ASSERT(line, cond)                                                                      \
    do {                                                                                           \
        if (!(cond))                                                                               \
            assertion_failed(#cond, "CLSegs.c", line);                                             \
    } while (0)

Segment *Segment_New(const char *name, UInt16 attrs) {
    Segment *seg = xmalloc(0, sizeof(Segment));
    orig_strncpy(seg->name, name, sizeof(seg->name));
    seg->name[sizeof(seg->name) - 1] = 0;
    seg->attrs = attrs;
    return seg;
}
HOOK(0x428c30, Segment_New);

void Segment_Free(Segment *seg) {
    if (seg)
        xfree(seg);
}
HOOK(0x428c60, Segment_Free);

Bool8 Segments_AddSegment(Segments *segs, Segment *seg, UInt16 *index);

Bool8 Segments_Initialize(Segments *segs) {
    Segment *jump, *main;
    UInt16 idx;

    OS_ASSERT(36, segs != NULL);
    orig_memset(segs, 0, sizeof(Segments));
    segs->segsArray = 0;

    jump = Segment_New("Jump Table", 0x28);
    Segments_AddSegment(segs, jump, &idx);
    OS_ASSERT(44, idx==0);

    main = Segment_New("Main", 0xFFFF);
    Segments_AddSegment(segs, main, &idx);
    OS_ASSERT(49, idx==1);
    return 1;
}
HOOK(0x428c70, Segments_Initialize);

Bool8 Segments_Terminate(Segments *segs) {
    UInt16 index;

    OS_ASSERT(57, segs != NULL);
    if (segs->segsArray) {
        for (index = 0; index < segs->segsCount; index++)
            Segment_Free(segs->segsArray[index]);
        xfree(segs->segsArray);
    }
    segs->segsArray = 0;
    return 1;
}
HOOK(0x428d20, Segments_Terminate);

/* static in the original, but a function of its own at 0x428d70 */
Bool8 Segments_GrowSegments(Segments *segs, UInt16 *index) {
    OS_ASSERT(78, segs != NULL);
    if (segs->segsCount >= segs->arraySize) {
        segs->arraySize += 20;
        segs->segsArray = xrealloc("segments", segs->segsArray, sizeof(Segment *) * segs->arraySize);
    }
    *index = segs->segsCount++;
    return 1;
}
HOOK(0x428d70, Segments_GrowSegments);

Bool8 Segments_AddSegment(Segments *segs, Segment *seg, UInt16 *index) {
    UInt16 ni;
    if (Segments_GrowSegments(segs, &ni)) {
        segs->segsArray[ni] = seg;
        *index = ni;
        return 1;
    }
    return 0;
}
HOOK(0x428dd0, Segments_AddSegment);

Segment *Segments_GetSegment(Segments *segs, UInt16 segnum) {
    OS_ASSERT(137, segs != NULL);
    if (segnum < segs->segsCount)
        return segs->segsArray[segnum];
    return 0;
}
HOOK(0x428e20, Segments_GetSegment);

UInt16 Segments_Count(const Segments *segs) {
    OS_ASSERT(147, segs != NULL);
#ifdef RECOMP_NEGATIVE_CONTROL_CLSEGS /* build.py --cflags=-DRECOMP_NEGATIVE_CONTROL_CLSEGS */
    return segs->segsCount + 1;
#endif
    return segs->segsCount;
}
HOOK(0x428e60, Segments_Count);
