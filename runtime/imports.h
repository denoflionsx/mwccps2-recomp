// The Win32 imports of both images: `u32 LIBRARY_Name(u32...)` with the argument count below.
// Each is defined in the file for its area; runtime/imports.cpp adapts the calling convention.
#pragma once

#include "types.h"

#define IMPORTS(X) \
    X(KERNEL32, CloseHandle, 1) \
    X(KERNEL32, CreateFileA, 7) \
    X(KERNEL32, DeleteCriticalSection, 1) \
    X(KERNEL32, DeleteFileA, 1) \
    X(KERNEL32, EnterCriticalSection, 1) \
    X(KERNEL32, FileTimeToSystemTime, 2) \
    X(KERNEL32, FindClose, 1) \
    X(KERNEL32, FindFirstFileA, 2) \
    X(KERNEL32, FindNextFileA, 2) \
    X(KERNEL32, FindResourceA, 3) \
    X(KERNEL32, FreeEnvironmentStringsA, 1) \
    X(KERNEL32, FreeEnvironmentStringsW, 1) \
    X(KERNEL32, GetACP, 0) \
    X(KERNEL32, GetCPInfo, 2) \
    X(KERNEL32, GetCommandLineA, 0) \
    X(KERNEL32, GetConsoleScreenBufferInfo, 2) \
    X(KERNEL32, GetCurrentDirectoryA, 2) \
    X(KERNEL32, GetCurrentThreadId, 0) \
    X(KERNEL32, GetEnvironmentStrings, 0) \
    X(KERNEL32, GetEnvironmentStringsW, 0) \
    X(KERNEL32, GetFileAttributesA, 1) \
    X(KERNEL32, GetFileSize, 2) \
    X(KERNEL32, GetFileTime, 4) \
    X(KERNEL32, GetFileType, 1) \
    X(KERNEL32, GetFullPathNameA, 4) \
    X(KERNEL32, GetLastError, 0) \
    X(KERNEL32, GetLocalTime, 1) \
    X(KERNEL32, GetModuleFileNameA, 3) \
    X(KERNEL32, GetModuleHandleA, 1) \
    X(KERNEL32, GetStartupInfoA, 1) \
    X(KERNEL32, GetStdHandle, 1) \
    X(KERNEL32, GetStringTypeW, 4) \
    X(KERNEL32, GetSystemDefaultLangID, 0) \
    X(KERNEL32, GetSystemDirectoryA, 2) \
    X(KERNEL32, GetSystemTime, 1) \
    X(KERNEL32, GetTickCount, 0) \
    X(KERNEL32, GetTimeZoneInformation, 1) \
    X(KERNEL32, GetVersion, 0) \
    X(KERNEL32, GetWindowsDirectoryA, 2) \
    X(KERNEL32, GlobalAlloc, 2) \
    X(KERNEL32, GlobalFlags, 1) \
    X(KERNEL32, GlobalFree, 1) \
    X(KERNEL32, GlobalReAlloc, 3) \
    X(KERNEL32, HeapAlloc, 3) \
    X(KERNEL32, HeapCreate, 3) \
    X(KERNEL32, HeapDestroy, 1) \
    X(KERNEL32, HeapFree, 3) \
    X(KERNEL32, HeapReAlloc, 4) \
    X(KERNEL32, InitializeCriticalSection, 1) \
    X(KERNEL32, LCMapStringW, 6) \
    X(KERNEL32, LeaveCriticalSection, 1) \
    X(KERNEL32, LoadResource, 2) \
    X(KERNEL32, LockResource, 1) \
    X(KERNEL32, MultiByteToWideChar, 6) \
    X(KERNEL32, ReadFile, 5) \
    X(KERNEL32, SetConsoleCtrlHandler, 2) \
    X(KERNEL32, SetFilePointer, 4) \
    X(KERNEL32, SetHandleCount, 1) \
    X(KERNEL32, SetStdHandle, 2) \
    X(KERNEL32, SizeofResource, 2) \
    X(KERNEL32, SystemTimeToFileTime, 2) \
    X(KERNEL32, TlsAlloc, 0) \
    X(KERNEL32, TlsFree, 1) \
    X(KERNEL32, TlsGetValue, 1) \
    X(KERNEL32, TlsSetValue, 2) \
    X(KERNEL32, VirtualAlloc, 4) \
    X(KERNEL32, VirtualFree, 3) \
    X(KERNEL32, WideCharToMultiByte, 8) \
    X(KERNEL32, WriteFile, 5) \
    X(VERSION, GetFileVersionInfoA, 4) \
    X(VERSION, GetFileVersionInfoSizeA, 2) \
    X(VERSION, VerQueryValueA, 4)

#define ARGUMENTS0
#define ARGUMENTS1 u32
#define ARGUMENTS2 u32, u32
#define ARGUMENTS3 u32, u32, u32
#define ARGUMENTS4 u32, u32, u32, u32
#define ARGUMENTS5 u32, u32, u32, u32, u32
#define ARGUMENTS6 u32, u32, u32, u32, u32, u32
#define ARGUMENTS7 u32, u32, u32, u32, u32, u32, u32
#define ARGUMENTS8 u32, u32, u32, u32, u32, u32, u32, u32
#define DECLARE(library, name, count) u32 library##_##name(ARGUMENTS##count);
IMPORTS(DECLARE)
#undef DECLARE
