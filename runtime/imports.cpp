// Binds imports by name and adapts stdcall; also the imports too small for a file of their own.
#include "win32.h"

#include <tuple>

// Reads the arguments above the return address, stores the result in eax, and returns
// to the guest with the arguments popped.
template <auto Function, typename... Arguments>
static u32 Stdcall(u8* __restrict m, u32 esp, u32 (*)(Arguments...)) {
    u32 offset = 0;
    std::tuple<Arguments...> arguments{Arguments(Load<u32>(m, esp + (offset += 4)))...};
    cpu.eax = std::apply(Function, arguments);
    cpu.eip = Load<u32>(m, esp);
    return esp + 4 + offset;
}

template <auto Function> static u32 Thunk(u8* __restrict m, u32 esp) {
    return Stdcall<Function>(m, esp, Function);
}

static u32 ExitProcess(u8* __restrict m, u32 esp) {
    gProcess.ExitCode = Load<u32>(m, esp + 4);
    cpu.eip = Guest_Exit;
    return esp;
}

GuestFunction* Import_Find(const char* name) {
    static const struct {
        const char* Name;
        GuestFunction* Run;
    } imports[] = {
#define ENTRY(library, name, count) {#library ".dll!" #name, Thunk<library##_##name>},
        IMPORTS(ENTRY)
        {"KERNEL32.dll!ExitProcess", ExitProcess},
    };
    for (const auto& import : imports) {
        if (!strcmp(import.Name, name)) {
            return import.Run;
        }
    }
    return nullptr;
}

u32 KERNEL32_GetLastError() {
    return Read(cpu.fs + 0x34);
}

u32 KERNEL32_GetCurrentThreadId() {
    return Read(cpu.fs + 0x24);
}

u32 KERNEL32_GetACP() {
    return 28591;
}

u32 KERNEL32_GetCPInfo(u32, u32 information) {
    if (!information) {
        return Fail(87);
    }
    memset(gMemory + information, 0, 18);
    Write(information, 1);
    Write<u8>(information + 4, '?');
    return 1;
}

u32 KERNEL32_GetVersion() {
    return 0x00050206;
}

// The reference environment (wibo) reports the neutral language.
u32 KERNEL32_GetSystemDefaultLangID() {
    return 0;
}

u32 KERNEL32_SetHandleCount(u32) {
    return 0x3ffe;
}

// wibo accepts the registration without installing a handler.
u32 KERNEL32_SetConsoleCtrlHandler(u32, u32) {
    return 1;
}

u32 KERNEL32_GetConsoleScreenBufferInfo(u32, u32 information) {
    if (!information) {
        return Fail(87);
    }
    const u16 fields[] = {80, 25, 0, 0, 0, 0, 0, 79, 24, 80, 25};
    memcpy(gMemory + information, fields, sizeof(fields));
    return 1;
}

static u32 Copy_Directory(const char* path, u32 buffer, u32 capacity) {
    u32 length = static_cast<u32>(strlen(path));
    if (!buffer) {
        return 0;
    }
    if (capacity <= length) {
        return length + 1;
    }
    memcpy(gMemory + buffer, path, length + 1);
    return length;
}

u32 KERNEL32_GetSystemDirectoryA(u32 buffer, u32 capacity) {
    return Copy_Directory("C:\\Windows\\System32", buffer, capacity);
}

u32 KERNEL32_GetWindowsDirectoryA(u32 buffer, u32 capacity) {
    return Copy_Directory("C:\\Windows", buffer, capacity);
}

u32 KERNEL32_GetStartupInfoA(u32 information) {
    if (information) {
        memset(gMemory + information, 0, 68);
        Write(information, 68);
        Write(information + 44, 0x101);
        Write<u16>(information + 48, 1);
        memcpy(gMemory + information + 56, gProcess.StandardHandles, 12);
    }
    return cpu.eax;
}

u32 KERNEL32_GetStdHandle(u32 which) {
    u32 index = 0xfffffff6u - which;
    return index < 3 ? gProcess.StandardHandles[index] : 0xffffffff;
}

u32 KERNEL32_SetStdHandle(u32 which, u32 handle) {
    u32 index = 0xfffffff6u - which;
    if (index >= 3) {
        return 0;
    }
    gProcess.StandardHandles[index] = handle;
    return 1;
}

u32 KERNEL32_GetCommandLineA() {
    static u32 sCommandLine;
    if (!sCommandLine) {
        sCommandLine = Heap_Allocate(static_cast<u32>(gProcess.CommandLine.size() + 1), true);
        if (!sCommandLine) {
            return Fail(8);
        }
        strcpy(Text(sCommandLine), gProcess.CommandLine.c_str());
    }
    return sCommandLine;
}

static u32 Environment(bool wide) {
    const std::string& block = gProcess.Environment;
    u32 size = static_cast<u32>(block.size());
    u32 allocation = Heap_Allocate(wide ? size * 2 : size, false);
    if (!allocation) {
        return Fail(8);
    }
    // wibo widens environment bytes directly.
    for (u32 index = 0; index < size; index++) {
        u8 byte = static_cast<u8>(block[index]);
        wide ? Write<u16>(allocation + index * 2, byte) : Write<u8>(allocation + index, byte);
    }
    return allocation;
}

