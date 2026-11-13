#include "guest.h"

static u32 Physical_Index(u32 index) {
    return ((cpu.x87.Status >> 11) + index) & 7;
}

static void Set_Top(u32 top) {
    cpu.x87.Status = static_cast<u16>((cpu.x87.Status & ~0x3800u) | (top << 11));
}

static void Indefinite(Float80* value) {
    value->Significand = 0xc000000000000000ull;
    value->SignExponent = 0xffff;
}

static bool Pending() {
    return (cpu.x87.Status & ~cpu.x87.Control & 0x3f) != 0;
}

static void Update_Pending() {
    cpu.x87.Status &= ~0x8080u;
    if (Pending()) {
        cpu.x87.Status |= 0x8080;
    }
}

void X87_Initialize() {
    cpu.x87.Control = 0x37f;
    cpu.x87.Status = 0;
    cpu.x87.Tags = 0xffff;
}

void X87_Set_Control(u16 control) {
    X87_Wait();
    cpu.x87.Control = (control & 0x1f3f) | 0x40;
    Update_Pending();
}

// An unmasked exception is pending; the guest installs no handler for it.
void X87_Wait() {
    if (Pending()) {
        Fatal("unmasked x87 exception, status %04x", cpu.x87.Status);
    }
}

static bool X87_Raise(u16 exceptions) {
    cpu.x87.Status |= exceptions & 0x3f;
    Update_Pending();
    return (exceptions & ~cpu.x87.Control & 0x3f) == 0;
}

// `index` counts from the top of the register stack.
static bool X87_Read(u32 index, Float80* value) {
    u32 physical = Physical_Index(index);
    if (((cpu.x87.Tags >> (physical * 2)) & 3) == 3) {
        cpu.x87.Status = (cpu.x87.Status & ~0x200u) | 0x40;
        Indefinite(value);
        return X87_Raise(1);
    }
    *value = cpu.x87.Registers[physical];
    return true;
}

static void X87_Write(u32 index, const Float80* value) {
    u32 physical = Physical_Index(index);
    u32 exponent = value->SignExponent & 0x7fff;
    u32 tag = 0;
    if (!exponent && !value->Significand) {
        tag = 1;
    }
    else if (!exponent || exponent == 0x7fff || !(value->Significand >> 63)) {
        tag = 2;
    }
    cpu.x87.Registers[physical] = *value;
    cpu.x87.Tags = static_cast<u16>((cpu.x87.Tags & ~(3u << (physical * 2))) | (tag << (physical * 2)));
}

static bool X87_Push(const Float80* value, u16 exceptions = 0) {
    u32 next = (Physical_Index(0) - 1) & 7;
    Float80 output = *value;
    cpu.x87.Status &= ~0x200u;
    if (((cpu.x87.Tags >> (next * 2)) & 3) != 3) {
        cpu.x87.Status |= 0x240;
        if (!X87_Raise(1)) {
            return false;
        }
        Indefinite(&output);
    }
    // FLD still pushes when its denormal exception is unmasked.
    else if (!X87_Raise(exceptions) && (exceptions & 1)) {
        return false;
    }
    Set_Top(next);
    X87_Write(0, &output);
    return true;
}

static void X87_Pop() {
    u32 top = Physical_Index(0);
    cpu.x87.Tags |= 3u << (top * 2);
    Set_Top((top + 1) & 7);
}

void X87_Change_Sign(bool absolute) {
    X87_Wait();
    u32 physical = Physical_Index(0);
    bool empty = ((cpu.x87.Tags >> (physical * 2)) & 3) == 3;
    cpu.x87.Status &= ~0x200u;
    Float80 value;
    if (!X87_Read(0, &value)) {
        return;
    }
    if (!empty) {
        value.SignExponent = absolute ? value.SignExponent & 0x7fff : value.SignExponent ^ 0x8000;
    }
    X87_Write(0, &value);
}

void X87_Exchange(u32 index) {
    X87_Wait();
    cpu.x87.Status &= ~0x200u;
    Float80 first;
    Float80 second;
    bool first_valid = X87_Read(0, &first);
    bool second_valid = X87_Read(index, &second);
    if (first_valid && second_valid) {
        X87_Write(0, &second);
        X87_Write(index, &first);
    }
}

