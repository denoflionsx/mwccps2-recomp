/*
 * CLErrors.c — the driver's error reporting (mweppc src/driver/CLErrors.c), whole unit.
 *
 * Messages come from string table 12000 (getindstring). CLMessageReporter formats into the exe's
 * own static buffers (stmsg 0x54a6e8, diagnostic_message_buffer 0x54a5e8) and sends the result
 * through CLStyledMessageDispatch with format "%". Message types: Msg_Warning 2, Msg_Error 3,
 * Msg_Status 5; messagetypeInfo 0, Warning 1, Error 2.
 */
#include "orig.h"

#define S(addr) ((const char *)(addr))

ORIG_FN_STDCALL(char *, getindstring, 0x415c30, (char *buffer, SInt16 resID, SInt16 index));
ORIG_FN(char *, orig_strcat, 0x404f50, (char *d, const char *s));
ORIG_FN(char *, mvprintf, 0x40f460, (char *buf, UInt32 len, const char *format, char **va));
ORIG_FN(SInt16, CLStyledMessageDispatch, 0x417370, (void *plugin, void *ref, SInt32 errorNumber, SInt16 msgType, const char *format, ...));
ORIG_FN(void, orig_free, 0x404730, (void *p));
ORIG_FN_STDCALL(const char *, OS_GetErrText, 0x411d10, (int err));
ORIG_FN(char *, orig_strerror, 0x405140, (int err));
ORIG_FN(void, CLPrintErr, 0x417230, (const char *format, ...));
ORIG_FN(void, orig_exit, 0x402180, (int code));

#define stmsg ((char *)0x54a6e8)
#define stbuf ((char *)0x54a5e8) /* diagnostic_message_buffer */

static char *CLGetErrorString(SInt16 errid, char *buffer) {
    getindstring(buffer, 12000, errid);
    orig_strcat(buffer, S(0x51d980) /* "\n" */);
    return buffer;
}
HOOK(0x408a10, CLGetErrorString);

static void CLMessageReporter(int flags, SInt16 errid, char **va) {
    char *ptr;

    CLGetErrorString(errid, stmsg);
#ifdef RECOMP_NEGATIVE_CONTROL_CLERRORS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLERRORS */
    stmsg[0] = '<';
#endif
    ptr = mvprintf(stbuf, 256, stmsg, va);
    CLStyledMessageDispatch(0, 0, 0, flags == 2 ? 3 : flags == 1 ? 2 : 5, S(0x51d984) /* "%" */, ptr);
    if (ptr != stbuf)
        orig_free(ptr);
}
HOOK(0x408a40, CLMessageReporter);

void CLReportError(int errid, ...) {
    __builtin_va_list va;
    __builtin_va_start(va, errid);
    CLMessageReporter(2, (SInt16)errid, (char **)va);
    __builtin_va_end(va);
}
HOOK(0x408ac0, CLReportError);

void CLReportWarning(int errid, ...) {
    __builtin_va_list va;
    __builtin_va_start(va, errid);
    CLMessageReporter(1, (SInt16)errid, (char **)va);
    __builtin_va_end(va);
}
HOOK(0x408af0, CLReportWarning);

void CLReport(int errid, ...) {
    __builtin_va_list va;
    __builtin_va_start(va, errid);
    CLMessageReporter(0, (SInt16)errid, (char **)va);
    __builtin_va_end(va);
}
HOOK(0x408b20, CLReport);

void CLReportOSError(int errid, int err, ...) {
    char mybuf[256], myerr[256], *txt;
    const char *oserr;
    __builtin_va_list va;

    __builtin_va_start(va, err);
    txt = mvprintf(mybuf, sizeof(mybuf), CLGetErrorString((SInt16)errid, myerr), (char **)va);
    __builtin_va_end(va);
    oserr = OS_GetErrText(err);
    CLReportError(99, txt, oserr, err);
    if (txt != mybuf)
        orig_free(txt);
}
HOOK(0x408b50, CLReportOSError);

void CLReportCError(int errid, int err_no, ...) {
    char mybuf[256], myerr[256], *txt;
    const char *serr;
    __builtin_va_list va;

    __builtin_va_start(va, err_no);
    txt = mvprintf(mybuf, sizeof(mybuf), CLGetErrorString((SInt16)errid, myerr), (char **)va);
    __builtin_va_end(va);
    serr = orig_strerror(err_no);
    CLReportError(100, txt, serr, err_no);
    if (txt != mybuf)
        orig_free(txt);
}
HOOK(0x408be0, CLReportCError);

void CLInternalError(const char *file, int line, const char *format, ...) {
    char mybuf[256], *txt;
    __builtin_va_list va;

    __builtin_va_start(va, format);
    txt = mvprintf(mybuf, sizeof(mybuf), format, (char **)va);
    __builtin_va_end(va);
    CLPrintErr(S(0x51d988) /* "INTERNAL ERROR [%s:%d]:\n%s\n" */, file, line, txt);
    if (txt != mybuf)
        orig_free(txt);
}
HOOK(0x408c80, CLInternalError);

void CLFatalError(const char *format, ...) {
    char mybuf[256], *txt;
    __builtin_va_list va;

    __builtin_va_start(va, format);
    txt = mvprintf(mybuf, sizeof(mybuf), format, (char **)va);
    __builtin_va_end(va);
    CLPrintErr(S(0x51d9a4) /* "FATAL ERROR:\n%s\n" */, txt);
    if (txt != mybuf)
        orig_free(txt);
    orig_exit(-123);
}
HOOK(0x408d00, CLFatalError);
