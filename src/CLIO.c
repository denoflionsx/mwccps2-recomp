/*
 * CLIO.c — the driver's console and file output (mweppc src/driver/CLIO.c), partial port.
 *
 * Its functions in our exe are byte-identical between mweppc's GameCube 2.0 compiler and ours.
 * Ported: console setup, IO_Initialize/IO_Terminate, writing handles to stdout or files, the
 * "working" stubs and CheckForUserBreak (part A); the message text formatter and CLPrintDispatch
 * (part B); the default message style, its helpers and CLStyledMessageDispatch (part C). WrapText,
 * the other message styles (MPW, terse, IDE, parseable), help output and CLPrint/CLPrintErr are
 * never run by the corpus and stay the original's.
 *
 * MSL from the exe: stdout FILE 0x510744, stderr 0x510798, errno at _GetThreadLocalData()+4,
 * _HandleTable 0x56c8c0 (translate flag +4). optsEnvir.cols (short) 0x56acea, rows 0x56acec.
 * clState.userBreak 0x57b361. WriteHandleToFile/AppendHandleToFile give OS_SpecToString a size of
 * 0x104 for a 256-byte buffer in the original; ours is 0x104 bytes so a long path cannot run past
 * it (their frames are 4 bytes larger; they run after the compile).
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef void FILE;
typedef char **Handle;
typedef UInt32 DWORD;
#define IMP(type, addr) (*(type *)(addr))
typedef void *(__stdcall *GetStdHandle_t)(DWORD);
typedef int(__stdcall *SetConsoleCtrlHandler_t)(void *, int);
typedef int(__stdcall *GetConsoleScreenBufferInfo_t)(void *, void *);
#define GetStdHandle IMP(GetStdHandle_t, 0x547164)
#define SetConsoleCtrlHandler IMP(SetConsoleCtrlHandler_t, 0x547238)
#define GetConsoleScreenBufferInfo IMP(GetConsoleScreenBufferInfo_t, 0x54723c)

ORIG_FN(int, orig_setvbuf, 0x40c7a0, (FILE *f, char *buf, int mode, UInt32 size));
ORIG_FN(UInt32, orig_fwrite, 0x406290, (const void *p, UInt32 size, UInt32 n, FILE *f));
ORIG_FN(int, orig_fflush, 0x40c980, (FILE *f));
ORIG_FN(FILE *, orig_fopen, 0x40ca70, (const char *name, const char *mode));
ORIG_FN(int, orig_fclose, 0x40c8f0, (FILE *f));
ORIG_FN(int, orig_fprintf, 0x403bd0, (FILE *f, const char *fmt, ...));
ORIG_FN(UInt8 *, GetThreadLocalData, 0x4020a0, (void));
ORIG_FN_STDCALL(void, HLock, 0x414940, (Handle h));
ORIG_FN_STDCALL(void, HUnlock, 0x414990, (Handle h));
ORIG_FN(void, CLReportCError, 0x408be0, (int id, int err, ...));
ORIG_FN(void, CLReportOSError, 0x408b50, (int id, int err, ...));
ORIG_FN_STDCALL(char *, OS_SpecToString, 0x412fc0, (const void *spec, char *buf, int size));
ORIG_FN_STDCALL(int, OS_SetMacFileCreatorAndType, 0x423020, (const void *spec, UInt32 creator, UInt32 type));
ORIG_FN_STDCALL(int, OS_NewFileHandle, 0x421ae0, (const void *spec, void *text, Bool8 writeable, void *file));
ORIG_FN_STDCALL(int, OS_FreeFileHandle, 0x421bd0, (void *file));
ORIG_FN(Bool8, IO_HelpTerminate, 0x415dd0, (void));

#define orig_stdout ((FILE *)0x510744)
#define orig_stderr ((FILE *)0x510798)
#define orig_errno (*(int *)(GetThreadLocalData() + 4))
#define HandleTable ((UInt8 **)0x56c8c0)
#define ioPiping ORIG_VAR(Bool8, 0x54be09)
#define ioInHelp ORIG_VAR(Bool8, 0x54be08)
#define ioLineNum ORIG_VAR(int, 0x54b8f0)
#define envir_cols ORIG_VAR(SInt16, 0x56acea)
#define envir_rows ORIG_VAR(SInt16, 0x56acec)
#define userBreak ORIG_VAR(Bool8, 0x57b361)
#define catchinterrupt ((void *)0x415c50) /* the original's console Ctrl handler, kept */
#define border ORIG_VAR(const char *, 0x52d08c)
#define STSbuf ((char *)0x57cd9c)
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2 /* MSL's values: setvbuf modes as the original passes them */

