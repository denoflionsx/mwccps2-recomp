// Guest heaps. The reference environment (wibo) allocates with mimalloc from 64 MiB arenas,
// and the compiler's output depends on where blocks land and which bytes they reuse. Each
// arena is therefore mirrored by a host arena that mimalloc manages; a block's guest address
// is its offset in that mirror, and its bytes live in guest memory.
#include "win32.h"

#include <mimalloc.h>
#include <stdlib.h>
#include <unordered_map>

static constexpr u32 sArenaSize = 0x4000000;
static constexpr u32 sLargeBlock = 0x800000; // served by virtual memory instead

struct Arena {
    Arena* Next;
    void* Allocation;
    u8* Mirror;
    mi_arena_id_t Id;
    mi_heap_t* Heap;
    u32 Base;
};

struct Block {
    u8* Mirror; // null for large blocks
    u32 MappedSize;
};

struct Heap {
    Arena* Arenas;
    Arena* Hint;
    std::unordered_map<u32, Block> Blocks;
    u32 Flags;
};

static std::unordered_map<u32, Heap*> sPrivateHeaps; // by handle, the first arena's address
static u32 sNextPrivateHeap = 0x30000000;

static void Arena_Destroy(Arena* arena) {
    if (arena->Heap) {
        mi_heap_destroy(arena->Heap);
    }
    if (arena->Id) {
        mi_arena_unload(arena->Id, nullptr, nullptr, nullptr);
    }
    free(arena->Allocation);
    free(arena);
}

static Arena* Arena_Create(u32 base) {
    if (!Memory_Available(base, sArenaSize)) {
        return nullptr;
    }
    Arena* arena = static_cast<Arena*>(calloc(1, sizeof(Arena)));
    if (!arena) {
        return nullptr;
    }
    arena->Base = base;
    arena->Allocation = calloc(1, usize{sArenaSize} + 65535);
    if (arena->Allocation) {
        upointer aligned = (reinterpret_cast<upointer>(arena->Allocation) + 65535) & ~upointer{65535};
        arena->Mirror = reinterpret_cast<u8*>(aligned);
        if (mi_manage_os_memory_ex(arena->Mirror, sArenaSize, true, true, true, -1, true, &arena->Id)) {
            arena->Heap = mi_heap_new_ex(0, true, arena->Id);
            if (arena->Heap && Memory_Map(base, sArenaSize)) {
                return arena;
            }
        }
    }
    Arena_Destroy(arena);
    return nullptr;
}

static Heap* Heap_Create(u32 base, u32 flags) {
    Arena* arena = Arena_Create(base);
    return arena ? new Heap{arena, arena, {}, flags} : nullptr;
}

static Heap* Process_Heap() {
    static Heap* heap = Heap_Create(0x20000000, 0);
    if (!heap) {
        Fatal("cannot create the process heap");
    }
    return heap;
}

// Large blocks belong to virtual memory and outlive the heap, as in wibo.
static bool Heap_Destroy(Heap* heap) {
    bool success = true;
    while (heap->Arenas) {
        Arena* arena = heap->Arenas;
        heap->Arenas = arena->Next;
        success = Memory_Unmap(arena->Base, sArenaSize) && success;
        Arena_Destroy(arena);
    }
    delete heap;
    return success;
}

static u8* Mirror_Allocate(Arena* arena, void* previous, u32 size, bool zero) {
    if (previous) {
        return static_cast<u8*>(zero ? mi_heap_rezalloc_aligned(arena->Heap, previous, size, 8) :
            mi_heap_realloc_aligned(arena->Heap, previous, size, 8));
    }
    return static_cast<u8*>(zero ? mi_heap_zalloc_aligned(arena->Heap, size, 8) :
        mi_heap_malloc_aligned(arena->Heap, size, 8));
}

// Tries the arena that last succeeded, then the others, then a new arena at the highest
// free address below 0x70000000.
static u8* Mirror_Place(Heap* heap, void* previous, u32 size, bool zero) {
    u8* mirror = Mirror_Allocate(heap->Hint, previous, size, zero);
    Arena** tail = &heap->Arenas;
    for (; !mirror && *tail; tail = &(*tail)->Next) {
        if (*tail != heap->Hint && (mirror = Mirror_Allocate(*tail, previous, size, zero))) {
            heap->Hint = *tail;
        }
    }
    for (u32 base = 0x70000000 - sArenaSize; !mirror && base >= 0x110000; base -= 65536) {
        if (Memory_Available(base, sArenaSize)) {
            Arena* arena = *tail = Arena_Create(base);
            if (arena && (mirror = Mirror_Allocate(arena, previous, size, zero))) {
                heap->Hint = arena;
            }
            break;
        }
    }
    return mirror;
}

static u32 Guest_Address(const Heap* heap, const u8* mirror) {
    for (const Arena* arena = heap->Arenas; arena; arena = arena->Next) {
        upointer offset = reinterpret_cast<upointer>(mirror) - reinterpret_cast<upointer>(arena->Mirror);
        if (offset < sArenaSize) {
            return arena->Base + static_cast<u32>(offset);
        }
    }
    Fatal("heap block outside its arenas");
}

static u32 Usable_Size(const Heap* heap, u32 address) {
    auto block = heap->Blocks.find(address);
    if (block == heap->Blocks.end()) {
        return 0;
    }
    return block->second.Mirror ? static_cast<u32>(mi_usable_size(block->second.Mirror)) : block->second.MappedSize;
}

