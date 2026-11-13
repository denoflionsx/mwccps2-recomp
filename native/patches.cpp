// Optional changes to the compiler's behaviour, run from the hook sites listed in
// config/images.json. The README describes how to use them.
#include "patches.h"
#include "win32.h"

#include <stdio.h>
#include <stdlib.h>

// Compiler routines and data, by address in mwccps2.exe.
enum : u32 {
    Compile_Begin = 0x433120,      // start of each source file
    Allocate_Return = 0x433352,    // the node allocator's return; eax is the new block
    Pragma_Handler = 0x43fdb0,
    Line_Rename = 0x4419f0,        // #line: (file, Pascal-string name)
    File_Node = 0x433790,          // makes the debug-record name node for a file
    Debug_Begin = 0x48af29,
    Directive_Begin = 0x4413b0,
    Directive_Return = 0x4414a5,
    Skip_Line = 0x43d5c0,
    Report_Error = 0x43c670,
    Abort_Compile = 0x479220,
    Make_Name_Node = 0x432ef0,
    Resolve_File = 0x441b30,
    Integer_Literal_Caller = 0x469477,
    Float_Literal_Caller = 0x4694b7,

    Source_Position = 0x57aba0,
    Main_File = 0x54d2f4,          // also the start of the file table
    Current_File = 0x57b6d8,
    Current_Name_Node = 0x57b714,
    Full_Paths = 0x57dbfd,
    Main_File_Name = 0x567f80,
    File_Index = 0x57dc72,
    Line_Number = 0x57c800,
    Integer_Value = 0x56c8b0,
    Float_Value = 0x56e268,
};

static constexpr u32 sStack = 0x10d00000; // scratch memory and a stack for calls into the compiler
static constexpr u32 sStackSize = 0x100000;
static constexpr u32 sNames = sStack + sStackSize; // separate from the scratch stack
static constexpr u32 sNamesSize = 0x100000;
static constexpr u32 sNameSize = 268;
static constexpr u32 sExpressionSize = 0x28;
static constexpr u32 sHascallOffset = 5;

enum : u8 { AsFound, Clear, Set };

static bool sStaleHascall;
static bool sLinePath;
static bool sTrace;
static FILE* sReplay;
static u8 sMode;
static u8 sQueue[64]; // pending values from `#pragma stale_hascall(...)`
static u32 sQueued;
static u32 sTaken;
static u32 sMainName;
static u32 sNameCount;
static u32 sSavedStack;
static u8 sStackBytes[0x2000];

// Calls a compiler routine on the scratch stack and restores the CPU state, so the call
// leaves no trace on the compiler's own stack. Returns false if the compiler exited.
static bool Call(u32 address, std::initializer_list<u32> arguments = {}, u32* result = nullptr) {
    CPU saved = cpu;
    u32 top = sStack + sStackSize - 16;
    cpu.esp = cpu.esp >= sStack && cpu.esp < top ? cpu.esp - 64 : top;
    if (!Guest_Call(address, arguments)) {
        return false;
    }
    if (result) {
        *result = cpu.eax;
    }
    cpu = saved;
    return true;
}

// Returns from the hooked compiler function in its place.
static bool Return() {
    cpu.eip = Pop(gMemory, cpu.esp);
    return true;
}

static const char* Blanks(const char* text) {
    return text + strspn(text, " \t");
}

static bool Word(const char** text, const char* word) {
    usize length = strlen(word);
    if (strncmp(*text, word, length) || isalnum(static_cast<u8>((*text)[length])) || (*text)[length] == '_') {
        return false;
    }
    *text += length;
    return true;
}

// #pragma stale_hascall on | off | reset | (0, 1, _, ...)
static bool Pragma() {
    u32 position = Read(Source_Position);
    std::string text(Text(position), strcspn(Text(position), "\r\n"));
    const char* cursor = Blanks(text.c_str());
    if (!Word(&cursor, "stale_hascall")) {
        return false;
    }
    cursor = Blanks(cursor);
    u32 queued = 0;
    bool valid = true;
    if (*cursor == '(') {
        cursor = Blanks(cursor + 1);
        while (valid && *cursor != ')') {
            valid = queued < sizeof(sQueue) && strchr("01_", *cursor) && *cursor;
            if (valid) {
                sQueue[queued++] = *cursor == '1' ? Set : *cursor == '0' ? Clear : AsFound;
                cursor = Blanks(cursor + 1);
                valid = *cursor == ',' || *cursor == ')';
                cursor = *cursor == ',' ? Blanks(cursor + 1) : cursor;
            }
        }
        if (valid) {
            cursor++;
            sQueued = queued;
            sTaken = 0;
        }
    }
    else {
        if (Word(&cursor, "on")) {
            sMode = Set;
        }
        else if (Word(&cursor, "off")) {
            sMode = Clear;
        }
        else if (Word(&cursor, "reset")) {
            sMode = AsFound;
        }
        else {
            valid = false;
        }
        sQueued = sTaken = 0;
    }
    if (valid) {
        Write(Source_Position, position + static_cast<u32>(cursor - text.c_str()));
        return !Call(Skip_Line) || Return();
    }
    return !Call(Report_Error, {0x75}) || !Call(Abort_Compile) || Return();
}