void X87_Load_Log10_2() {
    X87_Wait();
    u32 rounding = (cpu.x87.Control >> 10) & 3;
    Float80 value = {0x9a209a84fbcff799ull, 0x3ffd};
    if (rounding == 1 || rounding == 3) {
        value.Significand--;
    }
    X87_Push(&value);
}

void X87_Extract() {
    X87_Wait();
    cpu.x87.Status &= ~0x200u;
    u32 next = (Physical_Index(0) - 1) & 7;
    Float80 significand;
    Float80 exponent;
    bool empty = ((cpu.x87.Tags >> (Physical_Index(0) * 2)) & 3) == 3;
    if (!X87_Read(0, &significand)) {
        return;
    }
    if (((cpu.x87.Tags >> (next * 2)) & 3) != 3) {
        Indefinite(&significand);
        if (X87_Push(&significand)) {
            X87_Write(1, &significand);
        }
        if (empty) {
            cpu.x87.Status &= ~0x200u;
        }
        return;
    }
    u32 encoded = significand.SignExponent & 0x7fff;
    u16 exceptions = 0;
    if (encoded && !(significand.Significand >> 63)) {
        Indefinite(&significand);
        exponent = significand;
        exceptions = 1;
    }
    else if (encoded == 0x7fff) {
        if (significand.Significand << 1) {
            exceptions = (significand.Significand & 0x4000000000000000ull) ? 0 : 1;
            significand.Significand |= 0x4000000000000000ull;
            exponent = significand;
        }
        else {
            exponent = {0x8000000000000000ull, 0x7fff};
        }
    }
    else if (!significand.Significand) {
        exponent = {0x8000000000000000ull, 0xffff};
        exceptions = 4;
    }
    else {
        s32 power = static_cast<s32>(encoded ? encoded : 1) - 0x3fff;
        if (!encoded) {
            exceptions = 2;
            while (!(significand.Significand >> 63)) {
                significand.Significand <<= 1;
                power--;
            }
        }
        Float80_From_Integer(power, &exponent);
        significand.SignExponent = (significand.SignExponent & 0x8000) | 0x3fff;
    }
    if (!X87_Raise(exceptions)) {
        return;
    }
    X87_Write(0, &exponent);
    X87_Push(&significand);
}

void X87_Load_Integer(s64 value) {
    X87_Wait();
    Float80 output;
    Float80_From_Integer(value, &output);
    X87_Push(&output);
}

void X87_Load_Register(u32 index) {
    X87_Wait();
    cpu.x87.Status &= ~0x200u;
    Float80 value;
    if (X87_Read(index, &value)) {
        X87_Push(&value);
    }
}

void X87_Store_Register(u32 index) {
    X87_Wait();
    cpu.x87.Status &= ~0x200u;
    Float80 value;
    if (X87_Read(0, &value)) {
        X87_Write(index, &value);
        X87_Pop();
    }
}

void X87_Load(u32 address, u32 width, bool integer) {
    X87_Wait();
    Float80 value;
    u16 exceptions = 0;
    if (integer) {
        Float80_From_Integer(width == 32 ? s64{Read<s32>(address)} : Read<s64>(address), &value);
    }
    else if (width == 80) {
        value = {Read<u64>(address), Read<u16>(address + 8)};
    }
    else {
        exceptions = Float80_From_Bits(width == 32 ? Read<u32>(address) : Read<u64>(address), width, &value);
    }
    X87_Push(&value, exceptions);
}

void X87_Store(u32 address, u32 width, bool integer, bool pop) {
    X87_Wait();
    cpu.x87.Status &= ~0x200u;
    Float80 value;
    if (!X87_Read(0, &value)) {
        return;
    }
    u64 bits;
    u16 exceptions;
    bool rounded_up;
    if (integer) {
        s64 output;
        exceptions = Float80_To_Integer(cpu.x87.Control, &value, width, &output, &rounded_up);
        bits = static_cast<u64>(output);
    }
    else {
        exceptions = Float80_To_Bits(cpu.x87.Control, &value, width, &bits, &rounded_up);
    }
    if (exceptions & ~cpu.x87.Control & 0x18) {
        exceptions &= ~0x20u;
    }
    X87_Raise(exceptions);
    if (exceptions & ~cpu.x87.Control & 0x1f) {
        return;
    }
    if (rounded_up) {
        cpu.x87.Status |= 0x200;
    }
    memcpy(gMemory + address, &bits, width / 8);
    if (pop) {
        X87_Pop();
    }
}

