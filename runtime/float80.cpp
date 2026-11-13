#include "float80.h"

#include <assert.h>
extern "C" {
#include "softfloat.h"
extFloat80_t softfloat_originalRoundPackToExtF80(bool sign, int_fast32_t exponent,
    uint_fast64_t significand, uint_fast64_t extra, uint_fast8_t precision);
}

static bool sArithmeticActive;
static bool sRoundedUp;
static u16 sControl;

static bool Magnitude_Increased(const extFloat80_t* input, const extFloat80_t* output) {
    u32 first = input->signExp & 0x7fff;
    u32 second = output->signExp & 0x7fff;
    return second > first || (second == first && output->signif > input->signif);
}

extern "C" extFloat80_t softfloat_roundPackToExtF80(bool sign, int_fast32_t exponent,
    uint_fast64_t significand, uint_fast64_t extra, uint_fast8_t precision) {
    u32 previous_flags = softfloat_exceptionFlags;
    extFloat80_t output = softfloat_originalRoundPackToExtF80(sign, exponent, significand, extra, precision);
    if (!sArithmeticActive) {
        return output;
    }
    u32 flags = softfloat_exceptionFlags;
    // x87 preserves a wrapped finite result for unmasked exponent exceptions.
    if ((flags & softfloat_flag_overflow) && !(sControl & 8)) {
        exponent -= 0x6000;
        softfloat_exceptionFlags = previous_flags;
        output = softfloat_originalRoundPackToExtF80(sign, exponent, significand, extra, precision);
        flags = softfloat_exceptionFlags | softfloat_flag_overflow;
    }
    else if (exponent <= 0 && (output.signExp & 0x7fff) == 0 && !(sControl & 16)) {
        exponent += 0x6000;
        softfloat_exceptionFlags = previous_flags;
        output = softfloat_originalRoundPackToExtF80(sign, exponent, significand, extra, precision);
        flags = softfloat_exceptionFlags | softfloat_flag_underflow;
    }
    u32 rounding = softfloat_roundingMode;
    softfloat_roundingMode = softfloat_round_minMag;
    extFloat80_t truncated = softfloat_originalRoundPackToExtF80(sign, exponent, significand, extra, precision);
    sRoundedUp = Magnitude_Increased(&truncated, &output);
    softfloat_roundingMode = rounding;
    softfloat_exceptionFlags = flags;
    return output;
}

static void Begin_Operation(u16 control) {
    const u8 rounding[] = {softfloat_round_near_even, softfloat_round_min,
        softfloat_round_max, softfloat_round_minMag};
    const u8 precision[] = {32, 80, 64, 80};
    softfloat_roundingMode = rounding[(control >> 10) & 3];
    extF80_roundingPrecision = precision[(control >> 8) & 3];
    softfloat_detectTininess = softfloat_tininess_afterRounding;
    softfloat_exceptionFlags = 0;
}

static u16 Exception_Bits() {
    u32 flags = softfloat_exceptionFlags;
    return static_cast<u16>(((flags & softfloat_flag_invalid) ? 1 : 0)
        | ((flags & softfloat_flag_infinite) ? 4 : 0)
        | ((flags & softfloat_flag_overflow) ? 8 : 0)
        | ((flags & softfloat_flag_underflow) ? 16 : 0)
        | ((flags & softfloat_flag_inexact) ? 32 : 0));
}

static void Copy_Result(const extFloat80_t* value, Float80* result) {
    result->Significand = value->signif;
    result->SignExponent = value->signExp;
}

static u16 Prepare_Value(const Float80* value, extFloat80_t* result) {
    result->signif = value->Significand;
    result->signExp = value->SignExponent;
    u32 exponent = value->SignExponent & 0x7fff;
    if (exponent && !(value->Significand >> 63)) {
        // Unsupported x87 encodings produce the indefinite quiet NaN.
        result->signif = 0xc000000000000000ull;
        result->signExp = 0xffff;
        return 1;
    }
    if (!exponent && value->Significand) {
        if (value->Significand >> 63) {
            result->signExp |= 1;
        }
        return 2;
    }
    return 0;
}

