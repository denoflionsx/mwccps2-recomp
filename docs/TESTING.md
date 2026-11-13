# Testing

**"Behaves the same"** means: for every case in the test corpus (source file × flag set), the
hybrid gives the same exit code, the same stdout/stderr and the same output files as the original.

```bash
python tools/build.py
python tools/exttest.py                                   # the extensions, and that they are idle when unused
python tools/difftest.py --cand build/mwccps2.exe         # tests/ under every flag set
python tools/difftest.py --cand build/mwccps2.exe --manifest tests/cli/manifest.json build/corpus/imoq/manifest.json
```

Without `--cand`, `difftest.py` compares the original with itself (a determinism check). On Linux
the compilers run under wibo (`orig/wibo` from `tools/setup.py`, or `WIBO`).

`python tools/imoq_corpus.py path/to/IMOQ_Decomp` snapshots the .hack//Infection decompilation's
sources into `build/corpus/imoq/` with a manifest of its compile commands.

The counts below are den's, from his working tree, whose probe sources (`probes/src`) are not in
this repository; here the single-file corpus is `tests/`.

## The corpus

- `probes/src` and `tests`, each file under 16 flag sets (`FLAGSETS` in `difftest.py`): five
  code-generation sets (`O0`, `O2`, `O4p`, `O4pg`, and `imoq`, the game's flags) plus driver modes
  and front-end options (`-E`, `-EP`, `-M`, `-make`, `-ansi strict`, `-w all`, `-char unsigned`,
  `-inline all,deferred`, `-enum min`, `-fp off`, `-sdatathreshold 0`, `-lang c++`, `-opt display`).
- The 195 compile units of .hack//Infection, as manifest cases: the exact command its `build.ninja`
  runs (per-unit flags, `tools/mwcc_warmup.cpp` compiled first in the same run, `MWCIncludes=include`).
  `imoq_corpus.py` snapshots the game's `src/` and `include/` into `build/corpus/imoq/`.
- 3,875 cases in all; a full run takes about a minute.
- `tests/cli/manifest.json` (2026-11-06): 16 command-line-only cases (`-version`, `-help`,
  `--plugin-debug`, `-v`, `-M`/`-MD`, `-E`, a missing file, a bad option, warnings and every message
  style) on `tests/cli/x.c`. `-timing` is left out: it prints times. `portcheck.py` runs this
  manifest along with the IMOQ one, so a normal suite is now 3,911 cases.

Every file a compile leaves behind is compared, not only the object. A failing case is rerun up to
3 times (`--retries`): one that passes on a retry is counted as **intermittent**, apart from the
failures that repeat (see "Address-dependent output" below).

## Coverage

`difftest.py --cov` records which basic blocks of the original the candidate runs, and
`tools/covreport.py <run dir> --md docs/data/coverage.md` summarizes it per module, with the
cases that add the most and the blocks only one case reaches.

How it works: Ghidra's basic blocks (exported with a Ghidra script → `docs/data/blocks.tsv`,
63,203 blocks in 4,703 functions) go into `cov_blocks.bin`. The hybrid writes an `int3` at each
block start; a vectored exception handler marks the block in a memory-mapped file (so it survives a
crash), puts the original byte back and resumes. Each block traps once per process. Blocks under a
hook's `jmp` are left alone, and functions already replaced are left out of the report.

**Use coverage runs only to measure.** The traps are not fully invisible: on some game units
(`enemy*.cpp`) the original's code comes out differently with them armed, probably because the
compiler reads uninitialized stack memory and Windows' exception handling writes below the stack
pointer. Decide pass/fail with a normal run.

**Result** ([docs/data/coverage.md](data/coverage.md)): 32,630 of 63,201 blocks (51.6%) and
3,148 of 4,679 functions (67.3%). The probes alone reached 46.4%; the extra flag sets added 1.8
points, the game 3.4 more (its `system.cpp`, which includes 12 other sources, runs 25,000 blocks by
itself). The biggest gaps:

