/*
 * CError.c — the frontend's error state and error/warning entry points
 * (mweppc src/compiler/frontend/CError.c), partial port.
 *
 * 13 of its functions are in our exe, all byte-identical between mweppc's GameCube 2.0 compiler
 * and ours. Ported: the 8 the corpus runs. CError_Error and CError_Warning pass their arguments
 * on as a va_list (a plain pointer on this target) to CError_VAErrorMessage (0x456cf0, not in
 * the map); CError_Error first longjmps to an active try-block (trychain->jmpbuf, +4).
 * CError_Internal ends in a longjmp to errorreturn and never returns.
 * TStreamElement.tokenfile is at +4.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef char *va_list_t;

ORIG_FN(void, CError_VAErrorMessage, 0x456cf0, (int code, va_list_t va, Bool8 flag1, Bool8 is_warning));
ORIG_FN(void, CError_ErrorMessage, 0x456810, (int code, char *msg, Bool8 flag1, Bool8 is_warning));
ORIG_FN(void, CError_BreakPoint, 0x457210, (const char *a, const char *b));
ORIG_FN(void, AssemblerError, 0x4f92e0, (void));
ORIG_FN(void, CompilerGetCString, 0x432a20, (SInt16 index, char *string));
ORIG_FN(int, orig_sprintf, 0x403d30, (char *buf, const char *fmt, ...));
ORIG_FN(void, orig_longjmp, 0x41cb80, (void *jmpbuf, int val));

#define cerror_lasterrorline ORIG_VAR(SInt32, 0x54d638)
#define cerror_errorcount ORIG_VAR(SInt16, 0x54d63c)
#define cerror_token ORIG_VAR(UInt8 *, 0x54d63e)
#define cerror_locktoken ORIG_VAR(UInt8 *, 0x57c7e4)
#define trychain ORIG_VAR(UInt8 *, 0x57d6b8)
#define supress_warnings ORIG_VAR(Bool8, 0x57dbc0) /* copts */
#define in_assembler ORIG_VAR(Bool8, 0x57e03e)
#define preprocessing_only ORIG_VAR(Bool8, 0x57e13b)
#define string_buf ((char *)0x56e758)
#define errorreturn ((void *)0x56bf98)
#define TOKENFILE(t) (*(void **)((t) + 4))

void CError_Init(void) {
    cerror_errorcount = 0;
    cerror_lasterrorline = -1;
    cerror_token = 0;
    cerror_locktoken = 0;
}
HOOK(0x455780, CError_Init);

void CError_SetErrorToken(UInt8 *token) {
    if (token && TOKENFILE(token))
        cerror_token = token;
}
HOOK(0x4557b0, CError_SetErrorToken);

void CError_LockErrorPos(UInt8 *token, UInt8 **saved) {
    *saved = cerror_locktoken;
    if (token && TOKENFILE(token))
        cerror_locktoken = token;
}
HOOK(0x4557d0, CError_LockErrorPos);

void CError_UnlockErrorPos(UInt8 **saved) {
    cerror_locktoken = *saved;
}
HOOK(0x4557f0, CError_UnlockErrorPos);

void CError_ResetErrorSkip(void) {
    cerror_lasterrorline = -1;
}
HOOK(0x455800, CError_ResetErrorSkip);

void CError_Error(int code, ...) {
    __builtin_va_list va;
    if (trychain)
        orig_longjmp(trychain + 4, 1);
    __builtin_va_start(va, code);
    CError_VAErrorMessage(code, (va_list_t)va, 0, 0);
    __builtin_va_end(va);
    if (in_assembler && !preprocessing_only)
        AssemblerError();
}
HOOK(0x456d50, CError_Error);

void CError_Warning(int code, ...) {
    __builtin_va_list va;
#ifdef RECOMP_NEGATIVE_CONTROL_CERROR /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CERROR */
    return; /* no warnings */
#endif
    if (trychain || supress_warnings)
        return;
    __builtin_va_start(va, code);
    CError_VAErrorMessage(code, (va_list_t)va, 0, 1);
    __builtin_va_end(va);
}
HOOK(0x4571c0, CError_Warning);

void CError_Internal(char *filename, int line) {
    char format[128];
    CompilerGetCString(5, format);
    orig_sprintf(string_buf, format, filename, line);
    CError_ErrorMessage(10001, string_buf, 1, 0);
    orig_longjmp(errorreturn, 1);
    CError_BreakPoint(0, 0);
}
HOOK(0x457230, CError_Internal);
