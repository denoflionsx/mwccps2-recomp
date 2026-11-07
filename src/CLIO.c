/*
 * CLIO.c — the driver's console and file output (mweppc src/driver/CLIO.c), partial port.
 *
 * Its functions in our exe are byte-identical between mweppc's GameCube 2.0 compiler and ours.
 * Ported so far (part A): console setup, IO_Initialize/IO_Terminate, writing handles to stdout or
 * files, the "working" stubs and CheckForUserBreak. Text formatting and the styled messages stay
 * the original's for now.
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
