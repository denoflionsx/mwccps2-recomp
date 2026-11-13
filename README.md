# mwccps2-recomp

Metrowerks `mwccps2.exe` 3.0.1 (build 020123), the PlayStation 2 C/C++ compiler, statically
recompiled to a native executable for Windows and Linux x64. It needs no original files or wibo
at run time and produces the same output as the original under wibo.

## Build

Requires Clang, Ninja, CMake 3.20+, and Python 3.10+. On Windows, use a developer prompt.

```sh
python -m pip install -r requirements.txt
python tools/setup.py
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

`tools/setup.py` downloads and verifies the original compiler, its license library, SoftFloat,
and mimalloc. The result is `build/mwccps2.exe`, on Linux too.

## Use

Pass the original compiler's arguments unchanged:

```sh
build/mwccps2.exe -c -O2 file.c -o file.o
```

## Patches

Two optional changes to the compiler, enabled with `MWRECOMP_EXT=line_path,stale_hascall` (or
`all`). Variables can also be set as `NAME=VALUE` lines in `mwrecomp.env` beside the executable.

**`line_path`** records the path from `#line N "path"` in debug information instead of the real
file's. Put the directive first in each source and header.

**`stale_hascall`** sets the `hascall` byte of expression nodes, which the compiler leaves
uninitialized and which can change the order float arguments are evaluated in:

```c
#pragma stale_hascall on       // set it in the nodes that follow
#pragma stale_hascall off      // clear it
#pragma stale_hascall reset    // leave it as found
#pragma stale_hascall(1, 0, _) // for the next numeric literals: set, clear, leave
```

| Variable | Effect |
| --- | --- |
| `MWRECOMP_HASCALL=on\|off` | The mode each source starts in. |
| `MWRECOMP_HASCALL_TRACE=1` | Print each numeric literal and the byte it was given. |
| `MWRECOMP_HASCALL_REPLAY=file` | Take each literal's byte from the file's `0` and `1` characters. |

## Compare with the original

```sh
python tools/difftest.py --cand build/mwccps2.exe --corpus path/to/sources
```

Compares exit status, diagnostics, and every output byte. Run it on Linux, where the reference
is the original under wibo.