u16 Float80_Arithmetic(u16 control, const Float80* left, const Float80* right,
    FloatOperation operation, Float80* result, bool* rounded_up) {
    Begin_Operation(control);
    if (rounded_up) {
        *rounded_up = false;
    }
    extFloat80_t first;
    extFloat80_t second;
    extFloat80_t output;
    u16 exceptions = Prepare_Value(left, &first) | Prepare_Value(right, &second);
    if (exceptions & 1) {
        result->Significand = 0xc000000000000000ull;
        result->SignExponent = 0xffff;
        return 1;
    }
    bool first_nan = (first.signExp & 0x7fff) == 0x7fff && (first.signif << 1);
    bool second_nan = (second.signExp & 0x7fff) == 0x7fff && (second.signif << 1);
    if (first_nan || second_nan) {
        exceptions = 0;
    }
    sArithmeticActive = true;
    sRoundedUp = false;
    sControl = control;
    switch (operation) {
    case FloatOperation::Add:
        extF80M_add(&first, &second, &output);
        break;
    case FloatOperation::Subtract:
        extF80M_sub(&first, &second, &output);
        break;
    case FloatOperation::Multiply:
        extF80M_mul(&first, &second, &output);
        break;
    case FloatOperation::Divide:
        extF80M_div(&first, &second, &output);
        break;
    }
    sArithmeticActive = false;
    if (rounded_up) {
        *rounded_up = sRoundedUp;
    }
    Copy_Result(&output, result);
    if (Exception_Bits() & 5) {
        exceptions = 0;
    }
    return exceptions | Exception_Bits();
}

u16 Float80_From_Bits(u64 bits, u32 width, Float80* result) {
    assert(width == 32 || width == 64);
    Begin_Operation(0x37f);
    extFloat80_t output;
    u16 exceptions = 0;
    if (width == 32) {
        float32_t input = {static_cast<u32>(bits)};
        f32_to_extF80M(input, &output);
        if (!(bits & 0x7f800000) && (bits & 0x7fffff)) {
            exceptions = 2;
        }
    }
    else {
        float64_t input = {bits};
        f64_to_extF80M(input, &output);
        if (!(bits & 0x7ff0000000000000ull) && (bits & 0xfffffffffffffull)) {
            exceptions = 2;
        }
    }
    Copy_Result(&output, result);
    return exceptions | Exception_Bits();
}

void Float80_From_Integer(s64 value, Float80* result) {
    extFloat80_t output;
    i64_to_extF80M(value, &output);
    Copy_Result(&output, result);
}

u16 Float80_Compare(const Float80* left, const Float80* right, u16* condition) {
    Begin_Operation(0x37f);
    extFloat80_t first;
    extFloat80_t second;
    u16 exceptions = Prepare_Value(left, &first) | Prepare_Value(right, &second);
    bool first_nan = (first.signExp & 0x7fff) == 0x7fff && (first.signif << 1);
    bool second_nan = (second.signExp & 0x7fff) == 0x7fff && (second.signif << 1);
    if ((exceptions & 1) || first_nan || second_nan) {
        *condition = 0x4500;
        return 1;
    }
    if (extF80M_eq(&first, &second)) {
        *condition = 0x4000;
    }
    else {
        *condition = extF80M_lt(&first, &second) ? 0x100 : 0;
    }
    return exceptions | Exception_Bits();
}

u16 Float80_To_Bits(u16 control, const Float80* value, u32 width, u64* result, bool* rounded_up) {
    assert(width == 32 || width == 64);
    Begin_Operation(control);
    extFloat80_t input;
    u16 exceptions = Prepare_Value(value, &input) & 1;
    if (width == 32) {
        *result = extF80M_to_f32(&input).v;
    }
    else {
        *result = extF80M_to_f64(&input).v;
    }
    exceptions |= Exception_Bits();
    if (rounded_up) {
        extFloat80_t output;
        if (width == 32) {
            f32_to_extF80M(float32_t{static_cast<u32>(*result)}, &output);
        }
        else {
            f64_to_extF80M(float64_t{*result}, &output);
        }
        *rounded_up = (exceptions & 0x20) && !(exceptions & 1) && Magnitude_Increased(&input, &output);
    }
    return exceptions;
}

u16 Float80_To_Integer(u16 control, const Float80* value, u32 width, s64* result, bool* rounded_up) {
    assert(width == 32 || width == 64);
    Begin_Operation(control);
    extFloat80_t input;
    u16 exceptions = Prepare_Value(value, &input) & 1;
    if (width == 32) {
        *result = extF80M_to_i32(&input, softfloat_roundingMode, true);
    }
    else {
        *result = extF80M_to_i64(&input, softfloat_roundingMode, true);
    }
    exceptions |= Exception_Bits();
    if (rounded_up) {
        extFloat80_t output;
        i64_to_extF80M(*result, &output);
        *rounded_up = (exceptions & 0x20) && !(exceptions & 1) && Magnitude_Increased(&input, &output);
    }
    return exceptions;
}