void X87_Compare(u32 operand, bool memory, u32 pop_count) {
    X87_Wait();
    Float80 left;
    Float80 right;
    u16 exceptions = memory ? Float80_From_Bits(Read<u64>(operand), 64, &right) : 0;
    cpu.x87.Status &= ~0x200u;
    if (!X87_Read(0, &left) || (!memory && !X87_Read(operand, &right))) {
        cpu.x87.Status |= 0x4500;
        return;
    }
    u16 condition;
    exceptions |= Float80_Compare(&left, &right, &condition);
    if (exceptions & 1) {
        exceptions = 1;
    }
    cpu.x87.Status = (cpu.x87.Status & ~0x4500u) | condition;
    if (X87_Raise(exceptions)) {
        for (u32 index = 0; index < pop_count; index++) {
            X87_Pop();
        }
    }
}

void X87_Arithmetic(FloatOperation operation, u32 operand, u32 width, u32 destination, bool reverse, bool pop) {
    X87_Wait();
    Float80 left;
    Float80 right;
    Float80 output;
    u16 exceptions = width ? Float80_From_Bits(width == 32 ? Read<u32>(operand) : Read<u64>(operand), width, &right)
        : 0;
    cpu.x87.Status &= ~0x200u;
    if (!X87_Read(destination, &left) || (!width && !X87_Read(operand, &right))) {
        return;
    }
    bool rounded_up;
    exceptions |= Float80_Arithmetic(cpu.x87.Control, reverse ? &right : &left, reverse ? &left : &right,
        operation, &output, &rounded_up);
    if (exceptions & 1) {
        exceptions = 1;
    }
    else if (exceptions & ~cpu.x87.Control & 2) {
        exceptions = 2;
    }
    X87_Raise(exceptions);
    if (exceptions & ~cpu.x87.Control & 7) {
        return;
    }
    if (rounded_up) {
        cpu.x87.Status |= 0x200;
    }
    X87_Write(destination, &output);
    if (pop) {
        X87_Pop();
    }
}

void X87_Decimal(u32 address, bool store) {
    X87_Wait();
    Float80 value;
    if (!store) {
        u64 magnitude = 0;
        for (u32 index = 9; index > 0; index--) {
            u8 byte = Read<u8>(address + index - 1);
            magnitude = magnitude * 100 + (byte >> 4) * 10 + (byte & 15);
        }
        Float80_From_Integer(static_cast<s64>(magnitude), &value);
        value.SignExponent |= static_cast<u16>((Read<u8>(address + 9) & 0x80) << 8);
        X87_Push(&value);
        return;
    }
    cpu.x87.Status &= ~0x200u;
    if (!X87_Read(0, &value)) {
        return;
    }
    s64 integer;
    bool rounded_up;
    u16 exceptions = Float80_To_Integer(cpu.x87.Control, &value, 64, &integer, &rounded_up);
    u64 magnitude = integer < 0 ? 0 - static_cast<u64>(integer) : static_cast<u64>(integer);
    if (magnitude > 999999999999999999ull) {
        exceptions = 1;
    }
    X87_Raise(exceptions);
    if (exceptions & ~cpu.x87.Control & 1) {
        return;
    }
    if (exceptions & 1) {
        Write<u64>(address, 0xc000000000000000ull);
        Write<u16>(address + 8, 0xffff);
    }
    else {
        for (u32 index = 0; index < 9; index++) {
            u32 pair = static_cast<u32>(magnitude % 100);
            Write<u8>(address + index, static_cast<u8>(((pair / 10) << 4) | (pair % 10)));
            magnitude /= 100;
        }
        Write<u8>(address + 9, value.SignExponent >> 8 & 0x80);
        if (rounded_up) {
            cpu.x87.Status |= 0x200;
        }
    }
    X87_Pop();
}
