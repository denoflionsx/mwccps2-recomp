/*
 * CWSecretPluginCallbacks.c — the plugin API's "secret" handle callbacks (mweppc
 * src/driver/CWSecretPluginCallbacks.cpp, a C++ file with extern "C" functions).
 *
 * All 4 functions are byte-identical between mweppc's GameCube 2.0 compiler and ours. The three
 * callbacks are __stdcall and call through the context's callback table (also __stdcall). Layout
 * from our code: context->shellSignature +0x9c ('CWIE'), context->callbacks +0x104,
 * callbacks->cbInternal[0] +0x68, [1] +0x6c.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef int CWResult;
typedef UInt8 *CWPluginContext;
typedef CWResult(__stdcall *Internal)(CWPluginContext, void *, void *);
#define cwErrInvalidParameter 3
#define CTX_signature(c) (*(UInt32 *)((c) + 0x9c))
#define CTX_callbacks(c) (*(UInt8 **)((c) + 0x104))
#define CB_internal(cb, i) (*(Internal *)((cb) + 0x68 + 4 * (i)))

ORIG_FN_STDCALL(CWResult, CWGetNamedPreferences, 0x41fa10, (CWPluginContext c, const char *name, void **mh));

__attribute__((noinline)) Bool8 CWSecretPluginCallbacks_ValidateContext(CWPluginContext context) {
    return context && CTX_signature(context) == 0x43574945; /* 'CWIE' */
}
HOOK(0x41cba0, CWSecretPluginCallbacks_ValidateContext);

CWResult __stdcall CWSecretAttachHandle(CWPluginContext context, void *handle, void *memHandle) {
    if (!CWSecretPluginCallbacks_ValidateContext(context))
        return cwErrInvalidParameter;
    return CB_internal(CTX_callbacks(context), 0)(context, handle, memHandle);
}
HOOK(0x41cbc0, CWSecretAttachHandle);

CWResult __stdcall CWSecretDetachHandle(CWPluginContext context, void *memHandle, void *handle) {
    if (!CWSecretPluginCallbacks_ValidateContext(context))
        return cwErrInvalidParameter;
    return CB_internal(CTX_callbacks(context), 1)(context, memHandle, handle);
}
HOOK(0x41cc00, CWSecretDetachHandle);

CWResult __stdcall CWSecretGetNamedPreferences(CWPluginContext context, const char *prefsname, void **prefsdata) {
    void *memHandle;
    CWResult res;
    if (!prefsdata)
        return cwErrInvalidParameter;
#ifdef RECOMP_NEGATIVE_CONTROL_CWSECRET /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CWSECRET */
    return cwErrInvalidParameter;
#endif
    res = CWGetNamedPreferences(context, prefsname, &memHandle);
    if (!res)
        res = CWSecretDetachHandle(context, memHandle, prefsdata);
    return res;
}
HOOK(0x41cc40, CWSecretGetNamedPreferences);
