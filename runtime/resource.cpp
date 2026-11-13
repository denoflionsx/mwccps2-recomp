// Resources and version information, read from the mapped main image.
#include "win32.h"

static u32 Read16(const u8* data) {
    return data[0] | (u32{data[1]} << 8);
}

static u32 Read32(const u8* data) {
    return Read16(data) | (Read16(data + 2) << 16);
}

// Whether a resource directory key names `identifier`: an integer below 0x10000 or a string.
static bool Matches(const u8* data, u32 size, u32 key, u32 identifier) {
    if (identifier <= 0xffff) {
        return !(key & 0x80000000) && (key & 0xffff) == identifier;
    }
    u32 offset = key & 0x7fffffff;
    const char* text = Text(identifier);
    if (!(key & 0x80000000) || u64{offset} + 2 > size) {
        return false;
    }
    u32 length = Read16(data + offset);
    if (u64{offset} + 2 + length * 2 > size || length != strlen(text)) {
        return false;
    }
    for (u32 index = 0; index < length; index++) {
        if (Read16(data + offset + 2 + index * 2) != static_cast<u8>(text[index])) {
            return false;
        }
    }
    return true;
}

// Walks type, name, then language, preferring the neutral language.
u32 KERNEL32_FindResourceA(u32 module, u32 name, u32 type) {
    const Image* image = gProcess.Main;
    if ((module && module != image->Base) || !image->ResourceSize) {
        return Fail(1812);
    }
    const u8* data = gMemory + image->ResourceAddress;
    u32 size = image->ResourceSize;
    u32 offset = 0;
    for (u32 depth = 0; depth < 3; depth++) {
        if (u64{offset} + 16 > size) {
            return Fail(1812);
        }
        u32 count = Read16(data + offset + 12) + Read16(data + offset + 14);
        if (u64{offset} + 16 + count * 8 > size) {
            return Fail(1812);
        }
        bool found = false;
        u32 target = 0;
        for (u32 index = 0; index < count; index++) {
            u32 entry = offset + 16 + index * 8;
            u32 key = Read32(data + entry);
            bool matches = depth == 2 ? !(key & 0x80000000) : Matches(data, size, key, depth == 0 ? type : name);
            if (matches && (!found || (depth == 2 && (key & 0xffff) == 0))) {
                target = Read32(data + entry + 4);
                found = true;
                if (depth < 2 || (key & 0xffff) == 0) {
                    break;
                }
            }
        }
        if (!found) {
            return Fail(1813 + depth);
        }
        if (bool(target & 0x80000000) != (depth < 2)) {
            return Fail(1812);
        }
        offset = target & 0x7fffffff;
    }
    return u64{offset} + 16 > size ? Fail(1812) : image->ResourceAddress + offset;
}

// A resource handle is the address of its data entry: the data's RVA, then its size.
static u32 Resource_Field(u32 module, u32 entry, u32 field) {
    const Image* image = gProcess.Main;
    if (!entry || (module && module != image->Base) || !image->ResourceSize) {
        return Fail(1812);
    }
    if (entry < image->ResourceAddress || u64{entry} + 16 > u64{image->ResourceAddress} + image->ResourceSize) {
        return Fail(87);
    }
    return Read(entry + field);
}

u32 KERNEL32_LoadResource(u32 module, u32 entry) {
    u32 rva = Resource_Field(module, entry, 0);
    return rva ? rva + gProcess.Main->Base : 0;
}

u32 KERNEL32_SizeofResource(u32 module, u32 entry) {
    return Resource_Field(module, entry, 4);
}

u32 KERNEL32_LockResource(u32 data) {
    return data;
}

// Only the running executable has version information here.
static void Check_Executable(u32 name) {
    std::error_code error;
    if (!std::filesystem::equivalent(Path_From_Windows(Text(name)), gProcess.ExecutablePath, error) || error) {
        Fatal("version information of %s is unavailable", Text(name));
    }
}

