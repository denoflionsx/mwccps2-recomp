#include "cpu.h"

u32 Shift_Bits(ShiftOperation operation, u32 value, u32 count, u32 width) {
    u32 mask = 0xffffffffu >> (32 - width);
    u32 sign = u32{1} << (width - 1);
    bool left = operation == ShiftOperation::Left || operation == ShiftOperation::RotateLeft ||
        operation == ShiftOperation::CarryLeft;
    bool rotate = operation >= ShiftOperation::RotateLeft;
    count &= 31;
    if (rotate) {
        count %= width + (operation >= ShiftOperation::CarryLeft);
    }
    if (!count) {
        return value;
    }
    u32 original = value;
    bool carry = cpu.cf;
    if (!rotate) {
        // Shift in 64 bits, so a count past the width needs no special case.
        u64 wide = operation == ShiftOperation::Arithmetic ?
            static_cast<u64>(static_cast<s64>(static_cast<s32>(value << (32 - width))) >> (32 - width)) : value;
        carry = left ? (wide << count) >> width & 1 : wide >> (count - 1) & 1;
        value = static_cast<u32>(left ? wide << count : operation == ShiftOperation::Arithmetic ?
            static_cast<u64>(static_cast<s64>(wide) >> count) : wide >> count) & mask;
        cpu.zf = value == 0;
        cpu.sf = value & sign;
        cpu.pf = Parity(value);
    }
    else if (operation <= ShiftOperation::RotateRight) {
        u32 turn = left ? count : 32 - count;
        value = (value << turn) | (value >> (32 - turn));
        carry = left ? value & 1 : value & sign;
    }
    else {
        // Rotates through the carry are rare and short: one bit at a time.
        for (u32 index = 0; index < count; index++) {
            bool previous = carry;
            carry = left ? value & sign : value & 1;
            value = left ? (value << 1) | previous : (value >> 1) | (previous ? sign : 0);
        }
    }
    cpu.cf = carry;
    // The overflow flag is defined only for single-bit shifts.
    if (count == 1) {
        cpu.of = left ? bool(value & sign) != carry : operation == ShiftOperation::Right ? bool(original & sign) :
            operation == ShiftOperation::Arithmetic ? false : bool(value & sign) != bool(value & (sign >> 1));
    }
    return value;
}

u32 Shift_Double(u32 value, u32 source, u32 count, bool left) {
    count &= 31;
    if (!count) {
        return value;
    }
    u32 result = left ? (value << count) | (source >> (32 - count)) : (value >> count) | (source << (32 - count));
    cpu.cf = (left ? value >> (32 - count) : value >> (count - 1)) & 1;
    cpu.zf = result == 0;
    cpu.sf = Sign(result);
    cpu.pf = Parity(result);
    if (count == 1) {
        cpu.of = Sign(value ^ result);
    }
    return result;
}

void Divide(u32 divisor, bool is_signed) {
    u64 dividend = (u64{cpu.edx} << 32) | cpu.eax;
    if (!divisor) {
        Fatal("guest division by zero");
    }
    if (is_signed) {
        // INT64_MIN / -1 overflows on the host as well as the guest.
        s64 quotient = dividend == u64{1} << 63 ? s64{1} << 32 : static_cast<s64>(dividend) / static_cast<s32>(divisor);
        if (quotient != static_cast<s32>(quotient)) {
            Fatal("guest division overflow");
        }
        cpu.edx = static_cast<u32>(static_cast<s64>(dividend) % static_cast<s32>(divisor));
        cpu.eax = static_cast<u32>(quotient);
    }
    else {
        if (dividend / divisor > 0xffffffffu) {
            Fatal("guest division overflow");
        }
        cpu.edx = static_cast<u32>(dividend % divisor);
        cpu.eax = static_cast<u32>(dividend / divisor);
    }
}
