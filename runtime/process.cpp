// Guest process setup: command line, environment, stack, thread block and standard handles.
#include "process.h"
#include "win32.h"

#include <algorithm>

static constexpr u32 sStack = 0x11000000;
static constexpr u32 sStackSize = 0x100000;
static constexpr u32 sThreadBlock = 0x7ffde000;

// Reproduces the pinned wibo's quoting, including doubled trailing backslashes.
void Process_Argument(std::string* command, const char* argument) {
    if (!command->empty()) {
        *command += ' ';
    }
    bool quotes = strpbrk(argument, "\" \t\n") != nullptr;
    if (quotes) {
        *command += '"';
    }
    usize slashes = 0;
    for (const char* cursor = argument;; cursor++) {
        char character = *cursor;
        if (character == '\\') {
            slashes++;
            continue;
        }
        // Backslashes are literal unless they precede a quote.
        command->append(slashes * (!character || character == '"' ? 2 : 1), '\\');
        slashes = 0;
        if (!character) {
            break;
        }
        if (character == '"') {
            *command += '\\';
        }
        *command += character;
    }
    if (quotes) {
        *command += '"';
    }
}

static std::string Convert_Path(const std::string& input) {
    if (input.find('\\') != std::string::npos || (input.size() >= 2 && input[1] == ':' && input[0] != '/')) {
        std::string output = input;
        std::replace(output.begin(), output.end(), '/', '\\');
        return output;
    }
    return Path_To_Windows(input);
}

// Builds the environment block as wibo does: PATH, TEMP and TMP become Windows paths, and
// TEMP and TMP default to `temporary`.
std::string Process_Environment(char* const* entries, const char* temporary) {
    std::string block;
    bool has_temp = false;
    bool has_tmp = false;
    for (usize index = 0; entries && entries[index]; index++) {
        std::string entry = entries[index];
        usize equals = entry.find('=');
        std::string name = entry.substr(0, equals);
        std::transform(name.begin(), name.end(), name.begin(), [](u8 character) { return toupper(character); });
        has_temp = has_temp || name == "TEMP";
        has_tmp = has_tmp || name == "TMP";
        if (equals != std::string::npos && (name == "PATH" || name == "TEMP" || name == "TMP")) {
            std::string value = entry.substr(equals + 1);
            std::string converted;
            if (name == "PATH") {
                char delimiter = value.find(';') != std::string::npos ? ';' : ':';
                for (usize start = 0;;) {
                    usize end = value.find(delimiter, start);
                    std::string part = value.substr(start, end == std::string::npos ? end : end - start);
                    converted += (start ? ";" : "") + (part.empty() ? part : Convert_Path(part));
                    if (end == std::string::npos) {
                        break;
                    }
                    start = end + 1;
                }
                if (converted.empty()) {
                    converted = value;
                }
            }
            else {
                converted = Convert_Path(value);
            }
            entry = entry.substr(0, equals + 1) + converted;
        }
        block += entry + '\0';
    }
    std::string path = Convert_Path(temporary);
    if (!has_tmp) {
        block += "TMP=" + path + '\0';
    }
    if (!has_temp) {
        block += "TEMP=" + path + '\0';
    }
    return block + '\0';
}

void Process_Create(const Image* main, const Image* license) {
    if (!Memory_Create()) {
        Fatal("cannot reserve guest memory");
    }
    gProcess.Main = main;
    gProcess.License = license;
    if (license) {
        Image_Load(license);
    }
    Image_Load(main, "LMGR326B.dll", license);
    if (!Memory_Map(sStack, sStackSize) || !Memory_Map(sThreadBlock, Page_Size)) {
        Fatal("cannot map the guest stack");
    }
    cpu = {};
    X87_Initialize();
    cpu.fs = sThreadBlock;
    cpu.esp = sStack + sStackSize - 16;
    Write(cpu.fs, 0xffffffff);  // no exception handlers
    Write(cpu.fs + 0x24, 1);    // thread identifier
    for (u32 index = 0; index < 3; index++) {
        gProcess.StandardHandles[index] = (index + 1) * 4;
        if (!File_Attach(gProcess.StandardHandles[index], static_cast<s32>(index), index != 0)) {
            Fatal("cannot open the standard handles");
        }
    }
}
