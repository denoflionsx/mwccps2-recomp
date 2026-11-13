// Character set conversion and classification. Like wibo, these ignore code pages: each
// character converts by its low byte, and only ASCII has classes or case.
#include "win32.h"

// Length in characters of a zero-terminated string, including the terminator.
template <typename T> static u32 Length(u32 source) {
    u32 count = 0;
    while (Read<T>(source + count++ * sizeof(T))) {
    }
    return count;
}

template <typename Source, typename Destination>
static u32 Convert(u32 source, u32 count, u32 destination, u32 capacity) {
    if (count == 0xffffffff) {
        count = Length<Source>(source);
    }
    if (!capacity) {
        return count;
    }
    if (capacity < count) {
        return Fail(122);
    }
    for (u32 index = 0; index < count; index++) {
        Write<Destination>(destination + index * sizeof(Destination),
            Read<Source>(source + index * sizeof(Source)) & 0xff);
    }
    return count;
}

static bool Valid(u32 source, u32 count, u32 destination, u32 capacity) {
    return source && count && (count <= 0x7fffffff || count == 0xffffffff) && capacity <= 0x7fffffff &&
        (!capacity || destination);
}

u32 KERNEL32_MultiByteToWideChar(u32, u32, u32 source, u32 count, u32 destination, u32 capacity) {
    return Valid(source, count, destination, capacity) ? Convert<u8, u16>(source, count, destination, capacity)
        : Fail(87);
}

u32 KERNEL32_WideCharToMultiByte(u32, u32, u32 source, u32 count, u32 destination, u32 capacity, u32,
    u32 used_default) {
    if (!Valid(source, count, destination, capacity)) {
        return Fail(87);
    }
    if (used_default) {
        Write(used_default, 0);
    }
    return Convert<u16, u8>(source, count, destination, capacity);
}

u32 KERNEL32_GetStringTypeW(u32 type, u32 source, u32 count, u32 destination) {
    if (type != 1) {
        Fatal("GetStringTypeW type %u is unsupported", type);
    }
    if (!source || !destination) {
        return Fail(87);
    }
    if (count & 0x80000000) {
        count = Length<u16>(source) - 1;
    }
    for (u32 index = 0; index < count; index++) {
        u32 character = Read<u16>(source + index * 2);
        bool upper = character >= 'A' && character <= 'Z';
        bool lower = character >= 'a' && character <= 'z';
        bool digit = character >= '0' && character <= '9';
        bool space = character == ' ' || (character >= 9 && character <= 13);
        bool blank = character == ' ' || character == 9;
        bool hexadecimal = digit || (character >= 'A' && character <= 'F') || (character >= 'a' && character <= 'f');
        bool control = character < 32 || character == 127;
        bool punctuation = character >= 33 && character <= 126 && !upper && !lower && !digit;
        Write<u16>(destination + index * 2, static_cast<u16>(upper | lower << 1 | digit << 2 | space << 3 |
            punctuation << 4 | control << 5 | blank << 6 | hexadecimal << 7 | (upper || lower) << 8));
    }
    return 1;
}

u32 KERNEL32_LCMapStringW(u32, u32 flags, u32 source, u32 count, u32 destination, u32 capacity) {
    if (!source || !count) {
        return Fail(87);
    }
    if (count & 0x80000000) {
        count = Length<u16>(source);
    }
    if (!destination || !capacity) {
        return count;
    }
    if (capacity > 0x7fffffff || capacity < count) {
        return Fail(122);
    }
    if (flags & 0xc00) {
        return Fail(87);
    }
    memmove(gMemory + destination, gMemory + source, usize{count} * 2);
    for (u32 index = 0; index < count; index++) {
        u16 character = Read<u16>(destination + index * 2);
        if ((flags & 0x300) == 0x200 && character >= 'a' && character <= 'z') {
            Write<u16>(destination + index * 2, character - 32);
        }
        else if ((flags & 0x300) == 0x100 && character >= 'A' && character <= 'Z') {
            Write<u16>(destination + index * 2, character + 32);
        }
    }
    return count;
}