// Copies a path from a #line directive, folding each doubled backslash.
static std::string Path_Text(u32 address, u32 length) {
    std::string path;
    for (u32 index = 0; index < length && path.size() < 255; index++) {
        char character = Read<char>(address + index);
        if (character == '\\' && index + 1 < length && Read<char>(address + index + 1) == '\\') {
            index++;
        }
        path += character;
    }
    return path;
}

static bool Rename_File() {
    u32 file = Read(cpu.esp + 4);
    u32 name = Read(cpu.esp + 8);
    std::string path = Path_Text(name + 1, Read<u8>(name));
    if (file != Read(Main_File)) {
        u32 node = 0;
        strcpy(Text(sStack), path.c_str());
        if (!Call(Make_Name_Node, {sStack}, &node)) {
            return true;
        }
        Write(file + 0x46, node);
        if (Read(Current_File) == file) {
            Write(Current_Name_Node, node);
        }
        return Return();
    }
    // The main file's names live here, as the compiler's own node would be shared.
    u32 index = 0;
    while (index < sNameCount && strcmp(Text(sNames + index * sNameSize + 10), path.c_str())) {
        index++;
    }
    if (index == sNameCount) {
        if (sNameCount == sNamesSize / sNameSize) {
            Fatal("too many #line file names");
        }
        sNameCount++;
    }
    sMainName = sNames + index * sNameSize;
    strcpy(Text(sMainName + 10), path.c_str());
    if (!Read(Current_File)) {
        Write(Current_Name_Node, sMainName);
    }
    usize tail = Read<u8>(Full_Paths) ? 0 : path.find_last_of("\\/") + 1;
    strcpy(Text(Main_File_Name), path.c_str() + tail);
    return Return();
}

// The path of a `#line N "path"` directive that begins a source text, if there is one.
static bool First_Path(u32 text, u32 size, std::string* path) {
    const char* bytes = Text(text);
    auto skip = [&](u32 index, const char* set) {
        while (index < size && strchr(set, bytes[index]) && bytes[index]) {
            index++;
        }
        return index;
    };
    if (size < 5 || memcmp(bytes, "#line", 5)) {
        return false;
    }
    u32 digits = skip(5, " \t");
    u32 index = skip(skip(digits, "0123456789"), " \t");
    if (index == size || digits == size || !isdigit(static_cast<u8>(bytes[digits])) || bytes[index++] != '"') {
        return false;
    }
    u32 first = index;
    while (index < size && !strchr("\"\r\n", bytes[index])) {
        index++;
    }
    if (index == size || bytes[index] != '"') {
        return false;
    }
    *path = Path_Text(text + first, index - first);
    return true;
}

static bool Name_Node() {
    u32 caller = Read(cpu.esp);
    if (caller == 0x43c953) {
        cpu.eax = 0;
        return Return();
    }
    if (caller != 0x43b587) {
        return false;
    }
    u32 spec = Read(cpu.esp + 4);
    u32 text = Read(spec - 12);
    std::string path;
    if (!text || !First_Path(text, Read(spec - 8), &path)) {
        return false;
    }
    u32 node = 0;
    if (!Call(Resolve_File, {sStack + 0x100, spec, Read(cpu.esp + 8)})) {
        return true;
    }
    strcpy(Text(sStack), path.c_str());
    if (!Call(Make_Name_Node, {sStack}, &node)) {
        return true;
    }
    cpu.eax = node;
    return Return();
}

// The #line handler dirties stack bytes that later code reads uninitialized; keep them.
static void Save_Stack() {
    sSavedStack = 0;
    const char* directive = Blanks(Text(Read(Source_Position)));
    if (!strncmp(directive, "line", 4) && (directive[4] == ' ' || directive[4] == '\t')) {
        sSavedStack = cpu.esp - sizeof(sStackBytes);
        memcpy(sStackBytes, gMemory + sSavedStack, sizeof(sStackBytes));
    }
}

