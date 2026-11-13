// Guest paths: translation to host paths as wibo does it, and the path and module queries.
#include "win32.h"

#include <algorithm>
#include <unordered_map>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace Filesystem = std::filesystem;

static bool Equal_Case(const std::string& first, const std::string& second) {
    return first.size() == second.size() && std::equal(first.begin(), first.end(), second.begin(),
        [](u8 left, u8 right) { return tolower(left) == tolower(right); });
}

static std::string Join(const std::string& directory, const std::string& name) {
    bool separated = directory.empty() || directory.back() == '/';
#ifdef _WIN32
    // "C:name" is relative to the current directory of drive C.
    separated = separated || directory.back() == ':';
#endif
    return separated ? directory + name : directory + '/' + name;
}

// How a directory spells the entry `name`, which may differ in case; empty if it has none.
// The result is the same on every host, so paths the compiler records are too.
static std::string Disk_Spelling(const std::string& directory, const std::string& name) {
#ifdef _WIN32
    // The file system ignores case, so asking for the name finds the entry.
    WIN32_FIND_DATAA entry;
    HANDLE search = name.find_first_of("*?") == std::string::npos ?
        FindFirstFileA(Join(directory, name).c_str(), &entry) : INVALID_HANDLE_VALUE;
    if (search == INVALID_HANDLE_VALUE) {
        return "";
    }
    FindClose(search);
    return Equal_Case(entry.cFileName, name) ? entry.cFileName : "";
#else
    std::error_code error;
    if (Filesystem::exists(Join(directory, name), error)) {
        return name;
    }
    // Otherwise the first entry, in directory order, that matches ignoring case. Include
    // searches miss often, so each listing is kept until the directory's time stamp changes.
    struct Listing {
        timespec Modified;
        std::unordered_map<std::string, std::string> Names; // lowercase -> first spelling
    };
    static std::unordered_map<std::string, Listing> listings;
    auto lower = [](std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](u8 character) { return tolower(character); });
        return text;
    };
    struct stat status;
    if (stat(directory.c_str(), &status) != 0) {
        return "";
    }
    Listing& listing = listings[directory];
    if (listing.Modified.tv_sec != status.st_mtim.tv_sec || listing.Modified.tv_nsec != status.st_mtim.tv_nsec) {
        listing = {status.st_mtim, {}};
        for (Filesystem::directory_iterator entry(directory, error); !error
            && entry != Filesystem::directory_iterator();
            entry.increment(error)) {
            std::string spelling = entry->path().filename().string();
            listing.Names.emplace(lower(spelling), spelling);
        }
    }
    auto found = listing.Names.find(lower(name));
    return found == listing.Names.end() ? "" : found->second;
#endif
}

Filesystem::path Path_From_Windows(const std::string& input) {
    std::string text = input;
    std::replace(text.begin(), text.end(), '\\', '/');
    if (text.compare(0, 4, "//?/") == 0) {
        text.erase(0, 4);
    }
#ifndef _WIN32
    if (text.size() >= 3 && text[1] == ':' && text[2] == '/' &&
        (text[0] == 'c' || text[0] == 'C' || text[0] == 'z' || text[0] == 'Z')) {
        text.erase(0, 2);
    }
#endif
    std::string stripped;
    for (usize start = 0; start < text.size();) {
        usize separator = text.find('/', start);
        usize end = separator == std::string::npos ? text.size() : separator;
        usize length = end - start;
        bool dots = (length == 1 && text[start] == '.') ||
            (length == 2 && text[start] == '.' && text[start + 1] == '.');
        if (!dots) {
            while (end > start && text[end - 1] == '.') {
                end--;
            }
        }
        stripped.append(text, start, end - start);
        if (separator == std::string::npos) {
            break;
        }
        stripped += '/';
        start = separator + 1;
    }
    Filesystem::path path = Filesystem::path(stripped).lexically_normal();
#ifndef _WIN32
    // Like wibo, look for other spellings only when the exact path is absent.
    std::error_code error;
    if (Filesystem::exists(path, error)) {
        return path;
    }
#endif
    // Resolve one component at a time, as text: path objects convert encodings on Windows.
    // Directories are assumed to stay put while the compiler runs, so their spellings are
    // remembered; the final component is looked up every time.
    static std::unordered_map<std::string, std::string> spellings;
    std::string root = path.root_path().generic_string();
    std::string rest = path.relative_path().generic_string();
    std::string resolved = root.empty() ? "." : root;
    bool existing = true;
    for (usize start = 0; start <= rest.size() && !rest.empty();) {
        usize end = rest.find('/', start);
        bool last = end == std::string::npos;
        std::string name = rest.substr(start, last ? end : end - start);
        if (existing && !name.empty() && name != "." && name != "..") {
            std::string& known = spellings[Join(resolved, name)];
            std::string spelling = last || known.empty() ? Disk_Spelling(resolved, name) : known;
            existing = !spelling.empty();
            if (existing) {
                name = spelling;
                if (!last) {
                    known = spelling;
                }
            }
        }
        resolved = Join(resolved, name);
        start = last ? rest.size() + 1 : end + 1;
    }
    return resolved;
}