Bool8 CheckForUserBreak(void);

void SetupConsoleInfo(void) {
    void *console;
    SInt16 info[11]; /* CONSOLE_SCREEN_BUFFER_INFO; dwSize.X, dwSize.Y first */

    console = GetStdHandle((DWORD)-11); /* STD_OUTPUT_HANDLE */
    SetConsoleCtrlHandler(catchinterrupt, 1);
    ioPiping = 0;
    if (!GetConsoleScreenBufferInfo(console, info)) {
        envir_rows = 0;
        envir_cols = 80;
    } else {
        envir_rows = info[1];
        envir_cols = info[0];
        if (envir_cols > 256)
            envir_cols = 256;
    }
    HandleTable[0][4] = 0; /* translate */
    HandleTable[1][4] = 0;
    HandleTable[2][4] = 0;
}
HOOK(0x415c80, SetupConsoleInfo);

void InitWorking(void) {
}
HOOK(0x416150, InitWorking);

void ShowWorking(int x) {
    (void)x;
}
HOOK(0x416160, ShowWorking);

void TermWorking(void) {
}
HOOK(0x416170, TermWorking);

Bool8 CheckForUserBreak(void) {
    ShowWorking(4);
    return userBreak;
}
HOOK(0x416180, CheckForUserBreak);

Bool8 IO_Initialize(void) {
    ioPiping = 0;
    ioInHelp = 0;
    ioLineNum = 0;
    SetupConsoleInfo();
    /* MSL modes as passed by the original: piping -> 2, else 1 */
    if (ioPiping) {
        orig_setvbuf(orig_stdout, 0, 2, 0x1000);
        orig_setvbuf(orig_stderr, 0, 2, 0x1000);
    } else {
        orig_setvbuf(orig_stdout, 0, 1, 0x1000);
        orig_setvbuf(orig_stderr, 0, 1, 0x1000);
    }
    InitWorking();
    return 1;
}
HOOK(0x415d30, IO_Initialize);

Bool8 IO_Terminate(void) {
    if (ioInHelp)
        IO_HelpTerminate();
    TermWorking();
    return 1;
}
HOOK(0x415db0, IO_Terminate);

