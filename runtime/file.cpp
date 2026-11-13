// File handles over host descriptors, with the positions, errors and pipe behaviour of the
// reference environment (wibo).
#include "win32.h"

#include <errno.h>
#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#define _close close
#define _lseeki64 lseek
#define _unlink unlink
#endif

struct File {
    s32 Descriptor;
    s64 Position; // each handle keeps its own, as descriptors may be shared
    bool Pipe;
    bool Append;
};

static std::map<u32, File> sFiles;

static File* Find(u32 handle) {
    auto file = sFiles.find(handle);
    return file == sFiles.end() ? nullptr : &file->second;
}

bool File_Attach(u32 handle, s32 descriptor, bool append) {
#ifdef _WIN32
    s32 duplicate = _dup(descriptor);
    if (duplicate < 0 || _setmode(duplicate, _O_BINARY) < 0) {
        return false;
    }
    // The CRT can report a successful seek on a pipe.
    u32 type = GetFileType(reinterpret_cast<HANDLE>(_get_osfhandle(duplicate)));
    bool pipe = type == FILE_TYPE_PIPE || type == FILE_TYPE_CHAR;
#else
    // wibo reports broken pipes as errors rather than terminating.
    signal(SIGPIPE, SIG_IGN);
    s32 duplicate = dup(descriptor);
    if (duplicate < 0) {
        return false;
    }
    bool pipe = lseek(duplicate, 0, SEEK_CUR) < 0 && errno == ESPIPE;
#endif
    s64 position = _lseeki64(duplicate, 0, SEEK_CUR);
    sFiles[handle] = {duplicate, position >= 0 ? position : 0, pipe, append};
    return true;
}

static u32 File_Error(s32 error) {
    switch (error) {
    case EACCES:
        return 5;
    case EEXIST:
        return 183;
    case ENOENT:
        return 2;
    case ENOTDIR:
        return 3;
    case ENOMEM:
        return 8;
    case ENOSPC:
        return 112;
    case EINVAL:
    case ESPIPE:
        return 87;
    case EINTR:
        return 995;
    case EIO:
        return 30;
    case EPIPE:
        return 109;
    default:
        return 50;
    }
}

// wibo ignores the input high word and treats unknown methods as FILE_BEGIN at zero.
u32 KERNEL32_SetFilePointer(u32 handle, u32 distance, u32 high, u32 method) {
    File* file = Find(handle);
    if (!file) {
        return Fail(6, 0xffffffff);
    }
    s64 offset = static_cast<s32>(distance);
    s64 position = method == 0 ? offset : method == 1 ? file->Position + offset :
        method == 2 ? _lseeki64(file->Descriptor, offset, SEEK_END) : 0;
    if (position < 0) {
        return Fail(errno == EINVAL ? 131 : 87, 0xffffffff);
    }
    file->Position = position;
    if (high) {
        Write(high, static_cast<u32>(position >> 32));
    }
    return static_cast<u32>(position);
}

// wibo leaves the last error unchanged, even when seeking fails.
u32 KERNEL32_GetFileSize(u32 handle, u32 high) {
    File* file = Find(handle);
    if (!file) {
        return Fail(6, 0xffffffff);
    }
    s64 size = file->Pipe ? -1 : _lseeki64(file->Descriptor, 0, SEEK_END);
    if (high) {
        Write(high, size < 0 ? 0 : static_cast<u32>(size >> 32));
    }
    return size < 0 ? 0xffffffff : static_cast<u32>(size);
}

u32 KERNEL32_GetFileType(u32 handle) {
    File* file = Find(handle);
    if (!file) {
        return Fail(6);
    }
#ifdef _WIN32
    return GetFileType(reinterpret_cast<HANDLE>(_get_osfhandle(file->Descriptor)));
#else
    struct stat status;
    if (fstat(file->Descriptor, &status) != 0) {
        return Fail(File_Error(errno));
    }
    return S_ISCHR(status.st_mode) ? 2 : S_ISFIFO(status.st_mode) || S_ISSOCK(status.st_mode) ? 3 :
        S_ISREG(status.st_mode) || S_ISDIR(status.st_mode) || S_ISBLK(status.st_mode) ? 1 : 0;
#endif
}

