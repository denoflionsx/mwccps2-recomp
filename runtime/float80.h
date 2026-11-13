#pragma once

#include "types.h"

struct Float80 {
    u64 Significand;
    u16 SignExponent;
};

enum class FloatOperation : u32 {
    Add,
    Subtract,
    Multiply,
    Divide
};

// Return x87 exception bits; callers apply masks and commit guest state.
u16 Float80_Arithmetic(u16 control, const Float80* left, const Float80* right,
    FloatOperation operation, Float80* result, bool* rounded_up = nullptr);
u16 Float80_From_Bits(u64 bits, u32 width, Float80* result);
void Float80_From_Integer(s64 value, Float80* result);
u16 Float80_Compare(const Float80* left, const Float80* right, u16* condition);
u16 Float80_To_Bits(u16 control, const Float80* value, u32 width, u64* result, bool* rounded_up = nullptr);
u16 Float80_To_Integer(u16 control, const Float80* value, u32 width, s64* result, bool* rounded_up = nullptr);
