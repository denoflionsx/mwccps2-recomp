# mwccps2-recomp

A recompilation of the Metrowerks CodeWarrior PS2 C/C++ compiler, `mwccps2.exe` 3.0.1
(build 020123), one source unit at a time.

The goal is a rebuilt compiler that behaves the same as the original: the same exit code, the same
stdout/stderr and byte-identical output files over a test corpus. A byte-identical executable is
not the goal.

## How it works: the hybrid build

Nothing here builds a standalone compiler yet. Each recompiled function is compiled into a
replacement `LMGR326B.dll` (the FlexLM license DLL the compiler loads). When the DLL loads, it writes
a `jmp` over each original function it replaces. Everything not yet recompiled still runs from the
original exe, and the three FlexLM calls are forwarded to the real DLL (`LMGR326B_real.dll`).

Recompiled code calls the original's functions and uses its globals at their fixed addresses. The
exe has no ASLR and always loads at 0x400000.

```
hybrid/   the DLL itself: hook installer, coverage traps, debugging knobs (poke.c)
include/  orig.h: ORIG_FN / ORIG_FN_STDCALL / ORIG_VAR / HOOK
src/      the recompiled units, one file per original source file
```

`include/orig.h` in brief:

| Macro | Meaning |
|---|---|
| `ORIG_FN(ret, name, addr, args)` | call the original cdecl function at `addr` |
| `ORIG_FN_STDCALL(...)` | the same for `__stdcall` (the driver's OS layer and allocators) |
| `ORIG_VAR(type, addr)` | the original global, as an lvalue |
| `HOOK(addr, fn)` | replace the original function at `addr` with `fn` |

## Sources

Most units are ports of the matching files in
[mweppc](https://github.com/doldecomp/mweppc), the CC0 decompilation of Metrowerks' GameCube
compiler. 1,078 of our 4,403 functions are byte-identical to its GameCube 2.0 build. Every port is
checked against our own disassembly, because constants, layouts and some code differ for the PS2
target; where they differ, our code wins and the file header says so. Struct layouts are written as
offsets taken from our code.

## Building and testing

The build and test tools live in the parent project (`../tools/recomp`). They need clang,
lld-link and llvm-dlltool on PATH.

```
python tools/recomp/build_hybrid.py                     # build the DLL (from the parent folder)
python tools/recomp/portcheck.py CONTROL [CONTROL ...]  # build, full suite, negative controls
python tools/recomp/unitinfo.py src/driver/CLIO.c       # source + disassembly for porting a unit
python tools/recomp/nextunits.py                        # unported units, by corpus coverage
```

`difftest.py` runs the original and the hybrid side by side on 3,911 cases: the probe and test
sources under 16 flag sets, 195 real game files from the .hack//Infection decompilation, and 16
command-line-only cases (`-version`, `-help`, message styles, ...).

## Conventions for a unit

- Hook only functions the corpus runs. Everything else stays the original's until a test reaches it.
- Every unit has a **negative control**: a deliberate bug behind
  `#ifdef RECOMP_NEGATIVE_CONTROL_<UNIT>`, placed inside a function body. It shows the corpus
  actually tests the unit. If it is not caught, a coarser control is added.
- Functions the whole compile runs under (`CmdLine_*`, `SendCompilerRequest`,
  `Main_ResolveProject`, ...) keep the original's frame size. They are `noinline`, padded or naked
  asm, because the original reads stack leftovers in a few places.
- Every unit passes the full suite before it is committed; per-unit results are in
  `docs/RECOMP.md` in the parent project.

## Status

607 hooks from 68 source files; the full corpus passes. The per-unit table, the negative-control
results and the known run-to-run variance of the original are in `docs/RECOMP.md` in the parent
project.
