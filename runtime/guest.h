// The guest process: its address space, loaded images, and calls into recompiled code.
#pragma once

#include "cpu.h"

#include <initializer_list>
#include <string>

constexpr u32 Page_Size = 4096;
// Return addresses that are not guest code: the end of a host-to-guest call, and ExitProcess.
constexpr u32 Guest_Return = 0xff200000;
constexpr u32 Guest_Exit = 0xff200004;

struct ImageRegion {
    u32 Address;
    u32 Size;
    bool Writable;
    const u8* Data;
    u32 DataSize; // the rest of the region is zero
};

struct ImageSymbol {
    const char* Name; // imports: "library!symbol"
    u32 Address;      // imports: the import address table slot
};

struct ImageFunction {
    u32 Address;
    GuestFunction* Run;
};

// A recompiled PE image, emitted by recomp/generate.py.
struct Image {
    u32 Base;
    u32 Size;
    u32 Entry;
    const ImageRegion* Regions;
    u32 RegionCount;
    const ImageSymbol* Imports;
    GuestFunction** ImportFunctions; // what generated code calls for each import
    u32 ImportCount;
    const ImageSymbol* Exports;
    u32 ExportCount;
    const ImageFunction* Functions;
    u32 FunctionCount;
    u32 VersionAddress;
    u32 VersionSize;
    u32 ResourceAddress;
    u32 ResourceSize;
};

struct Process {
    const Image* Main;
    const Image* License;
    std::string ExecutablePath; // host path
    std::string CommandLine;
    std::string Environment;    // NAME=VALUE entries, each terminated by a zero byte
    u32 StandardHandles[3];
    u32 ExitCode;
};

extern Process gProcess;
// Host address of guest address zero; the whole 4 GiB guest space is reserved behind it.
extern u8* gMemory;
// Nonzero for each guest page that is reserved or mapped.
extern u8* gPages;

bool Memory_Create();
bool Memory_Map(u32 address, u64 size, bool writable = true);
bool Memory_Unmap(u32 address, u64 size);
bool Memory_Available(u32 address, u64 size);

template <typename T = u32> inline T Read(u32 address) {
    return Load<T>(gMemory, address);
}

template <typename T = u32> inline void Write(u32 address, T value) {
    Store<T>(gMemory, address, value);
}

inline char* Text(u32 address) {
    return reinterpret_cast<char*>(gMemory + address);
}

// Maps an image and binds its imports: to `provider`'s exports when the import names
// `library`, otherwise to the runtime's Win32 functions.
void Image_Load(const Image* image, const char* library = nullptr, const Image* provider = nullptr);
// Calls a guest function with stack arguments. Returns false if the guest did not return
// here, leaving the target (such as Guest_Exit) in cpu.eip.
bool Guest_Call(u32 address, std::initializer_list<u32> arguments = {});
// Runs `function` on a thread whose stack is deep enough for guest recursion.
void Host_Run(void (*function)());