std::string Path_To_Windows(const Filesystem::path& source) {
    Filesystem::path path = source.lexically_normal();
    std::string text = path.generic_string();
#ifndef _WIN32
    if (path.is_absolute()) {
        text = "Z:" + text;
    }
#endif
    std::replace(text.begin(), text.end(), '/', '\\');
    return text;
}

u32 Path_Error(const std::error_code& error) {
    if (error == std::errc::permission_denied) {
        return 5;
    }
    if (error == std::errc::no_such_file_or_directory) {
        return 2;
    }
    if (error == std::errc::not_a_directory) {
        return 3;
    }
    if (error == std::errc::not_enough_memory) {
        return 8;
    }
    if (error == std::errc::invalid_argument) {
        return 87;
    }
    return 50;
}

// Copies a string result, following the Win32 convention: the length when it fits, else
// the required size including the terminator with ERROR_INSUFFICIENT_BUFFER.
static u32 Copy_Out(const std::string& text, u32 buffer, u32 capacity) {
    u32 required = static_cast<u32>(text.size() + 1);
    if (capacity && !buffer) {
        return Fail(87);
    }
    if (capacity < required) {
        return Fail(122, required);
    }
    memcpy(gMemory + buffer, text.c_str(), required);
    return required - 1;
}

u32 KERNEL32_GetFullPathNameA(u32 name, u32 capacity, u32 buffer, u32 file_part) {
    if (file_part) {
        Write(file_part, 0);
    }
    if (!name) {
        return Fail(87);
    }
    std::string input = Text(name);
    Filesystem::path source = Path_From_Windows(input);
    std::error_code error;
    Filesystem::path absolute = Filesystem::absolute(source, error);
    if (error) {
        return Fail(Path_Error(error));
    }
    std::string path = Path_To_Windows(absolute);
    bool directory = source.filename() == "." || source.filename().empty() ||
        (!input.empty() && (input.back() == '/' || input.back() == '\\'));
    if (directory && path.back() != '\\') {
        path += '\\';
    }
    u32 length = Copy_Out(path, buffer, capacity);
    usize part = path.find_last_of('\\') + 1;
    if (length == path.size() && file_part && part < path.size()) {
        Write(file_part, buffer + static_cast<u32>(part));
    }
    return length;
}

u32 KERNEL32_GetCurrentDirectoryA(u32 capacity, u32 buffer) {
    std::error_code error;
    Filesystem::path directory = Filesystem::current_path(error);
    if (error) {
        return Fail(Path_Error(error));
    }
    std::string path = Path_To_Windows(directory);
    return capacity ? Copy_Out(path, buffer, capacity) : static_cast<u32>(path.size() + 1);
}

u32 KERNEL32_GetFileAttributesA(u32 name) {
    if (!name) {
        return Fail(87, 0xffffffff);
    }
    Filesystem::path path = Path_From_Windows(Text(name));
    // wibo reports the compiler's license file as present.
    if (path.filename() == "license.dat" && path.has_parent_path()) {
        return 0x80;
    }
    std::error_code error;
    Filesystem::file_status status = Filesystem::status(path, error);
    if (error) {
        return Fail(Path_Error(error), 0xffffffff);
    }
    return Filesystem::is_regular_file(status) ? 0x80 : Filesystem::is_directory(status) ? 0x10 : Fail(2, 0xffffffff);
}

static std::string Base_Name(const std::string& name) {
    return name.substr(name.find_last_of("/\\") + 1);
}

u32 KERNEL32_GetModuleHandleA(u32 name) {
    std::string input = name ? Text(name) : "";
    std::string base = Base_Name(input);
    if (input.empty() || Equal_Case(base, Base_Name(gProcess.ExecutablePath))) {
        return gProcess.Main->Base;
    }
    if (base.find('.') == std::string::npos) {
        base += ".dll";
    }
    if (gProcess.License && Equal_Case(base, "LMGR326B.dll")) {
        return gProcess.License->Base;
    }
    for (u32 index = 0; index < gProcess.Main->ImportCount; index++) {
        const char* import = gProcess.Main->Imports[index].Name;
        if (Equal_Case(base, std::string(import, strchr(import, '!')))) {
            Fatal("GetModuleHandleA(%s) is unsupported", base.c_str());
        }
    }
    return Fail(126);
}

u32 KERNEL32_GetModuleFileNameA(u32 module, u32 buffer, u32 capacity) {
    bool license = module && gProcess.License && module == gProcess.License->Base;
    if (!buffer || (module && module != gProcess.Main->Base && !license)) {
        return Fail(87);
    }
    if (!capacity) {
        return Fail(122);
    }
    std::error_code error;
    Filesystem::path absolute = Filesystem::absolute(gProcess.ExecutablePath, error);
    if (error) {
        return Fail(Path_Error(error));
    }
    if (license) {
        absolute = absolute.parent_path() / "LMGR326B.dll";
    }
    std::string path = Path_To_Windows(absolute);
    usize length = path.size() < capacity ? path.size() : capacity - 1;
    memcpy(gMemory + buffer, path.data(), length);
    gMemory[buffer + length] = 0;
    return length < path.size() ? Fail(122, capacity) : static_cast<u32>(length);
}