u32 KERNEL32_DeleteFileA(u32 name) {
    if (!name) {
        return Fail(87);
    }
    std::filesystem::path path = Path_From_Windows(Text(name));
    std::error_code error;
    if (std::filesystem::is_directory(std::filesystem::symlink_status(path, error))) {
        // wibo maps unlink's EISDIR to ERROR_NOT_SUPPORTED.
        return Fail(50);
    }
    return _unlink(path.string().c_str()) == 0 ? 1 : Fail(File_Error(errno));
}

u32 KERNEL32_GetFileTime(u32 handle, u32 creation, u32 access, u32 write) {
    File* file = Find(handle);
    if (!file) {
        return Fail(6);
    }
    const u32 addresses[] = {creation, access, write};
    u64 times[3];
#ifdef _WIN32
    FILE_BASIC_INFO basic;
    if (!GetFileInformationByHandleEx(reinterpret_cast<HANDLE>(_get_osfhandle(file->Descriptor)),
        FileBasicInfo, &basic, sizeof(basic))) {
        return Fail(GetLastError());
    }
    times[0] = static_cast<u64>(basic.ChangeTime.QuadPart);
    times[1] = static_cast<u64>(basic.LastAccessTime.QuadPart);
    times[2] = static_cast<u64>(basic.LastWriteTime.QuadPart);
#else
    struct stat status;
    if (fstat(file->Descriptor, &status) != 0) {
        return Fail(File_Error(errno));
    }
    const timespec* parts[] = {&status.st_ctim, &status.st_atim, &status.st_mtim};
    for (u32 index = 0; index < 3; index++) {
        s64 seconds = parts[index]->tv_sec;
        u64 fraction = static_cast<u64>(parts[index]->tv_nsec / 100);
        // Reject times outside what a FILETIME can hold.
        if (addresses[index] && (seconds < -11644473600ll || seconds > 1833029933770ll ||
            (seconds == 1833029933770ll && fraction > 9551615))) {
            return Fail(87);
        }
        times[index] = (static_cast<u64>(seconds) + 11644473600ull) * 10000000 + fraction;
    }
#endif
    for (u32 index = 0; index < 3; index++) {
        if (addresses[index]) {
            Write<u64>(addresses[index], times[index]);
        }
    }
    return 1;
}

u32 KERNEL32_CreateFileA(u32 name, u32 access, u32, u32, u32 creation, u32 flags, u32) {
    if (!name || creation < 1 || creation > 5) {
        return Fail(87, 0xffffffff);
    }
    if (flags & (0x40000000 | 0x04000000)) {
        Fatal("overlapped and delete-on-close files are unsupported");
    }
    std::filesystem::path path = Path_From_Windows(Text(name));
    std::error_code error;
    std::filesystem::file_status status = std::filesystem::status(path, error);
    bool exists = !error && std::filesystem::exists(status);
    bool directory = exists && std::filesystem::is_directory(status);
    if ((flags & 0x10) && !directory) {
        return Fail(87, 0xffffffff);
    }
    if (directory) {
        if (flags & 0x02000000) {
            Fatal("directory handles are unsupported");
        }
        return Fail(5, 0xffffffff);
    }
    if (creation == 1 && exists) {
        return Fail(80, 0xffffffff);
    }
    if ((creation == 3 || creation == 5) && !exists) {
        return Fail(2, 0xffffffff);
    }
    // Expand generic rights to the file rights they stand for.
    u32 rights = access & 0x0fffffff;
    rights |= access & 0x80000000 ? 0x120089 : 0;
    rights |= access & 0x40000000 ? 0x120116 : 0;
    rights |= access & 0x20000000 ? 0x1200a0 : 0;
    rights |= access & 0x10000000 ? 0x1f01ff : 0;
    if (rights & ~0x1f01ffu) {
        return Fail(5, 0xffffffff);
    }
    if (!rights) {
        rights = 0x80;
    }
    bool create = creation == 1 || creation == 2 || (creation == 4 && !exists);
    bool truncate = (creation == 2 || creation == 5) && exists;
    bool read = rights & 0xa9;
    bool write = (rights & 0x116) || create || truncate;
    bool append = (rights & 4) && !(rights & 2);
    s32 mode = (read && write ? O_RDWR : write ? O_WRONLY : O_RDONLY) | (create ? O_CREAT : 0) |
        (truncate ? O_TRUNC : 0) | (creation == 1 ? O_EXCL : 0) | (append ? O_APPEND : 0);
#ifdef _WIN32
    s32 descriptor = _open(path.string().c_str(), mode | _O_BINARY | _O_NOINHERIT, _S_IREAD | _S_IWRITE);
#else
    s32 descriptor = open(path.string().c_str(), mode | O_CLOEXEC, 0666);
#endif
    if (descriptor < 0) {
        return Fail(File_Error(errno), 0xffffffff);
    }
    u32 handle = 4;
    while (Find(handle)) {
        handle += 4;
    }
    bool attached = File_Attach(handle, descriptor, append);
    _close(descriptor);
    if (!attached) {
        return Fail(8, 0xffffffff);
    }
    if ((creation == 2 || creation == 4) && exists) {
        Set_Error(183);
    }
    return handle;
}

