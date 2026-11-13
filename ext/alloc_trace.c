/*
 * alloc_trace.c — MWRECOMP_ALLOC_TRACE: what the compiler's heaps hand out, on stderr.
 *
 * Where a block lands decides what stale bytes it starts with and which pointer values get left
 * behind in it, and both reach the generated code (docs/ENTROPY.md). When two runs that should
 * agree do not, the first line that differs between their traces is where they parted.
 *
 *   MWRECOMP_ALLOC_TRACE=1   one line per galloc, oalloc and aalloc call and per new heap block
 *                            (MoreHeapSpace): kind, size, address, caller; with each, how many
 *                            lalloc calls there were so far and a checksum of their addresses
 *   MWRECOMP_ALLOC_TRACE=2   lalloc calls too
 *
 * Not an extension (MWRECOMP_EXT does not matter): the wraps change nothing when the variable is
 * not set, and the trace itself is written from the handlers' stack.
 */
#include "ext.h"

static int level = -1;
static UInt32 lallocs, lsum;

static int tracing(void) {
    char text[4];

    if (level < 0)
        level = Rt_GetEnv("MWRECOMP_ALLOC_TRACE", text, sizeof text) ? text[0] - '0' : 0;
    return level;
}

static void line(const char *kind, UInt32 size, UInt32 result, UInt32 caller) {
    Rt_Say("mwrecomp: ");
    Rt_Say(kind);
    Rt_Say(" ");
    Rt_SayHex(size);
    Rt_Say(" -> ");
    Rt_SayHex(result);
    Rt_Say(" from ");
    Rt_SayHex(caller);
    Rt_Say("  lalloc ");
    Rt_SayDec(lallocs);
    Rt_Say(" ");
    Rt_SayHex(lsum);
    Rt_Say("\n");
}

/* stale_hascall.c owns the lalloc wrap and reports each call here */
void AllocTrace_Lalloc(UInt32 size, UInt32 result, UInt32 caller) {
    if (!tracing())
        return;
    lallocs++;
    lsum = (lsum << 1 | lsum >> 31) ^ result;
    if (level >= 2)
        line("lalloc", size, result, caller);
}

#define TRACED(Name, text, addr, moved)                          \
    static UInt32 Name##_size;                                   \
    extern UInt32 Name##_ret;                                    \
    int Name##_Before(UInt32 *args) {                            \
        Name##_size = args[0];                                   \
        return 0;                                                \
    }                                                            \
    UInt32 Name##_After(UInt32 *args, UInt32 result) {           \
        if (tracing())                                           \
            line(text, Name##_size, result, Name##_ret);         \
        return result;                                           \
    }                                                            \
    HOOK_WRAP(Name, addr, moved, -1, Name##_Before, Name##_After)

TRACED(Galloc, "galloc", 0x4332e0, 5);
TRACED(Oalloc, "oalloc", 0x4333a0, 5);
TRACED(Aalloc, "aalloc", 0x433360, 5);

/* MoreHeapSpace(heap, size): the heap's address shows which one grew */
static UInt32 more_heap, more_size;
extern UInt32 MoreHeap_ret;

int MoreHeap_Before(UInt32 *args) {
    more_heap = args[0];
    more_size = args[1];
    return 0;
}

UInt32 MoreHeap_After(UInt32 *args, UInt32 result) {
    if (tracing()) {
        Rt_Say("mwrecomp: heap ");
        Rt_SayHex(more_heap);
        line(" grows", more_size, result, MoreHeap_ret);
    }
    return result;
}
HOOK_WRAP(MoreHeap, 0x433070, 7, -1, MoreHeap_Before, MoreHeap_After);
