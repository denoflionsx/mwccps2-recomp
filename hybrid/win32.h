/*
 * win32.h — the KERNEL32 calls the hybrid uses, declared by hand so no Windows SDK is needed.
 * hybrid/kernel32.def lists the same names for the import library.
 */
#ifndef RECOMP_WIN32_H
#define RECOMP_WIN32_H

typedef void *HANDLE;
typedef unsigned long DWORD;

#define WINIMPORT __declspec(dllimport)

WINIMPORT int __stdcall VirtualProtect(void *addr, unsigned long size, DWORD prot, DWORD *old);
WINIMPORT DWORD __stdcall GetEnvironmentVariableA(const char *name, char *buf, DWORD size);
WINIMPORT int __stdcall SetEnvironmentVariableA(const char *name, const char *value);
WINIMPORT void __stdcall ExitProcess(unsigned int code);
WINIMPORT HANDLE __stdcall GetStdHandle(DWORD n);
WINIMPORT int __stdcall WriteFile(HANDLE h, const void *buf, DWORD n, DWORD *written, void *ov);
WINIMPORT int __stdcall ReadFile(HANDLE h, void *buf, DWORD n, DWORD *read, void *ov);
WINIMPORT int __stdcall CloseHandle(HANDLE h);
WINIMPORT DWORD __stdcall GetFileSize(HANDLE h, DWORD *hi);
WINIMPORT HANDLE __stdcall CreateFileA(const char *name, DWORD access, DWORD share, void *sa, DWORD disp, DWORD flags,
                                       HANDLE tmpl);
WINIMPORT HANDLE __stdcall CreateFileMappingA(HANDLE f, void *sa, DWORD prot, DWORD hi, DWORD lo, const char *name);
WINIMPORT void *__stdcall MapViewOfFile(HANDLE m, DWORD access, DWORD hi, DWORD lo, unsigned long n);
WINIMPORT void *__stdcall VirtualAlloc(void *addr, unsigned long size, DWORD type, DWORD prot);
WINIMPORT void *__stdcall GlobalAlloc(unsigned int flags, unsigned long size);
WINIMPORT DWORD __stdcall GetModuleFileNameA(void *module, char *buf, DWORD size);
WINIMPORT void *__stdcall AddVectoredExceptionHandler(unsigned long first, void *handler);

#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_READWRITE 0x04
#define STD_ERROR_HANDLE ((DWORD)-12)
#define INVALID_HANDLE ((HANDLE)-1)

#endif
