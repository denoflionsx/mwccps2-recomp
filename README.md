# mwccps2-recomp

A recompilation of the Metrowerks CodeWarrior PS2 C/C++ compiler, `mwccps2.exe` 3.0.1
(build 020123), one function at a time, and a few extensions for projects that rebuild games with
it byte for byte.

The goal is a rebuilt compiler that behaves the same as the original: the same exit code, the same
stdout/stderr and byte-identical output files over a test corpus. A byte-identical executable is
not the goal.

## Quick start

```bash
python tools/setup.py     # the original compiler into orig/ (and wibo on Linux)
python tools/build.py     # build/mwccps2.exe
python tools/exttest.py   # the extensions, against the original
python tools/difftest.py --cand build/mwccps2.exe
```

Needs Python 3.9+, clang, lld-link and llvm-dlltool (any recent LLVM), and for the tools
`pip install -r requirements.txt`. `make`, `make ext` and `make test` wrap the same commands.

`build/mwccps2.exe` is a copy of the original with the recompiled code merged in: one file, used
exactly like the original, on Windows or under [wibo](https://github.com/decompals/wibo). Every
function not yet recompiled still runs from the original's code. `python tools/build.py --ports none`
leaves all of them the original's and adds only the extensions.

## Layout

```
hybrid/   the code that runs first: installs hooks and wraps, coverage traps, debugging knobs
include/  orig.h: ORIG_FN / ORIG_VAR / HOOK / HOOK_WRAP
src/      the recompiled units, one file per original source file, in mweppc's folders
          (driver, frontend, optimizer, backend, msl)
ext/      the extensions
tests/    corpus sources, command-line cases (cli/), extension cases (ext/)
tools/    setup, build, tests, and the analysis tools behind docs/data
docs/     how it works and what was found; docs/data holds the analysis results
orig/     the original compiler (not in the repository)
```

| Document | What |
|---|---|
| [docs/HYBRID.md](docs/HYBRID.md) | how the build works, hooks and wraps, environment variables |
| [docs/EXTENSIONS.md](docs/EXTENSIONS.md) | `#line` paths in debug records, `#pragma stale_hascall`, allocation tracing |
| [docs/PORTING.md](docs/PORTING.md) | porting a unit from mweppc, conventions, per-unit results, next steps |
| [docs/TESTING.md](docs/TESTING.md) | the differential test, the corpus, coverage, negative controls |
| [docs/ENTROPY.md](docs/ENTROPY.md) | how the original's output depends on memory it never wrote |

## Sources

Most units are ports of the matching files in
[mweppc](https://github.com/doldecomp/mweppc), the CC0 decompilation of Metrowerks' GameCube
compiler. 1,078 of our 4,403 functions are byte-identical to its GameCube 2.0 build. Every port is
checked against our own disassembly, because constants, layouts and some code differ for the PS2
target; where they differ, our code wins and the file header says so. Struct layouts are written as
offsets taken from our code.

## Status

1250 hooks from 115 source files, and two extensions. On .hack//Infection's 216 units that use
this compiler, the build with every port and the build with only the extensions both give objects
byte-identical to the original's, debug sections included (under wibo).
