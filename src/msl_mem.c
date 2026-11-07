/*
 * msl_mem.c — MSL's memmove, memcpy, memset and __memrchr (mweppc src/msl/mem.c).
 *
 * Byte-identical between mweppc's GameCube 2.0 compiler and ours; the MSL source comes from the
 * CodeWarrior library, so these are written from our functions' code.
 *
 * They are naked functions with the original's instruction sequence, not C. Reason: the stack
 * bytes a function leaves behind are part of the compiler's behaviour (docs/RECOMP.md,
 * "Address-dependent output": operand builders copy uninitialized stack into operands, and some of
 * those bytes decide code). A C memset compiled by clang saved different registers and changed
 * enemy3/enemy4/enemyz at -O4,p; this version pushes exactly what the original pushes (memset:
 * edi, ebp; memcpy/memmove: esi, edi; __memrchr: edi) and touches no other stack.
 *
 * Semantics, for the record: copies of 16 bytes or more go single bytes until the destination is
 * 4-aligned, then 4-byte words, then the rest (memmove backwards when dest > src); counts compare
 * signed; __memrchr returns s for n == 0 (its repne scasb runs zero times, flags still "found").
 */
#include "orig.h"

#ifdef RECOMP_NEGATIVE_CONTROL_MSLMEM /* build_hybrid.py --cflags=-DRECOMP_NEGATIVE_CONTROL_MSLMEM */
#define MEMMOVE_DIRECTION_CHECK "jmp 1f\n" /* always forward: wrong for overlapping dest > src */
#else
#define MEMMOVE_DIRECTION_CHECK "cmp edi, esi\n ja 4f\n"
#endif

__attribute__((naked)) void memmove_(void) {
    __asm__ volatile(".intel_syntax noprefix\n"
                     "mov edx, dword ptr [esp + 4]\n"
                     "push esi\n"
                     "push edi\n"
                     "mov eax, dword ptr [esp + 0x14]\n"
                     "mov esi, dword ptr [esp + 0x10]\n"
                     "mov edi, edx\n"
                     "xor ecx, ecx\n"
                     MEMMOVE_DIRECTION_CHECK
                     "1:\n"
                     "cmp eax, 0x10\n"
                     "jl 3f\n"
                     "sub ecx, edi\n"
                     "and ecx, 3\n"
                     "je 2f\n"
                     "sub eax, ecx\n"
                     "rep movsb\n"
                     "2:\n"
                     "mov ecx, eax\n"
                     "and eax, 3\n"
                     "shr ecx, 2\n"
                     "rep movsd\n"
                     "test eax, eax\n"
                     "je 9f\n"
                     "3:\n"
                     "mov ecx, eax\n"
                     "rep movsb\n"
                     "jmp 9f\n"
                     "4:\n" /* backwards */
                     "std\n"
                     "lea esi, [esi + eax - 1]\n"
                     "lea edi, [edi + eax - 1]\n"
                     "cmp eax, 0x10\n"
                     "jl 6f\n"
                     "lea ecx, [edi - 3]\n"
                     "and ecx, 3\n"
                     "je 5f\n"
                     "sub eax, ecx\n"
                     "rep movsb\n"
                     "5:\n"
                     "sub esi, 3\n"
                     "sub edi, 3\n"
                     "mov ecx, eax\n"
                     "and eax, 3\n"
                     "shr ecx, 2\n"
                     "rep movsd\n"
                     "test eax, eax\n"
                     "je 7f\n"
                     "add esi, 3\n"
                     "add edi, 3\n"
                     "6:\n"
                     "mov ecx, eax\n"
                     "rep movsb\n"
                     "7:\n"
                     "cld\n"
                     "9:\n"
                     "mov eax, edx\n"
                     "pop edi\n"
                     "pop esi\n"
                     "ret\n"
                     ".att_syntax prefix\n");
}
HOOK(0x404c70, memmove_);

__attribute__((naked)) void memcpy_(void) {
    __asm__ volatile(".intel_syntax noprefix\n"
                     "mov edx, dword ptr [esp + 4]\n"
                     "push esi\n"
                     "push edi\n"
                     "mov eax, dword ptr [esp + 0x14]\n"
                     "mov esi, dword ptr [esp + 0x10]\n"
                     "mov edi, edx\n"
                     "xor ecx, ecx\n"
                     "cmp eax, 0x10\n"
                     "jl 2f\n"
                     "sub ecx, edi\n"
                     "and ecx, 3\n"
                     "je 1f\n"
                     "sub eax, ecx\n"
                     "rep movsb\n"
                     "1:\n"
                     "mov ecx, eax\n"
                     "and eax, 3\n"
                     "shr ecx, 2\n"
                     "rep movsd\n"
                     "test eax, eax\n"
                     "je 3f\n"
                     "2:\n"
                     "mov ecx, eax\n"
                     "rep movsb\n"
                     "3:\n"
                     "mov eax, edx\n"
                     "pop edi\n"
                     "pop esi\n"
                     "ret\n"
                     ".att_syntax prefix\n");
}
HOOK(0x404d00, memcpy_);

__attribute__((naked)) void memset_(void) {
    __asm__ volatile(".intel_syntax noprefix\n"
                     "push edi\n"
                     "push ebp\n"
                     "mov ebp, dword ptr [esp + 0xc]\n"
                     "mov eax, dword ptr [esp + 0x10]\n"
                     "xor ecx, ecx\n"
                     "mov edi, ebp\n"
                     "mov edx, dword ptr [esp + 0x14]\n"
                     "and eax, 0xff\n"
                     "cmp edx, 0x10\n"
                     "jbe 2f\n"
                     "mov ah, al\n"
                     "mov ecx, eax\n"
                     "cmp edx, 4\n"
                     "jle 2f\n"
                     "shl ecx, 0x10\n"
                     "or eax, ecx\n"
                     "xor ecx, ecx\n"
                     "sub ecx, edi\n"
                     "and ecx, 3\n"
                     "je 1f\n"
                     "sub edx, ecx\n"
                     "rep stosb\n"
                     "1:\n"
                     "mov ecx, edx\n"
                     "and edx, 3\n"
                     "shr ecx, 2\n"
                     "je 2f\n"
                     "rep stosd\n"
                     "2:\n"
                     "mov ecx, edx\n"
                     "rep stosb\n"
                     "mov eax, ebp\n"
                     "pop ebp\n"
                     "pop edi\n"
                     "ret\n"
                     ".att_syntax prefix\n");
}
HOOK(0x404d40, memset_);

__attribute__((naked)) void __memrchr_(void) {
    __asm__ volatile(".intel_syntax noprefix\n"
                     "xor edx, edx\n"
                     "push edi\n"
                     "mov edi, dword ptr [esp + 8]\n"
                     "mov eax, dword ptr [esp + 0xc]\n"
                     "mov ecx, dword ptr [esp + 0x10]\n"
                     "lea edi, [edi + ecx - 1]\n"
                     "std\n"
                     "repne scasb\n"
                     "cld\n"
                     "jne 1f\n"
                     "lea edx, [edi + 1]\n"
                     "1:\n"
                     "mov eax, edx\n"
                     "pop edi\n"
                     "ret\n"
                     ".att_syntax prefix\n");
}
HOOK(0x404da0, __memrchr_);