// The next value from MWRECOMP_HASCALL_REPLAY, when set.
static void Replay(u8* wanted) {
    static bool opened;
    const char* name = getenv("MWRECOMP_HASCALL_REPLAY");
    if (!opened && name && *name) {
        sReplay = fopen(Path_From_Windows(name).string().c_str(), "rb");
        if (!sReplay) {
            fprintf(stderr, "mwrecomp: cannot read MWRECOMP_HASCALL_REPLAY\n");
            exit(1);
        }
    }
    opened = true;
    if (sReplay) {
        s32 character;
        do {
            character = fgetc(sReplay);
        } while (character != EOF && character != '0' && character != '1');
        if (character == EOF) {
            fprintf(stderr, "mwrecomp: MWRECOMP_HASCALL_REPLAY has fewer literals than this run\n");
            exit(1);
        }
        *wanted = character == '1' ? Set : Clear;
    }
}

static void Trace(bool integer, u8 found, u8 left) {
    u32 file = Read(Main_File + Read<u16>(File_Index) * 4);
    std::string name(Text(file + 7), Read<u8>(file + 6) < 63 ? Read<u8>(file + 6) : 63);
    fprintf(stderr, "mwrecomp: literal %s:%u ", name.c_str(), Read(Line_Number));
    if (integer) {
        fprintf(stderr, "int %08x", Read(Integer_Value));
    }
    else {
        fprintf(stderr, "float %08x%08x", Read(Float_Value + 4), Read(Float_Value));
    }
    fprintf(stderr, " stale %u hascall %u\n", found != 0, left != 0);
}

// Sets the `hascall` byte of a new expression node, which the compiler leaves uninitialized.
static void Stale_Hascall() {
    u32 caller = Read(cpu.esp);
    bool literal = caller == Integer_Literal_Caller || caller == Float_Literal_Caller;
    u32 address = cpu.eax + sHascallOffset;
    u8 found = Read<u8>(address);
    u8 wanted = sMode;
    if (literal && sTaken < sQueued) {
        wanted = sQueue[sTaken++];
    }
    if (literal) {
        Replay(&wanted);
    }
    u8 left = wanted == AsFound ? found : wanted == Set;
    Write<u8>(address, left);
    if (literal && sTrace) {
        Trace(caller == Integer_Literal_Caller, found, left);
    }
}

static bool Hook(u32 address) {
    switch (address) {
    case Compile_Begin: {
        sMainName = sNameCount = sSavedStack = sQueued = sTaken = 0;
        const char* mode = getenv("MWRECOMP_HASCALL");
        sMode = !sStaleHascall || !mode ? AsFound : !strcmp(mode, "on") ? Set : !strcmp(mode, "off") ? Clear : AsFound;
        return false;
    }
    case Allocate_Return:
        if (Read(cpu.esp + 4) == sExpressionSize) {
            Stale_Hascall();
        }
        return false;
    case Pragma_Handler:
        return sStaleHascall && Pragma();
    case Line_Rename:
        return sLinePath && Rename_File();
    case File_Node:
        return sLinePath && Name_Node();
    case Debug_Begin:
        if (sLinePath && sMainName && !Read(Current_Name_Node)) {
            Write(Current_Name_Node, sMainName);
        }
        return false;
    case Directive_Begin:
        if (sLinePath) {
            Save_Stack();
        }
        return false;
    case Directive_Return:
        if (sLinePath && sSavedStack) {
            memcpy(gMemory + sSavedStack, sStackBytes, sizeof(sStackBytes));
            sSavedStack = 0;
        }
        return false;
    }
    return false;
}

static bool Enabled(const char* list, const char* name) {
    while (list && *list) {
        list += strspn(list, " ,\t\r\n");
        usize length = strcspn(list, " ,\t\r\n");
        if ((length == strlen(name) && !strncmp(list, name, length)) || (length == 3 && !strncmp(list, "all", 3))) {
            return true;
        }
        list += length;
    }
    return false;
}

void Patches_Install() {
    const char* trace = getenv("MWRECOMP_HASCALL_TRACE");
    const char* replay = getenv("MWRECOMP_HASCALL_REPLAY");
    sStaleHascall = Enabled(getenv("MWRECOMP_EXT"), "stale_hascall");
    sLinePath = Enabled(getenv("MWRECOMP_EXT"), "line_path");
    sTrace = trace && *trace;
    if (sStaleHascall || sLinePath || sTrace || (replay && *replay)) {
        if (!Memory_Map(sStack, sStackSize)) {
            Fatal("cannot map the patch stack");
        }
        if (sLinePath && !Memory_Map(sNames, sNamesSize)) {
            Fatal("cannot map the patch names");
        }
        Guest_Hook = Hook;
    }
}
