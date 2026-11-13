// Guest CPU state and the operations generated code is written in.
#pragma once

#include "types.h"
#include "x87.h"

#include <string.h>

struct CPU {
    // Each general register with its 16-bit and 8-bit views (little-endian host).
#define REGISTER(name) union { u32 e##name##x; u16 name##x; struct { u8 name##l, name##h; }; }
    REGISTER(a);
    REGISTER(c);
    REGISTER(d);
    REGISTER(b);
#undef REGISTER
    union { u32 esp; u16 sp; };
    union { u32 ebp; u16 bp; };
    union { u32 esi; u16 si; };
    union { u32 edi; u16 di; };
    u32 eip; // set by returns and indirect jumps; callers compare it with their call site
    u32 fs;  // address of the thread environment block
    bool cf, pf, af, zf, sf, of, df;
    X87State x87;
};

extern CPU cpu;

// A recompiled guest function. `m` is the host address of guest address zero. The stack
// pointer is passed in and returned, which keeps it in a host register; cpu.esp holds it
// only while host code runs.
using GuestFunction = u32(u8* __restrict m, u32 esp);
GuestFunction* Guest_Lookup(u32 address);

// Calls the function at a guest address. An unknown address is left in cpu.eip for the callers
// to unwind to: it may be a call site in an outer frame (longjmp, exception dispatch).
u32 Guest_Dispatch(u8* m, u32 esp, u32 address);
[[noreturn]] void Guest_Trap(u32 address);
[[noreturn]] void Fatal(const char* format, ...);
// When set, runs before each instruction listed under "hooks" in config/images.json.
// Returns true after making the guest function return.
extern bool (*Guest_Hook)(u32 address);

// Guest addresses are 64-bit here so that generated code can add a small displacement
// without wrapping; guest.cpp keeps guard pages on both sides of the address space.
template <typename T> inline T Load(const u8* m, s64 address) {
    T value;
    memcpy(&value, m + address, sizeof(T));
    return value;
}

template <typename T> inline void Store(u8* m, s64 address, T value) {
    memcpy(m + address, &value, sizeof(T));
}

inline void Push(u8* m, u32& esp, u32 value) {
    esp -= 4;
    Store<u32>(m, esp, value);
}

inline u32 Pop(const u8* m, u32& esp) {
    u32 value = Load<u32>(m, esp);
    esp += 4;
    return value;
}

#define R8(address) Load<u8>(m, address)
#define R16(address) Load<u16>(m, address)
#define R32(address) Load<u32>(m, address)
#define W8(address, value) Store<u8>(m, address, value)
#define W16(address, value) Store<u16>(m, address, value)
#define W32(address, value) Store<u32>(m, address, value)
#define PUSH(value) Push(m, esp, value)
#define POP() Pop(m, esp)
#define TAIL(function) [[clang::musttail]] return function(m, esp)
#define CALL(function, next) \
    do { Push(m, esp, next); esp = function(m, esp); if (cpu.eip != next) goto unwind; } while (0)
#define CALL_INDIRECT(target, next) \
    do { \
        u32 callee = target; \
        Push(m, esp, next); \
        esp = Guest_Dispatch(m, esp, callee); \
        if (cpu.eip != next) goto unwind; \
    } while (0)
#define JUMP_INDIRECT(target) \
    do { \
        u32 address = target; \
        GuestFunction* function = Guest_Lookup(address); \
        if (function) { \
            TAIL(function); \
        } \
        cpu.eip = address; \
        return esp; \
    } while (0)
#define RET(bytes) do { cpu.eip = Pop(m, esp); return esp + bytes; } while (0)
#define HOOK(address) \
    do { \
        if (Guest_Hook) { \
            cpu.esp = esp; \
            bool returned = Guest_Hook(address); \
            esp = cpu.esp; \
            if (returned) return esp; \
        } \
    } while (0)
#define TRAP(address) Guest_Trap(address)

template <typename T> inline bool Sign(T value) {
    return value >> (sizeof(T) * 8 - 1);
}

template <typename T> inline bool AddOverflow(T a, T b, T r) {
    return Sign<T>(~(a ^ b) & (a ^ r));
}

template <typename T> inline bool SubOverflow(T a, T b, T r) {
    return Sign<T>((a ^ b) & (a ^ r));
}

inline bool Auxiliary(u32 a, u32 b, u32 r) {
    return (a ^ b ^ r) & 0x10;
}

inline bool Parity(u32 r) {
    return !(__builtin_popcount(r & 0xff) & 1);
}

template <typename T> inline T Subtract_Flags(T a, T b) {
    T r = a - b;
    cpu.cf = a < b;
    cpu.of = SubOverflow<T>(a, b, r);
    cpu.af = Auxiliary(a, b, r);
    cpu.zf = r == 0;
    cpu.sf = Sign<T>(r);
    cpu.pf = Parity(r);
    return r;
}

enum class ShiftOperation { Left, Right, Arithmetic, RotateLeft, RotateRight, CarryLeft, CarryRight };

// Shifts and rotates whose flags are read. A zero count changes nothing.
u32 Shift_Bits(ShiftOperation operation, u32 value, u32 count, u32 width);
u32 Shift_Double(u32 value, u32 source, u32 count, bool left);
// Divides edx:eax, leaving the quotient in eax and the remainder in edx.
void Divide(u32 divisor, bool is_signed);

template <typename T> inline T Shift(ShiftOperation operation, T value, u32 count) {
    return static_cast<T>(Shift_Bits(operation, value, count, sizeof(T) * 8));
}

enum class StringOperation { Move, Store, Load, Compare, Scan };

// movs/stos/lods/cmps/scas. `repeat` is 0 for none, 1 for rep/repe, 2 for repne.
template <typename T> inline void String(u8* __restrict m, StringOperation operation, u32 repeat) {
    u32 size = sizeof(T);
    u32 step = cpu.df ? 0u - size : size;
    bool compares = operation == StringOperation::Compare || operation == StringOperation::Scan;
    for (u32 count = repeat ? cpu.ecx : 1; count; count--) {
        switch (operation) {
        case StringOperation::Move:
            Store<T>(m, cpu.edi, Load<T>(m, cpu.esi));
            break;
        case StringOperation::Store:
            Store<T>(m, cpu.edi, static_cast<T>(cpu.eax));
            break;
        case StringOperation::Load:
            memcpy(&cpu.eax, m + cpu.esi, sizeof(T));
            break;
        case StringOperation::Compare:
            Subtract_Flags<T>(Load<T>(m, cpu.esi), Load<T>(m, cpu.edi));
            break;
        case StringOperation::Scan:
            Subtract_Flags<T>(static_cast<T>(cpu.eax), Load<T>(m, cpu.edi));
            break;
        }
        if (operation != StringOperation::Store && operation != StringOperation::Scan) {
            cpu.esi += step;
        }
        if (operation != StringOperation::Load) {
            cpu.edi += step;
        }
        if (repeat) {
            cpu.ecx--;
            if (compares && cpu.zf != (repeat == 1)) {
                break;
            }
        }
    }
}
