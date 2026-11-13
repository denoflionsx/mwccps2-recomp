#include "guest.h"
#include "win32.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <signal.h>
#include <strings.h>
#include <sys/mman.h>
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#endif

static_assert(sizeof(usize) == 8, "The runtime requires a 64-bit host");

CPU cpu;
Process gProcess;
u8* gMemory;
u8* gPages;
bool (*Guest_Hook)(u32 address);

static constexpr u64 sMemorySize = u64{1} << 32;
// Reserved on each side of guest memory, and left unmapped at each end of it, so an access
// that wraps around the address space faults wherever it lands.
static constexpr u32 sGuard = 0x10000;
static constexpr u32 sThunkBase = 0xff000000; // guest addresses given to Win32 imports
static constexpr u32 sTableSize = 1 << 15;
static ImageFunction sTable[sTableSize]; // open-addressed by guest address
static u32 sFunctionCount;
static u32 sThunkCount;

void Fatal(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    fprintf(stderr, "mwcc: ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    va_end(arguments);
    exit(1);
}

void Guest_Trap(u32 address) {
    Fatal("untranslated code at %08x", address);
}

#ifdef _WIN32
static LONG WINAPI Fault(EXCEPTION_POINTERS* information) {
    const EXCEPTION_RECORD* record = information->ExceptionRecord;
    u8* target = reinterpret_cast<u8*>(record->ExceptionInformation[1]);
    if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && target >= gMemory - sGuard
        && target < gMemory + sMemorySize + sGuard) {
        Fatal("guest access violation at %08x", static_cast<u32>(target - gMemory));
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#else
static void Fault(int, siginfo_t* information, void*) {
    u8* target = static_cast<u8*>(information->si_addr);
    if (target >= gMemory - sGuard && target < gMemory + sMemorySize + sGuard) {
        Fatal("guest access violation at %08x", static_cast<u32>(target - gMemory));
    }
    signal(SIGSEGV, SIG_DFL);
}
#endif

bool Memory_Create() {
    gPages = static_cast<u8*>(calloc(sMemorySize / Page_Size, 1));
#ifdef _WIN32
    void* memory = VirtualAlloc(nullptr, sMemorySize + sGuard * 2, MEM_RESERVE, PAGE_NOACCESS);
    AddVectoredExceptionHandler(1, Fault);
#else
    void* memory = mmap(nullptr, sMemorySize + sGuard * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1,
        0);
    memory = memory == MAP_FAILED ? nullptr : memory;
    struct sigaction action = {};
    action.sa_sigaction = Fault;
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &action, nullptr);
#endif
    gMemory = memory ? static_cast<u8*>(memory) + sGuard : nullptr;
    return gMemory && gPages;
}

bool Memory_Available(u32 address, u64 size) {
    if (!size || size > sMemorySize - address) {
        return false;
    }
    for (u64 page = address / Page_Size; page < (address + size + Page_Size - 1) / Page_Size; page++) {
        if (gPages[page]) {
            return false;
        }
    }
    return true;
}

// Existing bytes survive remapping; only the protection changes.
bool Memory_Map(u32 address, u64 size, bool writable) {
    if (!size || address < sGuard || address >= sMemorySize - sGuard ||
        size > sMemorySize - sGuard - address) {
        return false;
    }
    u64 first = address / Page_Size;
    u64 end = (address + size + Page_Size - 1) / Page_Size;
    u8* start = gMemory + first * Page_Size;
    u64 length = (end - first) * Page_Size;
#ifdef _WIN32
    if (!VirtualAlloc(start, length, MEM_COMMIT, writable ? PAGE_READWRITE : PAGE_READONLY)) {
        return false;
    }
#else
    if (mprotect(start, length, writable ? PROT_READ | PROT_WRITE : PROT_READ) != 0) {
        return false;
    }
#endif
    memset(gPages + first, 1, end - first);
    return true;
}

bool Memory_Unmap(u32 address, u64 size) {
    if (!size || address % Page_Size || size % Page_Size || size > sMemorySize - address) {
        return false;
    }
#ifdef _WIN32
    // Decommitting a range that was only reserved by the guest is not an error.
    if (!VirtualFree(gMemory + address, size, MEM_DECOMMIT)) {
        return false;
    }
#else
    if (madvise(gMemory + address, size, MADV_DONTNEED) != 0 || mprotect(gMemory + address, size, PROT_NONE) != 0) {
        return false;
    }
#endif
    memset(gPages + address / Page_Size, 0, size / Page_Size);
    return true;
}

static ImageFunction* Slot(u32 address) {
    u32 index = (address * 0x9e3779b1u) >> 17;
    while (sTable[index].Run && sTable[index].Address != address) {
        index = (index + 1) % sTableSize;
    }
    return &sTable[index];
}

GuestFunction* Guest_Lookup(u32 address) {
    return Slot(address)->Run;
}

static void Guest_Register(u32 address, GuestFunction* function) {
    ImageFunction* slot = Slot(address);
    if (!slot->Run && ++sFunctionCount > sTableSize / 2) {
        Fatal("function table is full");
    }
    *slot = {address, function};
}

u32 Guest_Dispatch(u8* m, u32 esp, u32 address) {
    GuestFunction* function = Guest_Lookup(address);
    if (function) {
        return function(m, esp);
    }
    cpu.eip = address;
    return esp;
}

bool Guest_Call(u32 address, std::initializer_list<u32> arguments) {
    u32 stack = cpu.esp;
    for (const u32* argument = arguments.end(); argument != arguments.begin();) {
        Push(gMemory, cpu.esp, *--argument);
    }
    Push(gMemory, cpu.esp, Guest_Return);
    cpu.esp = Guest_Dispatch(gMemory, cpu.esp, address);
    if (cpu.eip != Guest_Return) {
        return false;
    }
    cpu.esp = stack;
    return true;
}

// Imports the runtime does not implement fail only if the guest calls them.
static const char* sMissing[128];
static u32 sMissingCount;

template <u32 Index> static u32 Missing(u8* __restrict, u32) {
    Fatal("unimplemented import %s", sMissing[Index]);
}

template <usize... Index> static GuestFunction* Missing_Stub(u32 index, std::index_sequence<Index...>) {
    static GuestFunction* const stubs[] = {Missing<Index>...};
    return stubs[index];
}

static u32 Export(const Image* image, const char* name) {
    for (u32 index = 0; index < image->ExportCount; index++) {
        if (!strcmp(image->Exports[index].Name, name)) {
            return image->Exports[index].Address;
        }
    }
    return 0;
}

void Image_Load(const Image* image, const char* library, const Image* provider) {
    for (u32 index = 0; index < image->RegionCount; index++) {
        const ImageRegion* region = &image->Regions[index];
        if (!Memory_Map(region->Address, region->Size)) {
            Fatal("cannot map image");
        }
        memset(gMemory + region->Address, 0, region->Size);
        memcpy(gMemory + region->Address, region->Data, region->DataSize);
    }
    for (u32 index = 0; index < image->FunctionCount; index++) {
        Guest_Register(image->Functions[index].Address, image->Functions[index].Run);
    }
    for (u32 index = 0; index < image->ImportCount; index++) {
        const char* name = image->Imports[index].Name;
        const char* symbol = strchr(name, '!') + 1;
        u32 address;
        if (library && strlen(library) == static_cast<usize>(symbol - 1 - name) && !_strnicmp(name, library,
            strlen(library))) {
            address = Export(provider, symbol);
        }
        else {
            address = sThunkBase + sThunkCount++ * 4;
            GuestFunction* function = Import_Find(name);
            if (!function && sMissingCount < 128) {
                sMissing[sMissingCount] = name;
                function = Missing_Stub(sMissingCount++, std::make_index_sequence<128>());
            }
            Guest_Register(address, function);
        }
        image->ImportFunctions[index] = Guest_Lookup(address);
        if (!image->ImportFunctions[index]) {
            Fatal("cannot bind import %s", name);
        }
        Write(image->Imports[index].Address, address);
    }
    for (u32 index = 0; index < image->RegionCount; index++) {
        const ImageRegion* region = &image->Regions[index];
        if (!region->Writable) {
            Memory_Map(region->Address, region->Size, false);
        }
    }
}

#ifdef _WIN32
static DWORD WINAPI Thread(void* function) {
    reinterpret_cast<void (*)()>(function)();
    return 0;
}
#else
static void* Thread(void* function) {
    reinterpret_cast<void (*)()>(function)();
    return nullptr;
}
#endif

void Host_Run(void (*function)()) {
    // Each guest call is a host call, and host frames are larger than the guest's.
    constexpr usize stack = usize{256} << 20;
#ifdef _WIN32
    HANDLE thread = CreateThread(nullptr, stack, Thread, reinterpret_cast<void*>(function),
        STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (!thread) {
        Fatal("cannot start the guest thread");
    }
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_attr_t attributes;
    pthread_t thread;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, stack);
    int result = pthread_create(&thread, &attributes, Thread, reinterpret_cast<void*>(function));
    pthread_attr_destroy(&attributes);
    if (result != 0) {
        Fatal("cannot start the guest thread");
    }
    pthread_join(thread, nullptr);
#endif
}
