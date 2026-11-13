// VirtualAlloc and VirtualFree over the guest address space, with Windows' 64 KiB reservation
// granularity and wibo's placement.
#include "win32.h"

#include <vector>

struct Region {
    u32 Address;
    u32 Size;
};

static constexpr u32 sLowest = 0x110000;
static constexpr u32 sHighest = 0x7f000000;
static std::vector<Region> sRegions;

static void Reserve(u32 address, u64 size) {
    memset(gPages + address / Page_Size, 1, size / Page_Size);
}

u32 Virtual_Allocate(u32 address, u32 size, u32 flags, u32* result) {
    if (!size) {
        return 87;
    }
    if (flags & (0x200000 | 0x400000 | 0x20000000 | 0x1000000 | 0x80000)) {
        return 50;
    }
    bool commit = flags & 0x1000;
    bool reserve = (flags & 0x2000) || (!address && commit);
    if (!commit && !reserve) {
        return 87;
    }
    u32 start = address & (reserve ? 0xffff0000u : 0xfffff000u);
    u64 end = (u64{address} + size + Page_Size - 1) & ~u64{Page_Size - 1};
    if (end > 0x80000000ull) {
        return address ? 487 : 8;
    }
    u32 length = static_cast<u32>(end - start);
    if (reserve) {
        if (!address) {
            bool top_down = flags & 0x100000;
            if (length > sHighest - sLowest) {
                return 8;
            }
            start = top_down ? (sHighest - length) & 0xffff0000 : sLowest;
            while (!Memory_Available(start, length)) {
                if (top_down ? start <= sLowest : u64{start} + 65536 + length > sHighest) {
                    return 8;
                }
                start += top_down ? 0u - 65536 : 65536;
            }
        }
        else if (!Memory_Available(start, length)) {
            return 487;
        }
        sRegions.push_back({start, length});
        Reserve(start, length);
    }
    else {
        bool reserved = false;
        for (const Region& region : sRegions) {
            reserved = reserved || (start >= region.Address
                && u64{start} + length <= u64{region.Address} + region.Size);
        }
        if (!reserved) {
            return 487;
        }
    }
    if (commit && !Memory_Map(start, length)) {
        Fatal("out of host memory");
    }
    *result = start;
    return 0;
}

u32 Virtual_Free(u32 address, u32 size, u32 flags) {
    if (!address) {
        return 487;
    }
    if (flags & 3) {
        return 50;
    }
    bool release = flags & 0x8000;
    bool decommit = flags & 0x4000;
    if (release == decommit) {
        return 87;
    }
    usize index = 0;
    while (index < sRegions.size() && address - sRegions[index].Address >= sRegions[index].Size) {
        index++;
    }
    if (index == sRegions.size()) {
        return 487;
    }
    Region region = sRegions[index];
    if (release && (address != region.Address || size)) {
        return size ? 87 : 487;
    }
    if (!release && !size && address != region.Address) {
        return 87;
    }
    u32 start = address & 0xfffff000;
    u64 end = size ? (u64{address} + size + Page_Size - 1) & ~u64{Page_Size - 1} : u64{region.Address} + region.Size;
    if (end > u64{region.Address} + region.Size) {
        return 487;
    }
    if (!Memory_Unmap(start, end - start)) {
        return 8;
    }
    if (release) {
        sRegions.erase(sRegions.begin() + index);
    }
    else {
        Reserve(start, end - start);
    }
    return 0;
}

u32 KERNEL32_VirtualAlloc(u32 address, u32 size, u32 flags, u32) {
    if (size && (flags & 0x80000) && !(flags & (0x200000 | 0x400000 | 0x20000000 | 0x1000000))) {
        Fatal("MEM_RESET is unsupported");
    }
    u32 result = 0;
    u32 error = Virtual_Allocate(address, size, flags, &result);
    return error ? Fail(error) : result;
}

u32 KERNEL32_VirtualFree(u32 address, u32 size, u32 flags) {
    u32 error = Virtual_Free(address, size, flags);
    return error ? Fail(error) : 1;
}
