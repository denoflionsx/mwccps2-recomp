// mwccps2: the recompiled compiler and its license library as one native executable.
#include "patches.h"
#include "process.h"
#include "win32.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
extern char** environ;
#endif

extern const Image Compiler_Image;
extern const Image License_Image;

// Applies NAME=VALUE lines from mwrecomp.env beside the executable. `#` begins a comment.
static void Load_Environment(const std::filesystem::path& directory) {
    FILE* file = fopen((directory / "mwrecomp.env").string().c_str(), "rb");
    if (!file) {
        if (errno != ENOENT) {
            Fatal("cannot load mwrecomp.env");
        }
        return;
    }
    std::string line;
    for (s32 character = 0; character != EOF;) {
        character = fgetc(file);
        if (character != EOF && character != '\r' && character != '\n' && character != 0) {
            line += static_cast<char>(character);
            continue;
        }
        usize separator = line.find('=');
        if (line[0] != '#' && separator != std::string::npos && separator != 0) {
#ifdef _WIN32
            _putenv_s(line.substr(0, separator).c_str(), line.c_str() + separator + 1);
#else
            setenv(line.substr(0, separator).c_str(), line.c_str() + separator + 1, 1);
#endif
        }
        line.clear();
    }
    fclose(file);
}

// The license library's DllMain, with process attach or detach as the reason.
static bool License_Main(bool attach) {
    return Guest_Call(License_Image.Entry, {License_Image.Base, attach ? 1u : 0u, attach ? 0u : 1u}) &&
        (!attach || cpu.eax);
}

static void Run() {
    // The library starts on the compiler's stack, and what it leaves there stays.
    CPU initial = cpu;
    if (!License_Main(true)) {
        Fatal("the license library failed to start");
    }
    cpu = initial;
    // The entry point has no caller; it leaves through ExitProcess.
    cpu.esp = Guest_Dispatch(gMemory, cpu.esp, Compiler_Image.Entry);
    if (cpu.eip != Guest_Exit) {
        Fatal("jump to unknown address %08x", cpu.eip);
    }
    if (!License_Main(false)) {
        gProcess.ExitCode = 1;
    }
}

int main(int count, char** arguments) {
    char executable[32768];
#ifdef _WIN32
    u32 length = GetModuleFileNameA(nullptr, executable, sizeof(executable));
#else
    ssize length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
#endif
    if (length <= 0 || static_cast<usize>(length) >= sizeof(executable)) {
        Fatal("cannot locate executable");
    }
    executable[length] = 0;
    gProcess.ExecutablePath = executable;
    Load_Environment(std::filesystem::path(executable).parent_path());
    Process_Argument(&gProcess.CommandLine, Path_To_Windows(executable).c_str());
    for (s32 index = 1; index < count; index++) {
        Process_Argument(&gProcess.CommandLine, arguments[index]);
    }
    const char* temporary = getenv("TMPDIR");
    std::error_code error;
    std::string fallback = temporary && *temporary ? "" : std::filesystem::temp_directory_path(error).string();
    if (error) {
        Fatal("cannot locate temporary directory");
    }
#ifdef _WIN32
    char** environment = _environ;
#else
    char** environment = environ;
#endif
    gProcess.Environment = Process_Environment(environment, temporary && *temporary ? temporary : fallback.c_str());
    Process_Create(&Compiler_Image, &License_Image);
    Patches_Install();
    Host_Run(Run);
    return static_cast<s32>(gProcess.ExitCode);
}