Bool8 SendHandleToFile(FILE *file, Handle text, SInt32 size) {
    Bool8 ret;
    char *ptr, *lineEnd, *end;

    if (file != orig_stdout && file != orig_stderr)
        orig_setvbuf(file, 0, 2, 0x1000);
    HLock(text);
    ptr = *text;
    if (size && ptr[size - 1] == 0)
        size--;
    if (size <= 0) {
        ret = 1;
    } else {
        end = *text + size;
        while (ptr < end) {
            lineEnd = ptr;
            while (lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r')
                lineEnd++;
            if (lineEnd - ptr) {
                if (orig_fwrite(ptr, lineEnd - ptr, 1, file) != 1) {
                    CLReportCError(10, orig_errno);
                    ret = 0;
                    goto fail;
                }
            }
            if (CheckForUserBreak()) {
                ret = 1;
                goto fail;
            }
#ifdef RECOMP_NEGATIVE_CONTROL_CLIO /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLIO */
            if (orig_fwrite("\n", 1, 1, file) != 1) {
#else
            if (orig_fwrite("\r\n", 2, 1, file) != 1) {
#endif
                CLReportCError(10, orig_errno);
                ret = 0;
                goto fail;
            }
            if (lineEnd < end && *(lineEnd++) == '\r' && *lineEnd == '\n')
                ++lineEnd;
            ptr = lineEnd;
        }
        ret = 1;
    }
fail:
    HUnlock(text);
    orig_fflush(file);
    if (file != orig_stdout && file != orig_stderr)
        orig_setvbuf(file, 0, 0, 0x1000);
    return ret;
}
HOOK(0x415de0, SendHandleToFile);

Bool8 ShowHandle(Handle text, UInt32 size, Bool8 decorate) {
    if (decorate)
        orig_fprintf(orig_stdout, border);
    if (!SendHandleToFile(orig_stdout, text, size))
        return 0;
    if (decorate)
        orig_fprintf(orig_stdout, border);
    orig_fflush(orig_stdout);
    return 1;
}
HOOK(0x415f20, ShowHandle);

static Bool8 handle_to_named_file(const void *spec, Handle text, UInt32 size, UInt32 creator, UInt32 type,
                                  const char *mode) {
    FILE *file;
    char path[0x104];
    OS_SpecToString(spec, path, 0x104);
    file = orig_fopen(path, mode);
    if (!file) {
        CLReportCError(8, orig_errno, path);
        return 0;
    }
    SendHandleToFile(file, text, size);
    orig_fclose(file);
    OS_SetMacFileCreatorAndType(spec, creator, type);
    return 1;
}

Bool8 WriteHandleToFile(const void *spec, Handle text, UInt32 size, UInt32 creator, UInt32 type) {
    return handle_to_named_file(spec, text, size, creator, type, "w+b");
}
HOOK(0x415f80, WriteHandleToFile);

Bool8 AppendHandleToFile(const void *spec, Handle text, UInt32 size, UInt32 creator, UInt32 type) {
    return handle_to_named_file(spec, text, size, creator, type, "a+b");
}
HOOK(0x4160b0, AppendHandleToFile);

Bool8 WriteBinaryHandleToFile(const void *spec, UInt32 maccreator, UInt32 mactype, void *text) {
    int err;
    UInt8 file[0x210]; /* OSFileHandle */
    if ((err = OS_NewFileHandle(spec, text, 1, file)) || (err = OS_FreeFileHandle(file)) ||
        (err = OS_SetMacFileCreatorAndType(spec, maccreator, mactype))) {
        CLReportOSError(8, err, OS_SpecToString(spec, STSbuf, 0x104));
        return 0;
    }
    return 1;
}
HOOK(0x416020, WriteBinaryHandleToFile);

/* ---- part B: message text formatting and printing ---- */

typedef struct TextBuffer {
    char *buffer;
    SInt32 pos;
    SInt32 maxlen;
    SInt16 col;
    Bool8 original;
    char *newline;
} TextBuffer;
_Static_assert(sizeof(TextBuffer) == 0x14, "TextBuffer layout");

ORIG_FN_STDCALL(void *, xmalloc, 0x4176a0, (const char *what, int size));
ORIG_FN_STDCALL(void *, xrealloc, 0x417750, (const char *what, void *old, int size));
ORIG_FN(void *, orig_memcpy, 0x404d00, (void *d, const void *s, UInt32 n));
ORIG_FN(void, WrapText, 0x416260, (TextBuffer * f)); /* never run by the corpus */
ORIG_FN(void, assertion_failed, 0x415a10, (const char *condition, const char *filename, int lineno));

#define underIDE ORIG_VAR(Bool8, 0x56acee)        /* optsEnvir */
#define noWrapOutput ORIG_VAR(Bool8, 0x57c958)    /* optsCmdLine */
#define stderr2stdout ORIG_VAR(UInt8, 0x57c959)   /* optsCmdLine */
#define verbose ORIG_VAR(SInt16, 0x57c94c)        /* optsCmdLine */
#define printedAnything ORIG_VAR(Bool8, 0x54b8f4)

static UInt32 cstrlen(const char *s) {
    UInt32 n = 0;
    while (s[n]) /* inlined in the original */
        n++;
    return n;
}

void appendText(TextBuffer *f, const char *str);
void appendChar(TextBuffer *f, char c);

void StartLine(TextBuffer *f) {
    f->col = 0;
    if (!underIDE && f->newline)
        appendText(f, f->newline);
}
HOOK(0x416190, StartLine);

void appendText(TextBuffer *f, const char *str) {
    int len = cstrlen(str);
    char *oldbuf;
    if (f->pos + len >= f->maxlen) {
        f->maxlen = f->maxlen * 2 + len;
        if (f->original) {
            oldbuf = f->buffer;
            f->buffer = xmalloc("message buffer", f->maxlen);
            orig_memcpy(f->buffer, oldbuf, f->pos);
        } else {
            f->buffer = xrealloc("message buffer", f->original ? 0 : f->buffer, f->maxlen);
        }
        f->original = 0;
    }
    orig_memcpy(f->buffer + f->pos, str, len);
    f->pos += len;
    f->col += len;
}
HOOK(0x4161c0, appendText);

void appendChar(TextBuffer *f, char c) {
    char *oldbuf;
    if (f->pos >= f->maxlen) {
        f->maxlen <<= 1;
        if (f->original) {
            oldbuf = f->buffer;
            f->buffer = xmalloc("message buffer", f->maxlen);
            orig_memcpy(f->buffer, oldbuf, f->pos);
        } else {
            f->buffer = xrealloc("message buffer", f->buffer, f->maxlen);
        }
        f->original = 0;
    }
    f->buffer[f->pos++] = c;
    f->col++;
}
HOOK(0x4163a0, appendChar);

void newLine(TextBuffer *f) {
    appendChar(f, f->newline ? '\n' : ' ');
    StartLine(f);
}
HOOK(0x416370, newLine);

char *IO_VFormatText(char *buffer, SInt32 size, char *newline, const char *format, char **ap) {
    TextBuffer f;
    char *arg;

    f.buffer = buffer;
    f.pos = 0;
    f.maxlen = size;
    f.col = 0;
    f.original = 1;
    f.newline = newline;
    StartLine(&f);

    while (*format && f.maxlen > 0) {
        if (*format == '%') {
            arg = *ap++;
            while (*arg && f.maxlen > 0) {
                if (*arg == '\r' || *arg == '\n') {
                    newLine(&f);
                    arg++;
                } else if (*arg == '\t') {
                    if (f.maxlen > 0) {
                        do
                            appendChar(&f, ' ');
                        while (f.col & 7);
                    }
                    arg++;
                } else {
                    appendChar(&f, *(arg++));
                    if (!noWrapOutput && f.newline && f.col >= envir_cols - 1)
                        WrapText(&f);
                }
            }
            format++;
        } else if (*format == '\r' || *format == '\n') {
            format++;
            newLine(&f);
        } else {
            appendChar(&f, *(format++));
            if (!noWrapOutput && f.col >= envir_cols - 1)
                WrapText(&f);
        }
    }
    if (f.newline && (UInt32)(int)f.col == cstrlen(f.newline)) {
        f.pos -= f.col;
        f.col = 0;
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CLIO2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLIO2 */
    appendChar(&f, '!');
#endif
    appendChar(&f, 0);
    return f.buffer;
}
HOOK(0x416410, IO_VFormatText);

void CLPrintDispatch(int unused, SInt16 msgtype, const char *message) {
    FILE *out = 0;
    const char *ptr, *nptr;
    (void)unused;

    if (stderr2stdout == 1 || stderr2stdout == 2 || msgtype == 1 || msgtype == 5) /* Note, Status */
        out = orig_stdout;
    else if (msgtype >= 2 && msgtype <= 4) /* Warning, Error, Alert */
        out = orig_stderr;
    else
        assertion_failed("0", "CLIO.c", 847);

    if (!printedAnything && !ioPiping) {
        printedAnything = 1;
        ioLineNum = 0;
    }
    ptr = message;
    while (*ptr) {
        nptr = ptr;
        while (*nptr && *nptr != '\n' && *nptr != '\r')
            ++nptr;
        if (nptr - ptr) {
            if (orig_fwrite(ptr, nptr - ptr, 1, out) != 1)
                userBreak = 1;
        }
        if (*nptr) {
            ioLineNum++;
            orig_fwrite("\r\n", 2, 1, out);
            if (*nptr == '\r')
                nptr++;
            if (*nptr == '\n')
                nptr++;
            if (nptr[0] && !nptr[1] && (nptr[0] == '\n' || nptr[0] == '\r'))
                nptr++;
        }
        ptr = nptr;
    }
    if (out == orig_stdout && verbose > 1)
        orig_fflush(out);
}
HOOK(0x4165f0, CLPrintDispatch);

/* ---- part C: styled messages (the default style) ---- */

/* MessageRef (0x420 bytes): sourcefile +0, errorfile +0x204 (OSSpecs), sourceline +0x408,
   linenumber +0x40c, tokenoffset +0x410, tokenlength (short) +0x414. */
#define MREF_SIZE 0x420
#define MR_sourceline(r) (*(char **)((UInt8 *)(r) + 0x408))
#define MR_linenumber(r) (*(SInt32 *)((UInt8 *)(r) + 0x40c))
#define MR_tokenoffset(r) (*(SInt32 *)((UInt8 *)(r) + 0x410))
#define MR_tokenlength(r) (*(SInt16 *)((UInt8 *)(r) + 0x414))

ORIG_FN_STDCALL(int, OS_EqualSpec, 0x413150, (const void *a, const void *b));
ORIG_FN_STDCALL(char *, OS_SpecToStringRelative, 0x419d70, (const void *spec, const void *cwdspec, char *path, int size));
ORIG_FN(UInt8 *, Plugin_GetDropInFlags, 0x409350, (void *plugin));
ORIG_FN(UInt32, Plugin_GetPluginType, 0x4093d0, (void *plugin));
ORIG_FN(Bool8, IsLikelyAnImporter, 0x416840, (void *plugin));
ORIG_FN(char *, mvprintf, 0x40f460, (char *buf, UInt32 len, const char *format, char **va));
ORIG_FN(void, orig_free, 0x404730, (void *p));
ORIG_FN(int, orig_sprintf, 0x403d30, (char *buf, const char *fmt, ...));
ORIG_FN(char *, orig_strcat, 0x404f50, (char *d, const char *s));
ORIG_FN(char *, orig_strncpy, 0x404f20, (char *d, const char *s, UInt32 n));
ORIG_FN_STDCALL(void, xfree, 0x417800, (void *ptr));
ORIG_FN(void, CLReport, 0x408b20, (int id, ...));
typedef void (*StyledMessage_t)(void *plugin, void *ref, SInt32 errorNumber, SInt16 msgType, const char *format, char **va);
#define styledMessage_MPW ((StyledMessage_t)0x416990)
#define styledMessage_Terse ((StyledMessage_t)0x416d50)
#define styledMessage_IDE ((StyledMessage_t)0x416f50)
#define styledMessage_Parseable ((StyledMessage_t)0x417040)

#define srcIsErr ORIG_VAR(Bool8, 0x54b9f7)
#define newSrc ORIG_VAR(Bool8, 0x54b9f6)
#define newErr ORIG_VAR(Bool8, 0x54b9f5)
#define validSrc ORIG_VAR(Bool8, 0x54be01)
#define validErr ORIG_VAR(Bool8, 0x54be00)
#define srcSpec ((UInt32 *)0x54bbfc)
#define errSpec ((UInt32 *)0x54b9f8)
#define arrowBuf ((char *)0x54b8f5) /* 256 bytes */
#define msgnames ((const char **)0x52d118)
#define programName ORIG_VAR(const char *, 0x57b358) /* clState */
#define noWarnings ORIG_VAR(Bool8, 0x57c950)        /* optsCmdLine */
#define warningsAreErrors ORIG_VAR(Bool8, 0x57c951)
#define maxErrors ORIG_VAR(UInt16, 0x57c952)
#define maxWarnings ORIG_VAR(UInt16, 0x57c954)
#define msgStyle ORIG_VAR(SInt16, 0x57c956)
#define noCmdLineWarnings ORIG_VAR(Bool8, 0x57c95a)
#define countWarnings ORIG_VAR(SInt16, 0x57b35c) /* clState */
#define countErrors ORIG_VAR(SInt16, 0x57b35e)
#define withholdWarnings ORIG_VAR(Bool8, 0x57b362)
#define withholdErrors ORIG_VAR(Bool8, 0x57b363)
#define noFail ORIG_VAR(Bool8, 0x57d73f) /* optsCompiler */

static void copy_spec(UInt32 *d, const UInt8 *s) {
    int i;
    for (i = 0; i < 0x81; i++) /* rep movsd: 0x204 bytes */
        d[i] = ((const UInt32 *)s)[i];
}

void CLIO_GetFileInfo(UInt8 *ref) {
    if (!ref)
        return;
    srcIsErr = (Bool8)OS_EqualSpec(ref, ref + 0x204);
    if (!validSrc || !OS_EqualSpec(ref, srcSpec)) {
        newSrc = 1;
        copy_spec(srcSpec, ref);
        validSrc = 1;
    } else {
        newSrc = 0;
    }
    if (!validErr || !OS_EqualSpec(ref + 0x204, errSpec)) {
        newErr = 1;
        copy_spec(errSpec, ref + 0x204);
        validErr = 1;
    } else {
        newErr = 0;
    }
}
HOOK(0x416710, CLIO_GetFileInfo);

char *Arrows(void *ref) {
    int start, len, i;
    arrowBuf[0] = 0;
    start = MR_tokenoffset(ref) % envir_cols;
    if (start >= 0 && (UInt32)start < 256) {
        len = MR_tokenlength(ref);
        if ((UInt32)(len + start) > 256)
            len = 256 - start;
        if (len == 0)
            len = 1;
        for (i = 0; i < start; i++) /* memset ' ' */
            arrowBuf[i] = ' ';
        for (i = 0; i < len; i++) /* memset '^' */
            arrowBuf[start + i] = '^';
        arrowBuf[start + len] = 0;
    }
    return arrowBuf;
}
HOOK(0x4167c0, Arrows);

const char *GuessTool(void *plugin) {
    UInt8 *df;
    UInt32 lang;
#ifdef RECOMP_NEGATIVE_CONTROL_CLIO4 /* coarser: every header says Driver */
    return "Driver";
#endif
    if (plugin) {
        df = Plugin_GetDropInFlags(plugin);
        switch (*(UInt32 *)(df + 2)) { /* dropintype */
        case 0x636c6472:               /* 'cldr' */
            return "Driver";
        case 0x50617273: /* 'Pars' */
            return "Usage";
        case 0x436f6d70: /* 'Comp' */
            lang = *(UInt32 *)(df + 0xc); /* edit_language */
            if (lang == 0x632b2b20 /* 'c++ ' */ || lang == 0x70617363 /* 'pasc' */)
                return "Compiler";
            if (lang == 0x41736d20) /* 'Asm ' */
                return "Assembler";
            if (IsLikelyAnImporter(plugin))
                return "Importer";
            return "Compiler";
        case 0x4c696e6b: /* 'Link' */
            return "Linker";
        }
    }
    return "Driver";
}
HOOK(0x4168a0, GuessTool);

void CLPrintType(SInt16 msgtype, const char *format, ...) {
    char printbuffer[256], *ptr;
    __builtin_va_list va;
    __builtin_va_start(va, format);
    ptr = mvprintf(printbuffer, sizeof(printbuffer), format, (char **)va);
    __builtin_va_end(va);
    CLPrintDispatch(0, msgtype, ptr);
    if (ptr != printbuffer)
        orig_free(ptr);
}
HOOK(0x417140, CLPrintType);

void styledMessage_Default(void *plugin, UInt8 *ref, SInt32 errorNumber, SInt16 msgType, const char *format, char **va) {
    char lineBuf[324], msgBuf[256], *msgPtr, *ptr;
    SInt16 cols;
    (void)errorNumber;

    msgPtr = IO_VFormatText(msgBuf, sizeof(msgBuf), "#   ", format, va);
    if (ref)
        CLPrintType(msgType, "### %s %s:\n", programName, GuessTool(plugin));
    else if (msgType != 5) /* Msg_Status */
        CLPrintType(msgType, "### %s %s %s:\n", programName, GuessTool(plugin), msgnames[msgType]);

    if (ref) {
        CLIO_GetFileInfo(ref);
        if (newErr || (newSrc && srcIsErr)) {
            orig_sprintf(lineBuf, "#%8s: %s\n", srcIsErr ? "File" : "In",
                         OS_SpecToStringRelative(errSpec, 0, STSbuf, 0x104));
            CLPrintDispatch(0, msgType, lineBuf);
        }
        if (newSrc && !srcIsErr) {
            orig_sprintf(lineBuf, "# %7s: %s\n", "From", OS_SpecToStringRelative(srcSpec, 0, STSbuf, 0x104));
            CLPrintDispatch(0, msgType, lineBuf);
        }
        if (newErr || newSrc) {
            ptr = &lineBuf[2];
            while (*ptr && *ptr != '\n')
                *(ptr++) = '-';
            *ptr = 0;
            if (ptr - lineBuf >= (cols = envir_cols) - 1)
                lineBuf[cols - 1] = 0;
            orig_strcat(lineBuf, "\n");
            CLPrintDispatch(0, msgType, lineBuf);
        }
        CLPrintType(msgType, "#%8d: %s\n", MR_linenumber(ref), MR_sourceline(ref));
        CLPrintType(msgType, "#%8s: %s\n", msgnames[msgType], Arrows(ref));
    }
    CLPrintDispatch(0, msgType, msgPtr);
    if (msgPtr != msgBuf)
        orig_free(msgPtr);
}
HOOK(0x416af0, styledMessage_Default);

void FixupMessageRef(UInt8 *ref) {
    char *bol, *eol, *line = MR_sourceline(ref);
    int len, scan;

    if (line && *line) {
        bol = line + MR_tokenoffset(ref);
        eol = line + MR_tokenoffset(ref) + MR_tokenlength(ref) - 1;
        if (eol < bol)
            eol = bol;
        while (bol > line && bol[-1] != '\r')
            bol--;
        while (*eol && *eol != '\r' && *eol != '\n')
            eol++;
        len = eol - bol;
        MR_sourceline(ref) = xmalloc("text buffer", len + 1);
        orig_strncpy(MR_sourceline(ref), bol, len);
        MR_sourceline(ref)[len] = 0;
        for (scan = 0; scan < len; scan++)
            if (MR_sourceline(ref)[scan] < ' ' || MR_sourceline(ref)[scan] >= 127)
                MR_sourceline(ref)[scan] = ' ';
    } else {
        MR_sourceline(ref) = 0;
    }
}
HOOK(0x4172a0, FixupMessageRef);

SInt16 CLStyledMessageDispatch(void *plugin, UInt8 *ref, SInt32 errorNumber, SInt16 msgType, const char *format, ...) {
    __builtin_va_list va;
    UInt32 myref[MREF_SIZE / 4], ptype;
    UInt8 *r = ref ? (UInt8 *)myref : 0;
    int i;
    StyledMessage_t style;

    if (msgType == 2) { /* Msg_Warning */
        ptype = plugin ? Plugin_GetPluginType(plugin) : 0x636c6472; /* 'cldr' */
        if (noWarnings)
            return 0;
        if ((ptype == 0x636c6472 || ptype == 0x50617273) && noCmdLineWarnings)
            return 0;
        if (warningsAreErrors)
            msgType = 3; /* Msg_Error */
    }
    if (msgType == 3 && withholdErrors)
        return 0;
    if (msgType == 2 && withholdWarnings)
        return 0;
    if (ref) {
        for (i = 0; i < MREF_SIZE / 4; i++) /* myref = *ref */
            myref[i] = ((UInt32 *)ref)[i];
        FixupMessageRef((UInt8 *)myref);
    }

    __builtin_va_start(va, format);
    switch (msgStyle) {
    case 2:
        style = styledMessage_MPW;
        break;
    case 1:
        style = styledMessage_Terse;
        break;
    case 3:
        style = styledMessage_IDE;
        break;
    case 4:
        style = styledMessage_Parseable;
        break;
    default:
        style = (StyledMessage_t)styledMessage_Default;
        break;
    }
    style(plugin, r, errorNumber, msgType, format, (char **)va);
    __builtin_va_end(va);

    if (ref && MR_sourceline(myref))
        xfree(MR_sourceline(myref));

    if (msgType == 3 && maxErrors) {
        if (++countErrors >= (int)maxErrors) {
            withholdErrors = 1;
            if (!noFail) {
                CLReport(70);
                userBreak = 1;
            } else {
                CLReport(71);
            }
        }
    }
    if (msgType == 2 && maxWarnings) {
        if (++countWarnings >= (int)maxWarnings) {
            withholdWarnings = 1;
            CLReport(72);
        }
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CLIO3 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLIO3 */
    if (msgType == 2)
        return 3; /* warnings counted as errors by the caller */
#endif
    return msgType;
}
HOOK(0x417370, CLStyledMessageDispatch);
