# The hybrid build

Nothing here builds a standalone compiler yet. `tools/build.py` makes `build/mwccps2.exe`: a copy of
the original with the recompiled code merged in.

1. The sources (`hybrid/`, `src/`, `ext/`) are compiled with clang for `i686-pc-windows-msvc` and
   linked by lld-link into an image of their own, at a fixed address.
2. The merge copies that image's sections into a copy of the exe, behind the exe's own; adds its
   imports to the exe's import table; and makes `mwrecomp_entry` (`hybrid/hook.c`) the entry point.
3. When the exe starts, `mwrecomp_entry` installs the hooks and jumps to the original entry.

The result is one exe. No DLL is loaded for it, the license DLL the compiler imports is left alone,
and the same file runs on Windows and under [wibo](https://github.com/decompals/wibo). The original
file is never changed, and the merged exe is not kept in the repository: it contains the original.

```bash
python tools/setup.py                     # the original compiler into orig/ (and wibo on Linux)
python tools/build.py                     # build/mwccps2.exe: every ported unit and the extensions
python tools/build.py --ports none --out build/ext    # the original's code with only the extensions
```

`build.py` needs clang, lld-link and llvm-dlltool on PATH (any recent LLVM; version-suffixed names
are found too). It compiles what changed, in parallel.

`build/obj/mwrecomp.dll` is the linked image by itself: the recompiled code without the original.
`python tools/build.py --image FILE --out DIR` merges such a file, so a machine without LLVM can
make the exe from an image built elsewhere and its own copy of the original.

## Two ways to change the original

**`HOOK(addr, fn)`** replaces the original function at `addr` with `fn`: a `jmp` over its first five
bytes. This is how a ported function goes live ([PORTING.md](PORTING.md)).

**`HOOK_WRAP(name, addr, moved, rel_at, before, after)`** keeps the function (or its port) and runs
two handlers around it. This is how the extensions are attached ([EXTENSIONS.md](EXTENSIONS.md)).
A wrap must not show in the compiler's memory, because the compiler's output depends on memory it
never wrote ([ENTROPY.md](ENTROPY.md)): the function runs at the stack depth and with the registers
it would have had, the handlers run on a stack of their own, and the return address slot is put
back as the original call would have left it. With handlers that do nothing, the compiler's memory
is the same as without the wrap. `include/orig.h` has the details.

Recompiled code calls the original's functions and uses its globals at their fixed addresses
(`ORIG_FN`, `ORIG_FN_STDCALL`, `ORIG_VAR`). The exe has no ASLR (`DllCharacteristics` = 0), so it
always loads at 0x400000.

## What must not change

- **The start.** `mwrecomp_entry` saves the stack below the entry's `esp`, runs the installer on
  it, and puts it back, registers and flags included: the original starts as if nothing ran first.
- **Where the hybrid's image sits** (`IMAGE_AT` in `tools/build.py`, 0x01000000, with an empty
  section filling the gap). Addresses of ported code end up in memory the compiler later reads as
  leftovers, such as return addresses on the stack. Right behind the exe (0x595000) three game
  units compiled differently from the original; at 0x01000000 none does.
- **The exe's own file.** Only the copy is changed: five sections added (`.mwpad`, `.mwtext`,
  `.mwrdata`, `.mwdata`, `.mwidata`), the entry point, the import directory.

Checked on .hack//Infection's 216 units that use this compiler: both builds give byte-identical
objects, debug sections included, to the original's.

## Environment

Read when the exe starts; `mwrecomp.env` next to the exe (NAME=VALUE lines) sets them for that
folder only, so two builds can be compared side by side.

| Env var | Effect |
|---|---|
| `MWRECOMP_HOOKS=off` | install no hooks: the hybrid runs as the original |
| `MWRECOMP_SKIP=503ca0,...` | leave these functions original |
| `MWRECOMP_ONLY=503ca0,...` | install only these hooks |
| `MWRECOMP_LIST=1` | print each installed hook to stderr |

For a negative control, `build.py --out build/neg --cflags=-DRECOMP_NEGATIVE_CONTROL`
builds a version with a deliberate bug in `magic` (`src/backend/DIVbyCnst.c`): 58 of the 3,875 cases fail, every time.

The coverage variables are in [TESTING.md](TESTING.md), the ones for hunting leftovers in [ENTROPY.md](ENTROPY.md).