| Gap | Blocks not run | How to reach it |
|---|---:|---|
| `IroPointerAnalysis.c` (+ ADTs) | ~2,050 | probes only switch on the pragma; the analysis itself needs pointer-heavy code at `-O4` with `opt_pointer_analysis` |
| `CPrec.c` precompiled headers | 1,172 | `-precompile`, `-prefix x.mch` (mweppc reports PCH bytes are not reproducible; compare the objects built with them) |
| `CSOM.c` | 603 | SOM classes (`#pragma SOMCheckEnvironment` etc.); possibly unreachable on this target |
| C++ front end (`CExpr2`, `CScope`, `CTemplate*`, `CClass`, `CException`, `CRTTI`) | ~4,500 | a C++ test suite: overload resolution, templates (partial specialization, members, friends), namespaces, exceptions with RTTI on |
| C front end (`CExpr`, `CDecl`, `CInit`, `CPrep`, `CParser`) | ~4,100 | error and warning paths, K&R C, initializer corners, `#pragma` coverage, random programs (Csmith) |
| `Resources.c`, `Option.c`, `CL*`, `Targets.c..MsDos.c` (driver) | ~4,000 | command-line handling: every option, bad options, `-help`, `-version`, response files, missing files, several sources per run |
| back end (`CodeGen`, `InstrSelection`, `MIPS_Peephole`, `InlineAsmMIPS`) | ~2,200 | VU0, 128-bit types, inline asm forms, `-fp off`, `#pragma` codegen switches |

### Targeted negative controls (2026-11-09)

A control only changes behaviour where its code runs, so `portcheck.py` reruns each control on just
the cases that can reach it:

- The suite run passes `difftest.py --hookcov`: the hybrid (`MWRECOMP_HOOKCOV`) writes an `int3`
  on the first byte of every replacement function and records, per case, which hooks ran (direct
  calls between ported functions included). The map is kept in `build/hookcov_map.json`.
- `tools/ctlreach.py CONTROL` finds the function holding the control's `#ifdef`/`#ifndef`
  (normal-build preprocessing, brace structure) and, for a helper without a hook, follows its
  callers by name. Callers of hooked functions are followed too: clang inlines ported functions
  into ported callers (`Peephole_Block` into `Peephole_Run`), and an inlined copy never hits the
  entry trap, so every hooked function on the way counts. A control at file scope, or an unhooked
  name used outside a function body (macro, table, function pointer), means "full suite".
- The control then runs with `--only` on the cases whose hooks meet that set; a case outside it runs
  the control build exactly like the normal one. "no case reaches its code" skips the build: the
  control is not caught. `--full` restores the old behaviour.

Validation: for all 49 controls of 2026-11-08/09 with a full-suite run, every case that failed
(apart from the flaky `enemy*`, `webnews`, `audio`, `pgrider` units) is inside the control's targeted
set; 8 were also rerun targeted, the markers CBROWSE3 and IROLINEARFORM4 failing every targeted case
that reaches them. Two earlier "catches" were noise: CDECL8 (708 cases, 0 fails) and OBJGEN2 (3,906
cases, only an `enemy2` flip). Of 285 controls, 278 map to hooks and 7 need the full suite; on
average a control runs 63% of the cases (narrow ones a few dozen, broad ones nearly all), so a
control takes about 4 minutes instead of 6 when it runs all its cases.

**Early stop.** A control is caught once a handful of cases fail, so `portcheck.py` passes
`difftest.py --stop-after 10 --stop-ignore 'enemy|webnews|audio|pgrider'`: the flaky units run last
and do not count, and once 10 other cases fail the cases not yet started are dropped (the 16 already
running finish, so about 26 run). Only uncaught or weakly caught controls run their whole targeted
set. First test (2026-11-09): CPREC, IROLINEARFORM4 (stopped after 26 cases each), CERROR6 (54 cases,
2 fails) and CDECL8 (708 cases, only an `enemy2` flip) took 2 min 19 s together, builds included,
against about 25 minutes before. A stopped control's fail count is therefore not its full reach;
`--all-fails` runs every control to the end. Unit rows from now on give "caught" counts as printed.

The hook traps run during the real suite. Unlike block coverage they arm only the replacements' own entries,
once per process, and the first suite with them passed 3,911/3,911 with no intermittent case; if
`enemy*` flips grow, compare with a `--full` run.