static u32 Transfer(u32 handle, u32 buffer, u32 size, u32 count, u32 overlapped, bool write) {
    File* file = Find(handle);
    if (!file) {
        return Fail(6);
    }
    if (!count && !overlapped) {
        return Fail(87);
    }
    if (count) {
        Write(count, 0);
    }
    if (overlapped) {
        Fatal("overlapped file access is unsupported");
    }
    u32 transferred = 0;
    s32 error = 0;
    bool stream = file->Pipe || (write && file->Append);
    while (transferred < size) {
        u32 chunk = size - transferred < 0x7fffffff ? size - transferred : 0x7fffffff;
        u8* data = gMemory + buffer + transferred;
        s64 result;
#ifdef _WIN32
        // No pread/pwrite: move the shared descriptor position and put it back.
        s64 previous = stream ? 0 : _lseeki64(file->Descriptor, 0, SEEK_CUR);
        if (!stream && _lseeki64(file->Descriptor, file->Position + transferred, SEEK_SET) < 0) {
            error = errno;
            break;
        }
        result = write ? _write(file->Descriptor, data, chunk) : _read(file->Descriptor, data, chunk);
        s32 transfer_error = errno;
        if (!stream) {
            _lseeki64(file->Descriptor, previous, SEEK_SET);
        }
        errno = transfer_error;
#else
        if (stream) {
            result = write ? ::write(file->Descriptor, data, chunk) : ::read(file->Descriptor, data, chunk);
        }
        else {
            result = write ? pwrite(file->Descriptor, data, chunk, file->Position + transferred) :
                pread(file->Descriptor, data, chunk, file->Position + transferred);
        }
#endif
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            error = errno ? errno : EIO;
            break;
        }
        if (!result) {
            break;
        }
        transferred += static_cast<u32>(result);
        if (!write && file->Pipe) {
            break;
        }
    }
    if (size && stream) {
        s64 position = file->Pipe ? 0 : _lseeki64(file->Descriptor, 0, SEEK_CUR);
        if (position >= 0) {
            file->Position = position;
        }
        else if (!error) {
            error = errno ? errno : EIO;
        }
    }
    else {
        file->Position += transferred;
    }
    if (!write && file->Pipe && size && !transferred && !error) {
        // wibo leaves the already-zeroed count unchanged at pipe EOF.
        return Fail(109);
    }
    Write(count, transferred);
    return error ? Fail(File_Error(error)) : 1;
}

u32 KERNEL32_WriteFile(u32 handle, u32 buffer, u32 size, u32 count, u32 overlapped) {
    return Transfer(handle, buffer, size, count, overlapped, true);
}

u32 KERNEL32_ReadFile(u32 handle, u32 buffer, u32 size, u32 count, u32 overlapped) {
    return Transfer(handle, buffer, size, count, overlapped, false);
}

u32 KERNEL32_CloseHandle(u32 handle) {
    File* file = Find(handle);
    if (!file) {
        return Fail(6);
    }
    _close(file->Descriptor);
    sFiles.erase(handle);
    return 1;
}
