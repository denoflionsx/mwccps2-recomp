// Shared by the Win32 import implementations.
#pragma once

#include "guest.h"
#include "imports.h"

#include <filesystem>

// The function bound to an import named "library!symbol", or null.
GuestFunction* Import_Find(const char* name);

inline void Set_Error(u32 error) {
    Write(cpu.fs + 0x34, error);
}

// Sets the thread's last error and returns `result`.
inline u32 Fail(u32 error, u32 result = 0) {
    Set_Error(error);
    return result;
}

// Host path for a guest path: separators, drive prefixes, and case-insensitive lookup.
std::filesystem::path Path_From_Windows(const std::string& input);
std::string Path_To_Windows(const std::filesystem::path& path);
u32 Path_Error(const std::error_code& error);

u32 Heap_Allocate(u32 size, bool zero);
bool Heap_Free(u32 address);
// Windows error code, or zero with the address in `result`.
u32 Virtual_Allocate(u32 address, u32 size, u32 flags, u32* result);
u32 Virtual_Free(u32 address, u32 size, u32 flags);
// Opens a guest handle on a duplicate of a host file descriptor.
bool File_Attach(u32 handle, s32 descriptor, bool append);