u32 VERSION_GetFileVersionInfoSizeA(u32 name, u32 handle) {
    if (handle) {
        Write(handle, 0);
    }
    if (!name) {
        return Fail(87);
    }
    Check_Executable(name);
    return gProcess.Main->VersionSize;
}

u32 VERSION_GetFileVersionInfoA(u32 name, u32, u32 size, u32 buffer) {
    const Image* image = gProcess.Main;
    if (!buffer || !size || !name) {
        return Fail(87);
    }
    Check_Executable(name);
    if (size < image->VersionSize) {
        return Fail(122);
    }
    if (!image->VersionSize) {
        return 0;
    }
    memmove(gMemory + buffer, gMemory + image->VersionAddress, image->VersionSize);
    memset(gMemory + buffer + image->VersionSize, 0, size - image->VersionSize);
    return 1;
}

// One node of a version resource: a header, a UTF-16 key, a value, then child nodes.
struct VersionBlock {
    u32 Size;
    u32 Length;
    u32 Type; // 1 when the value is text
    u32 KeySize;
    u32 Value;
    u32 ValueSize;
    u32 Children;
};

static bool Parse(const u8* data, u32 available, VersionBlock* block) {
    if (available < 6) {
        return false;
    }
    block->Size = Read16(data);
    block->Length = Read16(data + 2);
    block->Type = Read16(data + 4);
    if (block->Size < 6 || block->Size > available) {
        return false;
    }
    u32 offset = 6;
    while (offset + 2 <= block->Size && Read16(data + offset)) {
        offset += 2;
    }
    if (offset + 2 > block->Size) {
        return false;
    }
    block->KeySize = (offset - 6) / 2;
    block->Value = (offset + 5) & ~3u;
    block->ValueSize = block->Length * (block->Type == 1 ? 2 : 1);
    if (block->ValueSize && block->Value + block->ValueSize > block->Size) {
        return false;
    }
    block->Children = (block->Value + block->ValueSize + 3) & ~3u;
    if (block->Children > block->Size) {
        block->Children = block->Size;
    }
    return true;
}

u32 VERSION_VerQueryValueA(u32 block, u32 query, u32 buffer, u32 length) {
    if (!block || !query) {
        return 0;
    }
    u32 size = Read<u16>(block);
    const char* text = Text(query);
    if (*text == '\\') {
        text++;
    }
    u32 offset = 0;
    VersionBlock view;
    if (!Parse(gMemory + block, size, &view)) {
        return 0;
    }
    while (*text) {
        const char* separator = strchr(text, '\\');
        usize key_size = separator ? static_cast<usize>(separator - text) : strlen(text);
        bool found = false;
        for (u32 child = view.Children; !found && child + 6 <= view.Size;) {
            const u8* data = gMemory + block + offset + child;
            VersionBlock entry;
            if (!Parse(data, view.Size - child, &entry)) {
                break;
            }
            found = entry.KeySize == key_size;
            for (u32 index = 0; found && index < entry.KeySize; index++) {
                found = tolower(data[6 + index * 2]) == tolower(static_cast<u8>(text[index]));
            }
            if (found) {
                offset += child;
                view = entry;
            }
            child += (entry.Size + 3) & ~3u;
        }
        if (!found) {
            return 0;
        }
        text += key_size;
        if (*text) {
            text++;
        }
    }
    u32 value = view.ValueSize ? block + offset + view.Value : 0;
    if (view.Type == 1 && value) {
        // Text values are narrowed in place, as wibo does.
        for (u32 index = 0; index < view.Length; index++) {
            gMemory[value + index] = gMemory[value + index * 2];
        }
        gMemory[value + view.Length] = 0;
    }
    if (buffer) {
        Write(buffer, value);
    }
    if (length) {
        Write(length, view.Type == 1 ? view.Length : view.ValueSize);
    }
    return 1;
}