// Allocator metadata stays outside guest memory, so a reused block keeps its old bytes.
static u32 Allocate(Heap* heap, u32 size, bool zero) {
    u32 address = 0;
    Block block = {};
    if (size >= sLargeBlock) {
        if (Virtual_Allocate(0, size, 0x3000, &address)) {
            return 0;
        }
        block.MappedSize = (size + Page_Size - 1) & ~(Page_Size - 1);
    }
    else {
        block.Mirror = Mirror_Place(heap, nullptr, size, zero);
        if (!block.Mirror) {
            return 0;
        }
        address = Guest_Address(heap, block.Mirror);
    }
    heap->Blocks[address] = block;
    if (zero) {
        memset(gMemory + address, 0, size);
    }
    return address;
}

static bool Free(Heap* heap, u32 address) {
    auto block = heap->Blocks.find(address);
    if (block == heap->Blocks.end()) {
        return !address;
    }
    if (block->second.Mirror) {
        mi_free(block->second.Mirror);
    }
    else if (Virtual_Free(address, 0, 0x8000)) {
        return false;
    }
    heap->Blocks.erase(block);
    return true;
}

static u32 Reallocate(Heap* heap, u32 address, u32 size, bool zero) {
    auto found = heap->Blocks.find(address);
    if (found == heap->Blocks.end()) {
        return 0;
    }
    Block block = found->second;
    u32 old_size = Usable_Size(heap, address);
    if (!block.Mirror || size >= sLargeBlock) {
        u32 result = Allocate(heap, size, zero);
        if (!result) {
            return 0;
        }
        // wibo copies page-rounded sizes when the new block uses virtual memory.
        u32 capacity = size >= sLargeBlock ? Usable_Size(heap, result) : size;
        memmove(gMemory + result, gMemory + address, old_size < capacity ? old_size : capacity);
        Free(heap, address);
        return result;
    }
    u8* mirror = Mirror_Place(heap, block.Mirror, size, zero);
    if (!mirror) {
        return 0;
    }
    u32 result = Guest_Address(heap, mirror);
    if (result != address) {
        memmove(gMemory + result, gMemory + address, old_size < size ? old_size : size);
        if (zero && size > old_size) {
            memset(gMemory + result + old_size, 0, size - old_size);
        }
        if (!size) {
            gMemory[result] = 0;
        }
        heap->Blocks.erase(address);
    }
    heap->Blocks[result] = {mirror, 0};
    return result;
}

u32 Heap_Allocate(u32 size, bool zero) {
    return Allocate(Process_Heap(), size, zero);
}

bool Heap_Free(u32 address) {
    return Free(Process_Heap(), address);
}

u32 KERNEL32_GlobalAlloc(u32 flags, u32 size) {
    if (flags & 2) {
        Fatal("movable global memory is unsupported");
    }
    u32 address = Heap_Allocate(size, flags & 0x40);
    return address ? address : Fail(8);
}

u32 KERNEL32_GlobalReAlloc(u32 address, u32 size, u32 flags) {
    if (flags & 0x80) {
        Fatal("GlobalReAlloc(GMEM_MODIFY) is unsupported");
    }
    u32 result = Reallocate(Process_Heap(), address, size, flags & 0x40);
    return result ? result : Fail(8);
}

u32 KERNEL32_GlobalFree(u32 address) {
    return Heap_Free(address) ? 0 : address;
}

u32 KERNEL32_GlobalFlags(u32) {
    return 0;
}

// Private heaps take fixed ranges in creation order; wibo's placement is not reproduced.
u32 KERNEL32_HeapCreate(u32 flags, u32 initial_size, u32 maximum_size) {
    if (maximum_size && initial_size > maximum_size) {
        return Fail(87);
    }
    if (flags & 0x40000) {
        Fatal("executable heaps are unsupported");
    }
    u32 handle = sNextPrivateHeap;
    Heap* heap = handle < 0x70000000 ? Heap_Create(handle, flags) : nullptr;
    if (!heap) {
        return Fail(8);
    }
    sPrivateHeaps[handle] = heap;
    sNextPrivateHeap += sArenaSize;
    return handle;
}

u32 KERNEL32_HeapDestroy(u32 handle) {
    auto heap = sPrivateHeaps.find(handle);
    if (heap == sPrivateHeaps.end()) {
        return Fail(6);
    }
    bool destroyed = Heap_Destroy(heap->second);
    sPrivateHeaps.erase(heap);
    return destroyed ? 1 : Fail(8);
}

u32 KERNEL32_HeapFree(u32 handle, u32, u32 address) {
    if (!address) {
        return 1;
    }
    auto heap = sPrivateHeaps.find(handle);
    if (heap == sPrivateHeaps.end()) {
        return Fail(6);
    }
    return Free(heap->second, address) ? 1 : Fail(87);
}

u32 KERNEL32_HeapReAlloc(u32 handle, u32 flags, u32 address, u32 size) {
    auto found = sPrivateHeaps.find(handle);
    if (found == sPrivateHeaps.end()) {
        return Fail(6);
    }
    Heap* heap = found->second;
    if ((heap->Flags | flags) & 4) {
        return Fail(address ? 50 : 87);
    }
    bool zero = flags & 8;
    u32 result;
    if (!address) {
        result = Allocate(heap, size ? size : 1, zero);
    }
    else {
        u32 old_size = Usable_Size(heap, address);
        if (!old_size) {
            return Fail(87);
        }
        bool in_place = flags & 0x10;
        if (!size && !in_place) {
            Free(heap, address);
            return 0;
        }
        if (!size || (in_place && size > old_size)) {
            return Fail(8);
        }
        result = size <= old_size ? address : Reallocate(heap, address, size, zero);
    }
    return result ? result : Fail(8);
}

u32 KERNEL32_HeapAlloc(u32 handle, u32 flags, u32 size) {
    return KERNEL32_HeapReAlloc(handle, flags, 0, size);
}
