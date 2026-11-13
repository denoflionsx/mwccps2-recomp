// The x87 instructions the images use, operating on cpu.x87 and guest memory.
#pragma once

#include "float80.h"

struct X87State {
    Float80 Registers[8];
    u16 Control;
    u16 Status;
    u16 Tags;
};

void X87_Initialize();
void X87_Wait();
void X87_Set_Control(u16 control);
void X87_Change_Sign(bool absolute);
void X87_Exchange(u32 index);
void X87_Extract();
void X87_Load_Log10_2();
void X87_Load_Integer(s64 value);
void X87_Load_Register(u32 index);
void X87_Store_Register(u32 index);
void X87_Load(u32 address, u32 width, bool integer);
void X87_Store(u32 address, u32 width, bool integer, bool pop);
// `operand` is a guest address when `memory`/`width` is set, otherwise a stack index.
void X87_Compare(u32 operand, bool memory, u32 pop_count);
void X87_Arithmetic(FloatOperation operation, u32 operand, u32 width, u32 destination, bool reverse, bool pop);
void X87_Decimal(u32 address, bool store);
