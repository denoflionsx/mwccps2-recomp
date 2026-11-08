/*
 * FunctionCalls.c — call generation of the back end (mweppc src/compiler/FunctionCalls.c),
 * partial port.
 *
 * Ported so far (from the exe): the test for the two special call targets (0x4a4540: the objects
 * at 0x57ab74 and 0x57ab78).
 */
#include "orig.h"

int fn_4a4540(void *obj) {
#ifdef RECOMP_NEGATIVE_CONTROL_FUNCTIONCALLS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_FUNCTIONCALLS */
    if (obj == *(void **)0x57ab74 || obj == *(void **)0x57ab78) /* marks runs that find a target */
        ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "FUNCTIONCALLS\n");
#endif
    return obj == *(void **)0x57ab74 || obj == *(void **)0x57ab78;
}
HOOK(0x4a4540, fn_4a4540);

/* generates an argument's expression (+4; its node type picks the generator from the table
   0x53c638) into the argument's operand (+8), unless it is already done (+0x3a) */
typedef SInt32 (*GenFunc)(void *expr, void *target, void *operand);

/* returns the generator's result, or the expression's type when it is skipped (as the original
   leaves it in eax) */
SInt32 fn_4a4420(UInt8 *arg) {
    UInt8 *expr = *(UInt8 **)(arg + 4);

    if (*(UInt16 *)(arg + 0x3a) == 0)
        return ((GenFunc *)0x53c638)[expr[0]](expr, 0, arg + 8);
    return *(SInt32 *)(expr + 0xa);
}
HOOK(0x4a4420, fn_4a4420);
