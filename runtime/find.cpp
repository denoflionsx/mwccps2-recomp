// FindFirstFileA, FindNextFileA and FindClose.
#include "win32.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace Filesystem = std::filesystem;

struct Entry {
    std::string Name;
    std::string Path;
    bool Directory;
};

struct Search {
    std::vector<Entry> Entries;
    usize Next;
};

static std::map<u32, Search> sSearches;
static u32 sLastHandle;

// Case-insensitive match with `*` and `?`.
static bool Matches(const std::string& pattern, const std::string& text) {
    usize pattern_index = 0;
    usize text_index = 0;
    usize star = std::string::npos;
    usize restart = 0;
    while (text_index < text.size()) {
        if (pattern_index < pattern.size() && pattern[pattern_index] == '*') {
            star = pattern_index++;
            restart = text_index;
        }
        else if (pattern_index < pattern.size() && (pattern[pattern_index] == '?' ||
            tolower(static_cast<u8>(pattern[pattern_index])) == tolower(static_cast<u8>(text[text_index])))) {
            pattern_index++;
            text_index++;
        }
        else if (star != std::string::npos) {
            pattern_index = star + 1;
            text_index = ++restart;
        }
        else {
            return false;
        }
    }
    while (pattern_index < pattern.size() && pattern[pattern_index] == '*') {
        pattern_index++;
    }
    return pattern_index == pattern.size();
}

static Filesystem::path Canonical(const Filesystem::path& path) {
    std::error_code error;
    Filesystem::path canonical = Filesystem::weakly_canonical(path, error);
    return error ? path : canonical;
}

// Fills a WIN32_FIND_DATAA.
static void Populate(const Entry& entry, u32 buffer) {
    u32 attributes = entry.Directory ? 0x10 : 0x80;
    u64 size = 0;
    u64 times[3] = {116444736000000000ull, 116444736000000000ull, 116444736000000000ull};
#ifdef _WIN32
    HANDLE file = CreateFileA(entry.Path.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        FILE_BASIC_INFO basic;
        FILE_STANDARD_INFO standard;
        if (GetFileInformationByHandleEx(file, FileBasicInfo, &basic, sizeof(basic)) &&
            GetFileInformationByHandleEx(file, FileStandardInfo, &standard, sizeof(standard))) {
            attributes = standard.Directory || entry.Directory ? 0x10 : 0x20;
            attributes |= basic.FileAttributes & FILE_ATTRIBUTE_READONLY ? 1 : 0;
            size = attributes & 0x10 ? 0 : static_cast<u64>(standard.EndOfFile.QuadPart);
            times[0] = static_cast<u64>(basic.ChangeTime.QuadPart);
            times[1] = static_cast<u64>(basic.LastAccessTime.QuadPart);
            times[2] = static_cast<u64>(basic.LastWriteTime.QuadPart);
        }
        CloseHandle(file);
    }
#else
    struct stat status;
    if (stat(entry.Path.c_str(), &status) == 0) {
        attributes = S_ISDIR(status.st_mode) || entry.Directory ? 0x10 : 0;
        if (S_ISREG(status.st_mode) && !entry.Directory) {
            attributes |= 0x20;
            size = static_cast<u64>(status.st_size);
        }
        attributes |= status.st_mode & S_IWUSR ? 0 : 1;
        if (!attributes) {
            attributes = 0x80;
        }
        const timespec* parts[] = {&status.st_ctim, &status.st_atim, &status.st_mtim};
        for (u32 index = 0; index < 3; index++) {
            s64 seconds = parts[index]->tv_sec + 11644473600ll;
            times[index] = static_cast<u64>(seconds < 0 ? 0 : seconds) * 10000000 +
                static_cast<u64>(parts[index]->tv_nsec > 0 ? parts[index]->tv_nsec / 100 : 0);
        }
    }
#endif
    memset(gMemory + buffer, 0, 320);
    Write(buffer, attributes);
    memcpy(gMemory + buffer + 4, times, sizeof(times));
    Write(buffer + 28, static_cast<u32>(size >> 32));
    Write(buffer + 32, static_cast<u32>(size));
    memcpy(gMemory + buffer + 44, entry.Name.c_str(), entry.Name.size() < 259 ? entry.Name.size() : 259);
}

u32 KERNEL32_FindFirstFileA(u32 name, u32 buffer) {
    if (!buffer || !name) {
        return Fail(buffer ? 3 : 87, 0xffffffff);
    }
    std::string input = Text(name);
    if (input.empty()) {
        return Fail(3, 0xffffffff);
    }
    std::replace(input.begin(), input.end(), '/', '\\');
    if (input.back() == '\\') {
        return Fail(2, 0xffffffff);
    }
    usize separator = input.find_last_of('\\');
    std::string directory = separator == std::string::npos ? "." : input.substr(0, separator);
    std::string pattern = input.substr(separator + 1);
    if (directory.empty() || (separator == 2 && input[1] == ':')) {
        directory += '\\';
    }
    if (directory.find_first_of("*?", directory.compare(0, 4, "\\\\?\\") == 0 ? 4 : 0) != std::string::npos) {
        return Fail(123, 0xffffffff);
    }
    Filesystem::path host = Canonical(Path_From_Windows(directory));
    std::error_code error;
    Filesystem::file_status status = Filesystem::status(host, error);
    if (error || !Filesystem::is_directory(status)) {
        return Fail(error ? Path_Error(error) : 3, 0xffffffff);
    }
    Search search = {};
    // `path` must be canonical already.
    auto add = [&search](const std::string& entry_name, const Filesystem::path& path, bool is_directory) {
        search.Entries.push_back({entry_name, path.string(), is_directory});
    };
    if (pattern.find_first_of("*?") == std::string::npos) {
        host = Canonical(Path_From_Windows(input));
        status = Filesystem::status(host, error);
        if (error || !Filesystem::exists(status)) {
            return Fail(error ? Path_Error(error) : 2, 0xffffffff);
        }
        std::string display = host.filename().string();
        add(display.empty() ? pattern : display, host, Filesystem::is_directory(status));
    }
    else {
        if (Matches(pattern, ".")) {
            add(".", host, true);
        }
        if (Matches(pattern, "..")) {
            add("..", host.parent_path().empty() ? host : Canonical(host.parent_path()), true);
        }
        for (Filesystem::directory_iterator iterator(host, error); !error
            && iterator != Filesystem::directory_iterator();
            iterator.increment(error)) {
            std::string entry_name = iterator->path().filename().string();
            if (Matches(pattern, entry_name)) {
                std::error_code status_error;
                bool is_directory = iterator->is_directory(status_error);
                add(entry_name, Canonical(iterator->path()), is_directory && !status_error);
            }
        }
    }
    if (error || search.Entries.empty()) {
        return Fail(error ? Path_Error(error) : 2, 0xffffffff);
    }
    Populate(search.Entries[0], buffer);
    search.Next = 1;
    sSearches[++sLastHandle] = std::move(search);
    return Fail(0, sLastHandle);
}

u32 KERNEL32_FindNextFileA(u32 handle, u32 buffer) {
    if (!buffer) {
        return Fail(87);
    }
    auto search = sSearches.find(handle);
    if (search == sSearches.end()) {
        return Fail(6);
    }
    if (search->second.Next == search->second.Entries.size()) {
        return Fail(18);
    }
    Populate(search->second.Entries[search->second.Next++], buffer);
    return 1;
}

u32 KERNEL32_FindClose(u32 handle) {
    return sSearches.erase(handle) ? 1 : Fail(6);
}