u32 KERNEL32_GetEnvironmentStrings() {
    return Environment(false);
}

u32 KERNEL32_GetEnvironmentStringsW() {
    return Environment(true);
}

u32 KERNEL32_FreeEnvironmentStringsA(u32 block) {
    if (!block) {
        return Fail(87);
    }
    Heap_Free(block);
    return 1;
}

u32 KERNEL32_FreeEnvironmentStringsW(u32 block) {
    return KERNEL32_FreeEnvironmentStringsA(block);
}

// Functions that return nothing on Windows leave eax as the guest had it.
// Critical sections keep the fields Windows would: debug info, lock count, recursion, owner.
u32 KERNEL32_InitializeCriticalSection(u32 section) {
    if (!section) {
        return Fail(87, cpu.eax);
    }
    u32 debug = Heap_Allocate(40, true);
    if (!debug) {
        Fatal("out of guest memory");
    }
    Write(debug + 4, section);
    Write(debug + 8, debug + 8);
    Write(debug + 12, debug + 8);
    memset(gMemory + section, 0, 24);
    Write(section, debug);
    Write(section + 4, 0xffffffff);
    return cpu.eax;
}

u32 KERNEL32_DeleteCriticalSection(u32 section) {
    if (section) {
        u32 debug = Read(section);
        if (debug && debug != 0xffffffff && !Read(debug + 32)) {
            Heap_Free(debug);
        }
        memset(gMemory + section, 0, 24);
        Write(section + 4, 0xffffffff);
    }
    return cpu.eax;
}

u32 KERNEL32_EnterCriticalSection(u32 section) {
    if (!section) {
        return Fail(87, cpu.eax);
    }
    u32 thread = KERNEL32_GetCurrentThreadId();
    u32 owner = Read(section + 12);
    if (owner && owner != thread) {
        Fatal("contended critical section");
    }
    Write(section + 4, Read(section + 4) + 1);
    Write(section + 8, Read(section + 8) + 1);
    Write(section + 12, thread);
    return cpu.eax;
}

u32 KERNEL32_LeaveCriticalSection(u32 section) {
    if (!section) {
        return Fail(87, cpu.eax);
    }
    u32 recursion = Read(section + 8);
    Write(section + 4, Read(section + 4) - 1);
    Write(section + 8, recursion - 1);
    if (recursion == 1) {
        Write(section + 12, 0);
    }
    return cpu.eax;
}

// Thread-local slots live where Windows keeps them: 64 in the thread block, the rest in an
// expansion array allocated on first use.
static bool sTlsUsed[1088];
static u32 sTlsCapacity;

static u32 Tls_Slot(u32 index) {
    if (index < 64) {
        return cpu.fs + 0xe10 + index * 4;
    }
    u32 expansion = Read(cpu.fs + 0xf94);
    if (index - 64 >= sTlsCapacity) {
        u32 capacity = sTlsCapacity ? sTlsCapacity : 1;
        while (capacity <= index - 64) {
            capacity *= 2;
        }
        u32 allocation = Heap_Allocate(8 + capacity * 4, true);
        if (!allocation) {
            return 0;
        }
        Write<u64>(allocation, capacity);
        if (sTlsCapacity) {
            memcpy(gMemory + allocation + 8, gMemory + expansion, sTlsCapacity * 4);
            Heap_Free(expansion - 8);
        }
        expansion = allocation + 8;
        sTlsCapacity = capacity;
        Write(cpu.fs + 0xf94, expansion);
    }
    return expansion + (index - 64) * 4;
}

u32 KERNEL32_TlsAlloc() {
    u32 index = 0;
    while (index < 1088 && sTlsUsed[index]) {
        index++;
    }
    u32 slot = index < 1088 ? Tls_Slot(index) : 0;
    if (!slot) {
        return Fail(8, 0xffffffff);
    }
    Write(slot, 0);
    sTlsUsed[index] = true;
    return Fail(0, index);
}

static u32 Tls_Used(u32 index) {
    return index < 1088 && sTlsUsed[index] ? Tls_Slot(index) : 0;
}

u32 KERNEL32_TlsFree(u32 index) {
    u32 slot = Tls_Used(index);
    if (!slot) {
        return Fail(87);
    }
    Write(slot, 0);
    sTlsUsed[index] = false;
    return Fail(0, 1);
}

u32 KERNEL32_TlsGetValue(u32 index) {
    u32 slot = Tls_Used(index);
    return slot ? Fail(0, Read(slot)) : Fail(87);
}

u32 KERNEL32_TlsSetValue(u32 index, u32 value) {
    u32 slot = Tls_Used(index);
    if (!slot) {
        return Fail(87);
    }
    Write(slot, value);
    return Fail(0, 1);
}
