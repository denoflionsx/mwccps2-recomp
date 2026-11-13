# Porting a function

Functions move from the original into `src/` one at a time; each goes live, and gets tested, the
moment it is written ([HYBRID.md](HYBRID.md)). When nothing calls the original any more, the same
sources build a standalone exe. `src/` is laid out like mweppc: `driver/`, `frontend/`,
`optimizer/`, `backend/`, `msl/`.

## Conventions for a unit

- Hook only functions the corpus runs. Everything else stays the original's until a test reaches it.
- Every unit has a **negative control**: a deliberate bug behind
  `#ifdef RECOMP_NEGATIVE_CONTROL_<UNIT>`, placed inside a function body. It shows the corpus
  actually tests the unit. If it is not caught, a coarser control is added.
- Functions the whole compile runs under (`CmdLine_*`, `SendCompilerRequest`,
  `Main_ResolveProject`, ...) keep the original's frame size. They are `noinline`, padded or naked
  asm, because the original reads stack leftovers in a few places.
- Every unit passes the full suite before it is committed.

## Writing a recompiled function

```c
#include "orig.h"
ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line)); /* call the original */
#define copts_optimize_for_size ORIG_VAR(UInt8, 0x57dbe6)                /* use an original global */

MagicS magic(SInt32 d) { ... }
HOOK(0x503ca0, magic);  /* replace the original at 0x503ca0 */
```

- The exe has no ASLR (`DllCharacteristics` = 0), so it always loads at 0x400000 and every address
  in Ghidra / `docs/data/names.json` stays valid.
- Calling convention: the original is cdecl everywhere, with `ebx/esi/edi/ebp` callee-saved,
  8-byte structs returned in `edx:eax` and larger ones through a hidden first pointer argument. That
  is clang's `i686-pc-windows-msvc` convention too, so no thunks are needed.
- **Struct layouts must be exact.** The first hook went wrong because `decomp/DIVbyCnst.c` had two
  fields of `MagicU` swapped: harmless in notes, wrong in running code. Take field order from the
  original's loads and stores, not from the algorithm.
- Build flags (`build.py`): `-O2 -ffreestanding -fno-builtin -fwrapv -fno-strict-aliasing
  -fno-delete-null-pointer-checks -mno-sse`. Signed overflow wraps like the original's x86 code.
  x87 floating point is not handled yet; mweppc's `include/x87.h` emulates it and should be adopted
  before porting any float code (constant folding, `-sdatathreshold` parsing, literals).
- `build.py` refuses a `HOOK` address that is not a function start in the block list.

## Porting a unit from mweppc by hand

51 mweppc source files have every function byte-identical to ours (333 functions, mostly the driver
and the C library: `Resources.c`, `CWPluginsPrivate.cpp`, `alloc_g.c`, `CLAccessPaths.c`, ...).
`tools/addrmap.py` maps 1,768 of mweppc's names to our addresses
(`docs/data/addrmap_GC_2_0.json`): it walks the 1,078 byte-identical function pairs side by side
and pairs up their relocated addresses and call targets. Only pairs with a unique body vote;
`_copts` is the one conflict (the options struct really differs).

Recipe, as done for [src/driver/CLSegs.c](../src/driver/CLSegs.c):
1. Copy the functions from the mweppc checkout (`src/...`). Skip functions in neither binary (no
   address in the map).
2. Every outside function: `ORIG_FN` / `ORIG_FN_STDCALL` at the address from the map (the driver's
   `xmalloc`/`xrealloc`/`xfree` are `__stdcall`). Call the exe's C library this way too, not ours.
3. Every global: `ORIG_VAR` at the mapped address, so the exe's variable is used.
4. Write the original's assert file name as a literal (`"CLSegs.c"`), not `__FILE__`.
5. Check how callers read small return values: the original returns `Boolean` in `al` and
   `UInt16` in `ax`, and callers test `al` or extend `ax` themselves, so clang's returns fit.
6. `HOOK` every function at its address, statics included when they are functions of their own.
7. Build, run with `MWRECOMP_LIST=1` to see the hooks install, run the suite, check in the last
   coverage report that the corpus runs the functions, and try a deliberate bug.

**CLSegs.c (2026-11-06): ported, 8 functions; the suite passes 3,875/3,875.** The corpus runs 7 of
the 8 (`Segments_GetSegment` never). But a deliberate bug (`Segments_Count` returning one too many,
`--cflags=-DRECOMP_NEGATIVE_CONTROL_CLSEGS`) also passes: no output depends on that value. So for
driver code like this the suite proves "doesn't crash or change output", not that the port is right.
For such units, rely on the byte-identical source (and keep the port a literal copy).

**MemUtils.c (2026-11-06): ported, 5 functions** (`xmalloc`, `xcalloc`, `xrealloc`, `xstrdup`,
`xfree`, all `__stdcall`; [src/driver/MemUtils.c](../src/driver/MemUtils.c)). Call targets and the
`stderr` FILE (`0x510798`) were read from the functions' own code, since `free`, `fprintf` and
`exit` aren't in the address map. The original inlines `memset` and `strlen`; the port uses loops.
The suite passes 3,875/3,875, and a deliberate bug (`xstrdup` dropping the first character,
`-DRECOMP_NEGATIVE_CONTROL_MEMUTILS`) fails all 3,875: every compile goes through these.

**StringExtras.c (2026-11-06): ported, 4 functions** (`strcatn`, `strcpyn`, `ustrcmp`, `ustrncmp`,
`__stdcall`; [src/driver/StringExtras.c](../src/driver/StringExtras.c)). The only outside call is
MSL's `tolower` (`0x405ab0`), passed a sign-extended `char` like clang does; `strlen` is inlined.
The suite passes 3,875/3,875; a deliberate bug (`strcpyn` copying one character fewer,
`-DRECOMP_NEGATIVE_CONTROL_STRINGEXTRAS`) fails all 3,875, mostly in stderr messages, 46 in the object.

**CLPrefs.c (2026-11-06): ported, 5 functions** (preference panels;
[src/driver/CLPrefs.c](../src/driver/CLPrefs.c)). The first port with globals: `panellist`
(`0x54be10`) and `clState.pluginDebug` (`0x57b360` = `clState` + `0x228`), both through `ORIG_VAR`.
It calls the driver's Mac memory emulation (`NewHandle`, `MemError` cdecl; `HLock`, `HUnlock`,
`GetHandleSize`, `SetHandleSize` `__stdcall`); `CLStr90` is message 90. The suite passes 3,875/3,875
and the corpus runs all five in every case. A coarse bug (`Prefs_FindPanel` never finding a panel,
`-DRECOMP_NEGATIVE_CONTROL_CLPREFS2`) fails all 3,875; a fine one (`PrefPanel_GetHandle` copying one
byte too few, `-DRECOMP_NEGATIVE_CONTROL_CLPREFS`) is not caught, as the last byte of the panels the
corpus uses doesn't matter.

(`orig.h`'s `ORIG_VAR` now takes `(type, addr)`, as documented above; it had an unused name argument.)

**CLTarg.c (2026-11-06): ported, 4 functions** (build targets; [src/driver/CLTarg.c](../src/driver/CLTarg.c)).
`Target` (`0x1dc` bytes) embeds many other driver structs, so instead of rebuilding them the port
reaches each member by the offset our functions' code uses, and takes the long assertion texts
from the exe's strings. `OS_GetCWD` is `__stdcall` (no caller cleanup after its call). The suite
passes 3,875/3,875 and the corpus runs all four; a deliberate bug (`cpu` and `os` swapped,
`-DRECOMP_NEGATIVE_CONTROL_CLTARG`) fails all 3,875.

**FileHandles.c (2026-11-06): ported, 5 functions** (files held in memory handles;
[src/driver/FileHandles.c](../src/driver/FileHandles.c)). `OSFileHandle` = `OSSpec` (`0x204`
bytes, copied by value) + `OSHandle` at +0x204 + flags at +0x20c..e; every `OS_*` callee is
`__stdcall`, the two static helpers cdecl. The suite passes 3,875/3,875. The corpus runs
`OS_NewFileHandle`, `OS_WriteFileHandle` and `OS_FreeFileHandle` (2,853 cases: the ones that write
their object this way), never `OS_LoadFileHandle` or `OS_LockFileHandle`. A deliberate bug in the
write path (one byte short, `-DRECOMP_NEGATIVE_CONTROL_FILEHANDLES`) fails exactly those 2,853.
The two unexercised functions are untested.

**MacFileTypes.c (2026-11-06): ported, 6 functions** (Mac file types, magic-number detection;
[src/driver/MacFileTypes.c](../src/driver/MacFileTypes.c)). All `__stdcall`. From the code: the
static `fmList` pointer (`0x530038`), the extension hook (`0x57c7a8`), a packed mapping list (array
pointer at +2) with 14-byte entries and a signed byte length; `memset`/`memcmp` inlined. (Careful:
an `add esp, 8` after a call can be the function's own frame, not caller cleanup; the name's `@8`
settles it.) The suite passes 3,875/3,875 and the corpus runs all six; a deliberate bug (every
magic number matching, `-DRECOMP_NEGATIVE_CONTROL_MACFILETYPES`) fails all 3,875.
Function-pointer globals need a typedef before `ORIG_VAR`.

`tools/unitinfo.py <unit>` prints a unit's source, each function's address and coverage,
and our disassembly with call targets, globals and strings named: everything the recipe needs.

**Stack footprint matters.** The first `mem.c` port, in C, failed `enemy3`, `enemy4` and `enemyz`
(always, not intermittently) although its logic was right: clang's `memset` saved different
registers, so it left different bytes on the stack, and the operand builders later copy such
leftovers into operands that decide code ("All readers found" above). The fix was to write the four
routines as naked functions with the original's instructions, so they push exactly what the
original pushes. If a port fails only in leftover-sensitive units like these, suspect its stack
writes: bisect with `MWRECOMP_SKIP`, and for small leaf routines match the original's pushes.

Later units (same checks: suite with the port, corpus coverage, deliberate bug):

| unit (file in src) | functions | suite | corpus runs | deliberate bug | caught |
|---|---:|---|---|---|---|
| MSL `string.c` (`msl_string.c`) | 11 | 3,875/3,875 | 9 of 11 (`strstr`, `strerror` never) | `strrchr` returns the first match | all 3,875 fail |
| MSL `mem.c` (`msl_mem.c`) | 4 | 3,875/3,875 | all 4 | `memmove` always forward | 1,108 fail |
| `Memory.c` (Mac handles) | 15 | 3,875/3,875 | 13 of 15 | `SetHandleSize` keeps the stale pointer | 776 fail |
| `ResourceStrings.c` | 2 | 3,875/3,875 | both | `Res_GetResource` returns the previous string | 581 fail (every case that looks one up) |
| `CLProj.c` | 2 | 3,875/3,875 | both | `Proj_Terminate` skips freeing the targets | not caught: exit-time cleanup has no visible effect |
| `CLLoadAndCache.c` | 3 | 3,875/3,875 | all 3 | `CopyFileText` size one too large | 3,447 fail |
| MSL `extras.c`, `misc_io.c`, `sysenv_win32.c` (`msl_small.c`) | 5 | 3,875/3,875 | all 5 | `getenv` case-sensitive / `_strnicmp` always equal | not caught (the corpus's names already match in case) / all 3,875 fail |
| `CWSecretPluginCallbacks.cpp` (`CWSecretPluginCallbacks.c`) | 4 | 3,875/3,875 | all 4 | `CWSecretGetNamedPreferences` always fails | all 3,875 fail |
| MSL `pool_alloc_win32_g.c` (`msl_pool_alloc.c`) | 2 | 3,875/3,875 | both (`__sys_free` in 72 cases) | none planted | — (every allocation goes through it) |
| MSL `file_io.c` (`msl_file_io.c`) | 5 | 3,875/3,875 | all 5 (partly) | `fflush` drops the unwritten buffer | 720 fail (the `-E`/`-EP`/`-M`/`-make` cases, which write through stdio) |
| `CLLicenses.c` | 10 | 3,875/3,875 | all 10 (partly) | `License_Checkout` reports failure | all 3,875 fail |
| `MacSpecs.c` | 15 | 3,875/3,875 | 14 of 15 (`OS_ShortenSpec` never) | `OS_CanonNameCase` skips the case fix-up / wrong `parID` | not caught (names already in the right case) / all 3,875 fail |
| `CLOverlays.c` | 17 | 3,875/3,875 | 9 of 17 | `Overlays_CountGroups` one too many | not caught: compile-only runs never act on overlays (weakly tested unit) |
| MSL `time_win32.c` (`msl_time.c`) | 4 | 3,875/3,875 | 3 of 4 | time-zone bias added, not subtracted | not caught: no time value reaches the outputs |
| `Generic.c` (partial: the 7 the corpus runs, of 12) | 7 | 3,875/3,875 | all 7 | `OS_MakeSpec2` without separator / `OS_GetFileNamePtr` returns the whole path | not caught (callers pass paths ending in a separator) / 776 fail |
| `CLAccessPaths.c` (partial: the 10 the corpus runs, of 18) | 10 | 3,875/3,875 | all 10 | `Paths_FindPathSpec` never matches / `Paths_AddPath` drops the path | not caught (duplicates are harmless) / 193 fail (every game unit, through `-I`) |
| `DropInCompilerLinkerPrivate.c` (partial: 12 of 18; the C++ copy users and PCH functions stay) | 12 | 3,875/3,875 | all 12 | `CWIsGeneratingDebugInfo` always says no | 528 fail (the `-g` flag sets lose their debug sections) |
| `CWPluginsPrivate.c` (partial: the 19 the corpus runs, of 29) | 19 | 3,875/3,875 (one `enemy3` flip, 26/26 on rerun) | all 19 | `CWGetProjectFileCount` reports 0 files | 3,313 fail (nothing gets compiled) |
| `IroBitVect.c` (the 5 of 14 that are functions in our exe) | 5 | 3,875/3,875 (one `enemyD` flip, 20/20 on rerun) | all 5 | `Bv_Minus` intersects instead of subtracting | 1,065 fail (optimized code changes) |
| `IroUtil.c` (partial: 13 of the 21 in our exe, plus the static callbacks `ActNop` and `FindStart`) | 13 | 3,875/3,875 (one `enemyC` flip) | all 13 | `IRO_IsIntConstant` always false | 327 fail |
| `CWParserPluginsPrivate.c` (partial: the 13 the corpus runs, of 15 in our exe) | 13 | 3,875/3,875 | all 13 | `CWParserStoreCommandLineForPlugin` stores nothing / `CWParserSetFileOutputName` ignores the name | not caught (nothing reads the stored command lines) / 3,018 fail |
| `CLIncludeFileCache.c` | 8 | 3,875/3,875 (one `enemy2` flip) | 7 (`deletecacheentry` never: the cache never fills) | `CachedIncludeFile` says every hit is precompiled | 178 fail |
| `StringUtils.c` (partial: the 4 the corpus runs, of 7 in our exe) | 4 | 3,875/3,875 | all 4 | `c2pstrcpy` cuts names to 8 characters | 3,433 fail |
| `CLFiles.c` (partial: the 12 the corpus runs, of 13 in our exe) | 12 | 3,875/3,875 | all 12 | `Files_InsertFile` puts every file first / `Files_Count` says 0 | 1 fail (one file per run, so order barely matters) / 3,875 fail |
| `ClientGlue.c` (the 10 the corpus runs, of 11; `CmdLine_Initialize` as naked asm to keep the original stack depth) | 10 | 3,875/3,875 (one `enemy` flip) | all 10 | `SetPluginType` swaps language and type | 360 fail |
| `CLDependencies.c` (the 18 the corpus runs, of 20) | 18 | 3,875/3,875 | all 18 | `Deps_AddDependency` lists nothing / no include file is found | 2 fail (few `-M` cases have includes) / 213 fail |
| `CompilerTools.c` (21) | 16 | 3,875/3,875 (one `enemyK` flip) | all 16 | `getbit(4)` says 3 | 46 fail |
| `COSToolsCLT.c` (13 of 15) | 12 | 3,875/3,875 (one `enemy2` flip) | all 12 | `COS_ResizeHandle` always reports failure | 812 fail |
| `TextUtils.c` (the 3 the corpus runs, of 4 in our exe) | 3 | 3,875/3,875 (one `enemyz` flip) | all 3 | `GetIndString` upper-cases messages | 581 fail (every case with a message) |
| `CLBrowser.c` (the 4 the corpus runs, of 9) | 4 | 3,875/3,875 (two `enemy` flips) | all 4 | `Browser_Initialize` reports out of memory | 3,875 fail |
| `CError.c` (23) | 8 | 3,875/3,875 (one `enemyP` flip) | all 8 | `CError_Warning` drops every warning | 369 fail |
| `CLParserCallbacks_v1.c` (the 6 the corpus runs, of 11; two `__thiscall` members) | 6 | 3,875/3,875 | all 6 | `-o` directory is not stored in the target | 195 fail |
| `CLPluginRequests.c` (the 9 the corpus runs, of 11; `SendCompilerRequest` from our code: it has an up-to-date skip mweppc lacks; hooked functions `noinline` so the compile runs at the original stack depth) | 9 | 3,875/3,875 | all 9 | preprocessing requests report failure | 361 fail |
| `StaticGlue.c` (our counterparts of `cc-eabi-ppc.c` and `cc-eabi-ppc-mw.c`, from our code: PS2 constants and an ELF importer) | 9 | 3,875/3,875 | all 9 | wrong table for "CmdLine Tool Strings" / target OS `EABI` | not caught (strings 13000 unused) / 3,875 fail |
| `LowMem.c` | 2 | 3,875/3,875 | both | ticks always 0 / every memory call fails | 1 fail (user-break timing only) / 812 fail |
| `CInit.c` (the 3 that are functions in our exe) | 3 | 3,875/3,875 | all 3 | read-only and writable data swapped | 819 fail |
| `IrOptimizer.c` (the 2 in our exe) | 2 | 3,875/3,875 | both | loops never unrolled | 118 fail |
| `IroLoop.c` (partial: `BuildLabel`, of 3 in our exe) | 1 | 3,875/3,875 | yes | `IROLF_StartsNode` not set / label node dropped | 1 fail / 118 fail |
| `IroVars.c` (partial: 3 of the 5 in our exe; the two table initializers stay) | 3 | 3,875/3,875 (one `enemyP` flip) | all 3 | variable terms not counted / `allowRegister` ignored | not caught / 371 fail |
| `IroDump.c` (partial: `IRO_SetupDump`, `IRO_Dump`; the log is off in normal builds) | 2 | 3,875/3,875 | both (early exits) | `IRO_Dump` prints to stderr with logging off | 2,859 fail |
| `MsDos.c` (the 44 the corpus runs, of 59: files, handles, time, specs, the path parser, error codes) | 44 | 3,875/3,875 | all 44 | `OS_IsFile` always false / `/` kept in paths / no path is full / Mac times a day late / `fnfErr` mapped to `ioErr` | 213 fail / not caught (backslash paths only) / 582 fail / not caught / not caught (these values never reach output) |
| `CLMain.c` (23 of 41) | 14 | 3,875/3,875 | all 14 | timing line always printed / link model 2 / `stderr2stdout` forced / compiler panel not copied / an extra debug line | 3,573 fail / not caught (reset later) / 1 fail / 360 fail / 3,875 fail |
| `CLIO.c` (the 32 the corpus runs, of 38: console and file output, the message formatter, every message style, line wrapping, `CLPrint`/`CLPrintErr` and `CLStyledMessageDispatch`) | 32 | 3,875/3,875 | all 32 | lines end in `
` / a `!` after every message / warnings returned as errors / every header says "Driver" | 720 fail / 580 fail / not caught (callers ignore the result) / 581 fail |
` instead of `
` / a `!` after every message | 720 fail / 580 fail |
| `CLPlugins.c` (all 40; `GetToolVersionInfo` checked by hand with `-version`; accessors, `Plugin_Call` as naked asm with the original stack use, matching, lookup, create, terminate, version text, object writing, plugin and panel lists, `SupportedPlugin` from our code, the parser lookup with `alloca`, `Plugins_Add`; its `--plugin-debug` dump checked by hand: 108 identical lines) | 40 | 3,875/3,875 | all 40 | file-type mappings not added / object flags ignored / compiler-mapping flags 0 / no linker / no plugin per file / only exact target matches / version text changed / last panel lost / panel warnings always printed / a plugin-flags error per plugin / version text changed | not caught / 3,845 fail / not caught / 1 fail / not caught (the compiler is chosen another way) / 3,875 fail / not caught /  3,875 fail / 3,875 fail / 3,875 fail / 5 fail (the command-line cases) |
| `CLDropinCallbacks_V10.c` (partial: 19 callbacks the corpus runs: file text, include search, messages, new text documents, project entries, licenses, the context constructor) | 19 | 3,911/3,911 | all 19 | target name / never "already included" / warnings as notes / stdout documents with a border / object usage / empty texts / no include found / files never compiled | not caught / not caught / 383 fail / not caught / not caught / not caught (the compiler reads to the NUL) / 771 fail / 3,906 fail |
| `CPrepTokenizer.c` (12 of 16) | 8 | 3,911/3,911 | all 8 | identifiers cut to 8 characters | 2,205 fail |
| `CParser.c` (23 of 24) | 14 | 3,911/3,911 | all 14 | `CParser_IsConst` always false | 112 fail |
| `CFunc.c` (12) | 8 | 3,911/3,911 | all 8 | no unused-variable warnings | 28 fail |
| `CLErrors.c` (whole unit: `CLGetErrorString`, `CLMessageReporter`, `CLReportError`, `CLReportWarning`, `CLReport`, `CLReportOSError`, `CLReportCError`, `CLInternalError`, `CLFatalError`) | 9 | 3,911/3,911 | 5 of 9 | message text starts with `<` | 2 fail |
| `CScope.c` (12 of 15) | 8 | 3,911/3,911 | 7 of 8 | nothing found in hash name spaces | 2,324 fail |
| `CTemplateTools.c` (8 of 12) | 7 | 3,911/3,911 | 6 of 7 | template types not dependent | 24 fail |
| `CPrep.c` (25) | 10 | 3,911/3,911 | 6 of 10 | line numbers stay put | 540 fail |
| `CExpr2.c` (17) | 5 | 3,911/3,911 | all 5 | integer constants one too big | 1,538 fail |
| `CException.c` (5 of 5) | 4 | 3,911/3,911 | 3 of 4 | no exception action cleanup | 189 fail |
| `CMangler.c` (11) | 4 | 3,911/3,911 | 3 of 4 | RTTI names get the `__op` prefix | 47 fail |
| `CLMain.c` part 4 (4 more, 18 of 41: `special_debug`, `special_plugin_debug`, `Main_PassSpecialArgs`, `UpdatePCmdLineEnvirFromVersion`) | 4 | 3,911/3,911 | 1 of 4 | `--plugin-debug` ignored | 363 fail |
| `CLFileOps.c` (partial: 3 of 12 in our exe: `OutputTextData`, `StoreObjectFile`, `set_bytes`, which run after the compile returns; our `StoreObjectFile` has an extra flushed-data check) | 3 | 3,911/3,911 | all 3 | text output one byte short / cut in half | 1 fail (trailing byte not compared) / 727 fail |
| `DropInCompilerLinkerPrivate.c` part 2 (2 more, 14 of 18: `CWGetBrowseOptions`, `CWGetTargetInfo`, through the exe's C++ copy operators) | 2 | 3,911/3,911 | all 2 | target info request fails | 2 fail |
| `IroUtil.c` part 2 (6 more, 19 of 21: `IRO_TypesEqual`, `IRO_is_CPtypeequal`, `IRO_ExprsSame`, `IRO_UnsignedType`, `IRO_LocateFather_Cut_And_Paste_Without_Nopping`, `IRO_FindFirst`) | 6 | 3,911/3,911 | all 6 | funccalls the same (not caught) / no two expressions the same / unsigned char always | 0 / 517 / 58 fail |
| `MsDos.c` part 4 (3 more, 47 of 59: `OS_OpenDir`, `OS_ReadDir`, `OS_CloseDir`) | 3 | 3,911/3,911 | all 3 (one case) | directory never opens | 1 fail |
| `CParser.c` part 2 (2 more, 16 of 20: `is_typeequal`, `is_typesame`) | 2 | 3,911/3,911 | both | no two pointer types the same | 463 fail |
| `CLToolExec.c` (partial: `DeleteTemporaries`; its deletion branch is not reached by the corpus) | 1 | 3,911/3,911 | yes | deletion always reported (not caught) / returns false | 0 / 3,603 fail |
| `CPrepTokenizer.c` part 2 (2 more, 10 of 16: `CPrep_SkipNewComment`, `skipendoflinematch`) | 2 | 3,911/3,911 | both | `//` comments run past the line end | 339 fail |
| `Files.c` (partial: `PBHGetFInfoSync`; its only output the corpus can see is none: the caller keeps just the modification date) | 1 | 3,911/3,911 | yes | file not found / block garbage (neither caught) / crash on entry | 0 / 0 / 771 fail |
| `CInline.c` (16) | 2 | 3,911/3,911 | both | local copies made global | 11 fail |
| `CompilerTools.c` part 2 (3 more, 19 of 23 matched: `LockGList`, `UnlockGList`, `aalloc`; four more matched "functions" are look-alike setters in other units) | 3 | 3,911/3,911 | all 3 | `aalloc` hands out one block | 56 fail |
| `CParser.c` part 3 (4 more, 20 of 24: `CParser_SetUniqueID`, `CParser_GetBoolType`, `CParser_RegisterNonGlobalClass`, `CParser_NewClassAction`) | 4 | 3,911/3,911 | all 4 | `bool` is `int` in C++ / class actions dropped | 21 / 170 fail |
| `CScope.c` part 2 (3 more, 11 of 15: `CScope_NSIteratorFind`, `CScope_SetupParseNameResult`, `CScope_FindClassMemberObject`) | 3 | 3,910/3,911 (one `enemy1` flip) | all 3 | single objects reported as overload sets | 2,932 fail |
| `Resources.c` (4 of 31) | 4 | 3,910/3,911 (same flip) | all 4 | flag answer flipped (not caught) / program's own resources not found | 0 / 2 fail |
| `COSToolsCLT.c` part 2 (`COS_LockHandleHi`, filed under `Resources.c` by gcmatch) | 1 | 3,910/3,911 (same flip) | yes | none (one call to `HLockHi`) | – |
| `Resources.c` crash control (shows `OS_UsingMacResourceForkInfo` runs) | – | – | – | crash on entry | 213 fail |
| `CLMain.c` part 5 (5 more, 23 of 41: the driver plugin's info callbacks `CLT_dummymain`, `CLT_GetDropInFlags`, `CLT_GetDropInName`, `CLT_GetVersionInfo`, `CLT_GetFileTypeMappings`) | 5 | 3,911/3,911 (one `enemy4` flip) | all 5 | version info is the compiler's (run with the older `CLMAIN5` by mistake: 3,909 fail together; rerun as `CLMAIN6` in the next batch) | see next batch |
| `cc-mips-ps2.c` (13 of 15) | 13 | 3,911/3,911 | all 13 | compiler's flags block too short / linker named "Linker Tool Stub" | 3,911 / 3 fail |
| `ElfImporter.c` (8 of 9) | 8 | 3,911/3,911 | all 8 | target list is the compiler's | 2 fail |
| `CParser.c` part 4 (`CParser_SetCFMFlags`, `CParser_NewAliasObject`; close to mweppc) | 2 | 3,911/3,911 | both | alias offsets off by 4 | 139 fail |
| `CScope.c` part 3 (`CScope_SetFunctionScope`; close) | 1 | 3,911/3,911 | yes | member functions see no `this` | 454 fail |
| `CClass.c` (5), `CClass_MemberObject`, `CClass_CheckEnumAccess`) | 5 | 3,911/3,911 | all 5 | no destructors / no member objects found | 222 / 1 fail |
| `CFunc.c` part 2 (`CFunc_NewDeclBlock`; close) | 1 | 3,911/3,911 | yes | every block index 0 | 2 fail |
| `CLMain.c` part 5 rerun as `CLMAIN6` | – | – | – | version info is the compiler's | 2 fail |
| `CDecl.c` (9)) | 3 | 3,911/3,911 (one `enemy2` flip) | all 3 | template classes not completed there | 23 fail |
| `CParser.c` part 5 (`TypedefDeclInfo`; close) | 1 | 3,911/3,911 | yes | typedef'd `const` dropped | 27 fail |
| `CException.c` part 2 (`CExcept_CanThrowException`; close: ours knows one more runtime object, 0x57c754) | 1 | 3,911/3,911 | yes | nothing throws | 220 fail |
| `CPrepTokenizer.c` part 3 (`skipendofline`, `lexidentifier`; close) | 2 | 3,911/3,911 | both | skipped lines run on to the end of the file | 243 fail |
| `PCode.c` (2)) | 2 | 3,911/3,911 | both | every label index 0 | 1 fail |
| `CPrep.c` part 2 (`CPrep_BrowserFileOffset`, `CPrep_UnLex`; close) | 2 | 3,911/3,911 | both | offset 0 (not caught: browse output is off; a crash control follows next batch) / – | 0 |
| `CExpr.c` (10), `CExpr_ParseSizeof` (close)) | 3 | 3,911/3,911 | all 3 | no illegal-operand errors / warning whatever the options / `sizeof` one too big | 1 / 53 / 227 fail |
| `CExpr2.c` part 2 (`CExpr_NewENode`, `CExpr_IsTempConstruction`; close) | 2 | 3,911/3,911 | both | no temporary constructions recognised | 2 fail |
| `IroJump.c` (1) | 1 | 3,911/3,911 | yes | no gotos chained | 65 fail |
| `CTemplateTools.c` part 2 (`CTemplTool_RemoveOuterTemplateArgumentNameSpace`; close) | 1 | 3,911/3,911 | yes | name spaces left in place | 24 fail |
| `CPrep.c` part 2 crash control (shows `CPrep_BrowserFileOffset` runs) | – | – | – | crash on its fallback path | 19 fail |
| Batch of 2026-11-07 evening (not matched by gcmatch; each follows mweppc's logic where it has the function): `CScope.c` part 4 (10: setup, qualified and name-space name lookup, insert, the name-space iterator, local tag type, associated name spaces), `CParser.c` part 6 (~25: object constructors, update, unique names, name concat, try blocks, callback actions, const/volatile queries, linkage, virtual and pascal queries, anonymous unions), `CExpr2.c` part 3 (`makemonadicnode`, `CExpr_MakeObjRefNode`, `create_objectnode(2)`, `integralpromote`, `iszero`, `CExpr_IsOne`; our integer constants are 128-bit), `CPrep.c` part 3 (`ismacroname(3,4)`, `macrotest`, stream state, `goendofline`), `CInline.c` part 2 (5), `CClass.c` part 2 (constructor/destructor lookup, object access), `CExpr.c` part 2 (`checkreference`, `pointer_generation`, `expression`), `CPrepTokenizer.c` part 4 (14 token functions and `lookahead_noeol`), `CodeGen.c` (5), `CError.c` part 2 (the message buffer, 7), `CMachine.c` (8), `CInit.c` part 2 (5), `CMangler.c` part 2 (5), `CFunc.c` part 3, `CABI.c` (6), `CTemplateTools.c` part 3, `CExprConvMatch.c` (2), `IroUtil.c` part 3 (2), `CBrowse.c` (1), `msl_ctype.c` (3), `CDecl.c` part 2, `CTemplateClass.c` (5), `IroLinearForm.c` (2) | 123 | 3,911/3,911 (one `enemy4` flip) | each unit's control | 22 controls: CSCOPE4 3,906, CPARSER6 794, CEXPR2C 294, CPREP3 129, CCLASS3 254, CEXPR4 122, CPREPTOKENIZER4 448, CODEGEN 2, CERROR2 3, CMACHINE 1, CINIT2 32, CMANGLER2 2, CFUNC3 1, CABI 224, CEXPRCONVMATCH 1, IROUTIL5 36, MSLCTYPE 4, CTEMPLATECLASS 25, IROLINEARFORM 36 fail; CPARSER6B (lock token kept) and CINLINE2 (heap freed early) not caught, crash controls next batch | see column |
| Batch of 2026-11-07 late evening (not matched by gcmatch unless noted): `CPrepTokenizer.c` parts 5 and 6 (every keyword function of the tokenizer table, table-driven with each word's option checks, plus the operator, '.', 'L', '@' and number functions), `CPrep.c` part 4 (`notendofline`, `pushifstate`, `popfile`, `CPrep_GetFileOffsetInfo`), `CParser.c` part 7 (`CParser_Setup`), `CScope.c` part 5 (`CScope_GetLocalObject`, `CScope_PossibleTypeName`, `CScope_GetTagType`, `CScope_FindNonClassObject`), `CInline.c` part 3 (storage flags, ID translation, default-function actions), `CException.c` part 3 (`CExcept_CanThrowCheck`; its callback 0x4ec6c0 is not in x86map, so ours passes its own copy), `CDecl.c` part 3, `CClass.c` part 3 (`CClass_CanAccess`), `CExpr2.c` part 4 (`CExpr_AllBitsSet`, a list copy), `CError.c` part 3 (qualifiers, object names), `CFunc.c` part 4 (`CFunc_DeclareFuncName`), `mips.c` (6) | 61 | 3,911/3,911 (`enemy2`, `enemy4` flips) | each unit's control | CDECL3 222, CEXCEPTION3 3, CSCOPE5 488, CEXPR2D 2, MIPS 369, CPREPTOKENIZER5 313, CPREPTOKENIZER6 188, CPREPTOKENIZER6B 2,736, CPREP4 160, CPREP4B 324, CFUNC4 623, CPARSER7 635 fail; batch 5's crash controls CPARSER6B 850 and CINLINE2 62 fail; CCLASS4 and CINLINE3 not caught, crash controls next batch | see column |
| Batch of 2026-11-07 night (not matched by gcmatch; mweppc's logic unless noted): `CError.c` part 4 (namespace, type, unqualified and full function names, argument lists), `CMangler.c` part 3 (function and data link names), `CExpr.c` part 3 (`pointer_generation2`, `CExpr_PointerGeneration`, `s_expression`, `CExpr_ScanExpressionList`), `COptimizer.c` (3), `CInline.c` part 4 (object address references, the function-reference lists, `CheckDependencies`), `CExpr2.c` part 5 (`CExpr_SearchExprTree`), `CompilerTools.c` part 3 (`CHash`, `InitNameHash`), `csetups.c` (5), `CDecl.c` part 4 (`CDecl_NewPointerType`, the pointer size), `CPrep.c` part 5 (`insertmacro`, `InsertSpecialMacro`), `mips.c` part 2 (the opcode bit-field parser and setters), `CMachine.c` part 2 (type, class and struct alignment), `CFunc.c` part 5 (`CFunc_BlockStatement`), `IroCSE.c` (3) | 41 | 3,911/3,911 (`enemy2`, `enemy4` flips) | each unit's control | CERROR4 38, CMANGLER3 100, COPTIMIZER 2,665, CINLINE4 93, CINLINE5 157, CEXPR2E 291, CPREP5 134, MIPS2 1,532, CMACHINE2 107, IROCSE 851 fail; batch 6's crash control CCLASS4 18 fail; CEXPR5 and CSETUPS caught only flips, reworked: CEXPR5 687, CSETUPS 2,522 fail; batch 6's CINLINE3 (storage class of inlined objects) changes no output, a crash control next batch | see column |
| Batch of 2026-11-08 small hours (not matched by gcmatch; mweppc's logic where it has the function, else from the exe): `CExpr.c` part 4 (`optimizecomm`, the `*`/`/`/`+`/`-` node constructors with operator overloading, `CExpr_RewriteConst`, `CExpr_IntegralConstExprType`, `CExpr_IntegralConstOrDepExpr`, small helpers), `CExpr2.c` part 6 (`CExpr_AdjustFunctionCall`, `CExpr_AliasTransform`, `makecommaexpression`, `isnotzero`, `CExpr_ConvertToCondition`), `CMachine.c` part 3 (qualifier alignment, struct layout offset, float conversions), `CInt64.c` (new: `CInt64_GetULong`), `CFunc.c` part 6 (`CFunc_IsLabel`), `CScope.c` part 6 (list walks), `CParser.c` part 8, `CPrep.c` part 6 (the pragma push), `CError.c` part 5 (`CError_VAErrorMessage`), `CDecl.c` part 5 (member function and struct type makers), `CClass.c` part 4 (`CClass_ThisSelfObject`, `CClass_CreateThisSelfExpr`, the vtable object), `CBrowse.c` part 2 (`CBrowse_Finish`, a range writer), `CodeGen.c` part 2 (branch opcodes, constant size, labels), `COptimizer.c` part 2 (the branch optimizer's walk), `IroUtil.c` part 4 (`IRO_LocateFather`, `IRO_LocateFather_Cut_And_Paste`, `IRO_ReplaceReferenceWithNode`), `IroLinearForm.c` part 2, `IroCSE.c` part 2, `mips.c` part 3, and new back-end units from the exe: `ObjGen.c` (5), `Intrinsics.c` (3), `IroPointerAnalysis.c`, `IroPointerAnalysisADTs.c`, `PCodeInfo.c` (2), `BE_binary.c` (3), `InterferenceGraph.c` (`coalesced_path`), `FunctionCalls.c` (2), `InstrSelection.c`, `IroFlowgraph.c` (`AddLabelSucc`), `Exceptions.c`, `DWARF.c` | 75 | 3,911/3,911 (`enemy4`, `enemyW` flips; a first run failed 631 cases: `CMach_StringToFloat` ported void, fixed by returning `ScanFloat`'s end pointer, and the other wrappers that end in a call now return the callee's result) | each unit's control | CMACHINE3 191, CMACHINE4 4 (`gmfood`), CEXPR2G 22, CEXPR6 692, CEXPR7 1,748, CEXPR8 3 (`uprollstr`), CFUNC6 57, INTRINSICS 23, CODEGEN3 51, BEBINARY 2,233, COPTIMIZER2 57, INTERFERENCE 2,753, IROFLOWGRAPH 1,222, CSCOPE6 89, CPREP6 315, CERROR5 569, DWARF 530, CINLINE3 566 fail; reworked after catching only flips: IROUTIL6 (a stderr mark when it runs) 642, INSTRSELECTION (every call emitted twice) 1,548, CCLASS5 (`this` typed as the class) 454, CDECL5 (struct tag never declared) 217 fail; still not caught: CEXPR2F (struct-result call node typing) and OBJGEN (`fn_48a170` kinds) fail only flipping `enemy*` cases, and FUNCTIONCALLS shows the two special call targets never occur in the corpus | see column |
| Batch of 2026-11-08 early morning (from the exe, mweppc's logic where it has the function): `InstrSelection.c` part 2 (6 selection helpers), `DWARF.c` part 2 (6 writers), `Exceptions.c` part 2, `IroFlowgraph.c` part 2, `CClass.c` part 5, `CDecl.c` part 6, `CScope.c` part 7 (4), `CPrep.c` part 7 (4), `IroLinearForm.c` part 3, `IroUtil.c` part 5 (`IRO_FindLabelNode` and 4 walks), `CInline.c` part 6 (3), `CFunc.c` part 7 (`CFunc_CheckReturnAddress`), `CABI.c` part 2 (`CABI_GetThisArg`), `PCode.c` part 2, `CPrepTokenizer.c` part 7 (`tdiv`), `IroLoop.c` part 3 (4), and new units `IROUseDef.c` (2), `StructMoves.c` (1), `IroUnrollLoop.c` (3) | 53 | 3,911/3,911 (`webnews`, `enemy4` flips) | each unit's control | DWARF2 281, CPREP7 137, IROLINEARFORM3 193, CINLINE6 81, CABI2 170, CPREPTOKENIZER7 351, INSTRSELECTION2 74 fail; IROLOOP3 184, CDECL6 80, EXCEPTIONS 416 fail (run with batch 10); not caught (fail only flipping `enemy*` cases): CFUNC7, IROUNROLL, CSCOPE7; the IROLOOP3 control first failed to build (a `#ifndef` left the `if` without a body), fixed | see column |
| Batch of 2026-11-08 evening (from the exe, mweppc's logic where it has the function): `CInt64.c` part 2 (`CInt64_Init`, `SetLong`, `SetULong`, `IsZero`, `IsOne`, the compares, `Not`, `Inv`, `AddWithCarry`, `Add`), `CompilerTools.c` part 2 (the heaps: `MoreHeapSpace`, `InitHeaps`, `GetTotalHeapSize`, `ReleaseHeap`, `FreeHeaps`, `freelheap`, `freeaheap`, `freeoheap`; `ClearNameHashIDs`, `GetHashNameNodeForPath`), `Intrinsics.c` part 2 (the runtime function names), `CodeGen.c` part 3 (the PS2 plugin's license checkout and checkin, 7), `CFunc.c` part 8 (`CodeGen_GetNewVarInfo`, `CFunc_SetupLocalVarInfo`, `CFunc_CheckLabels`, 4 more), `CDecl.c` part 7 (`CDecl_NewArrayType`, `CanCreateObject`, `CanAllocObject`, `CDecl_CheckDefaultArgs`, 3 more), `CPrep.c` part 9 (`CPrep_BrowserFilePosition`, `CPrep_Cleanup`, 3 more), `CException.c` part 4 (3), `CTemplateTools.c` part 2, `CInline.c` part 7 (2), `CScope.c` part 8 (`CScope_GetScope`, `CScope_RestoreScope`), `CInit.c` part 3 (`CInit_DefineTentativeData`, 1 more), `CExpr2.c`, `CRTTI.c`, `IroUnrollLoop.c`, `IROUseDef.c` (1 each), and new `StackFrame.c` (`init_frame_sizes`, `assign_local_memory`, 1 more) | 73 | 3,911/3,911 (`webnews`, `enemy4` flips) | each unit's control | CDECL7 1075, CFUNC8 1851, CINT64 908, CODEGEN4 18, COMPILERTOOLS3 21, CSCOPE8 57, STACKFRAME 660 fail; not caught (fail only flipping `enemy*` cases): CEXCEPTION4, CINIT3, CPREP8, INTRINSICS2, reworked into stderr markers and rerun with batch 11 | see column |
| Batch of 2026-11-08 night, part 1 (from the exe, mweppc's logic where it has the function): `CError.c` part 6 (`CError_BufferAppendType`, `CError_GetTypeName`, the template argument writers), `CMachine.c` part 4–5 (`CMach_CalcFloatMonadic`, `CMach_CalcFloatConvertFromInt`, `CMach_InitFloatMem`, `CMach_IsUnsignedType`, `local_alignment`, `CABI_PassByReference`, the one-float struct tests, the init list), `CScope.c` part 9 (argument dependent lookup: `CScope_BuildTypeAssociatedNameSpaceList`, `CScope_ArgumentDependentNameLookup`, `CScope_NSIteratorNonClassFind`), `IrOptimizer.c` part 2 (`CountARef`, `CountDoubleInd`, `CountUsage`), `CDecl.c` part 8 (the implicit constructor and destructor types), `CABI.c` part 3 (`CABI_AllocateBases`, `CABI_FindZeroVirtualBaseMember`, `CABI_MakeThisExpr`, `CABI_ConstructorCallsNew`), `CParser.c` part 9 (`isdeclaration`, `getthetype`), `CodeGen.c` part 4 (`return_class`, `set_return_class_liveness`, the parameter register lists, 1 more), `CPrep.c` part 11 (the file offset info), `CFunc.c` part 10 (conditional jumps, the inline wrapper), `ObjGen.c` part 3 (`ObjGen_Setup`, the symbol table, `Sections_Init`) | 43 | 3,911/3,911 (`enemy2` flips) | each unit's control | CABI3 252, CDECL8 2 and OBJGEN2 2 (both later found to be noise: rerun on 2026-11-09 on the 708 cases that can reach it, CDECL8 fails none (its `audio.cpp` fail was noise), and OBJGEN2's `pgrider.cpp` fail does not repeat; neither is caught), CERROR6 3, CERROR7 171, CFUNC9 1003, CMACHINE5 95, CMACHINE6 160, CODEGEN5 61, CPARSER9 1733, CPREP9 516, CSCOPE9 29, IROPTIMIZER 117 fail; batch 10's reworked markers: CINIT3 3109, CPREP8 20 fail, INTRINSICS2 never prints (its functions are not reached by the suite); the controls now reuse the reference compiler's results cached by the suite run (`--ref-cache`), a no-op control passing 3,911/3,911 | see column |
| Batch of 2026-11-08 night, part 2: `CExpr.c` part 5–7 (`CExpr_IntegralConstExpr`, `CExpr_CheckUnusedExpression`, `conditional_expression`), `CFunc.c` part 11 (`parameter_type_list`, the loop weights, `CFunc_RestoreBlock`, a parameter list test), new `CTemplateNew.c` (`CTempl_Instantiate`), `CClass.c` part 6 (`CClass_AssignmentOperator`, the virtual override search, a qualifier move), `CDecl.c` part 9 (`CDecl_AddDefaultConstructor`, `CDecl_AddDefaultDestructor`), `CScope.c` part 10 (`CScope_SetClassDefScope`), `CInline.c` part 7 (expression copies), `PCode.c` part 3 (`initpcode`, `makepcode`, `makepcblock`, labels and branches, `appendpcode`, `isel_finish`) | 26 | 3,911/3,911 (`enemy2` flips) | each unit's control | CCLASS6 38, CDECL9 77, CEXPR9 2238, CEXPR10 432, CFUNC10 57, CINLINE7 430, CTEMPLATENEW 48, PCODE2 264 fail | see column |
| Batch of 2026-11-08 night, part 3 (the back end's register allocation, from the exe): new `RegisterInfo.c` (`init_registers`, `assign_dedicated_register`, `count_allocatable`, `available_mask`, `next_nonvolatile`, the register count saves, `count_scratch_registers`, `is_vu0_vector_class`, 2 more), new `Alias.c` (`initialize_aliases`, `hash_alias`, `create_alias`, the lookup, `make_alias`, `make_alias_set`, `add_alias_member`, the value numbering), `InterferenceGraph.c` part 2 (the sparse register sets, liveness: `BE_ComputeLiveness`, `compute_liveness`, `BE_UpdateLive`, `BE_IsDeadInstruction`; `init_coalesced_nodes`, `build_and_coalesce`, `early_coalesce_registers`, `Pipeline1_Run`, 2 list helpers), `PCodeInfo.c` part 3 (`is_rematerializable`, `emit_loop_align`), `CPrep.c` part 12 (`CPrep_SetSourceFile`, a token skipper) | 40 | 3,911/3,911 (`enemy1`, `enemy4` flips) | each unit's control | ALIAS 2141, CPREP10 175, INTERFERENCE2 2846, INTERFERENCE3 2480, PCODEINFO2 3, REGISTERINFO 3869 fail | see column |
| Batch of 2026-11-08 night, part 4 (from the exe; mweppc's where it has the function): new `LocalCSE.c` (the value tables, register kills, the store helpers, `cse_extended_block`, `LocalCSE_Run`, `Peephole_AddRule`, the peephole block pass, `Peephole_Run`), new `BE_macro.c` (`emit_prologue`, `emit_epilogue`, `BE_RemoveDeadInstructions`, the local offset and frame immediate fix-ups, 1 more), `IroLinearForm.c` part 4 (`MarkSubscript`, `IRO_PreLinearize`, `IRO_UpdateNodeFlags`), `IroBitVect.c` (`Bv_SetBit`), `IroFlowgraph.c` (`StartNode`), new `CPrec.c` (`SetupPrecompiler`, `CleanupPrecompiler`), new `CPreprocess.c` (`CPrep_PreprocessDumpNewLine`), `IroUnrollLoop.c` (`IRO_LoopUnroller`) | 28 | 3,911/3,911 (`enemy1`, `enemy4` flips) | each unit's control | BEMACRO 739, CPREC 3869, IROBITVECT2 2854, IROFLOWGRAPH2 2846, IROUNROLL2 1065, LOCALCSE 478, LOCALCSE2 2480 fail; not caught (no beyond-baseline fail, or only `enemy*` flips): CPREPROCESS, IROLINEARFORM4, reworked into stderr markers and rerun with batch 17 | see column |
| Batch of 2026-11-08 night, part 5: new `IroTransform.c` (`IRO_SimplifyNodes`, `IRO_PreTransformTrees`, `IRO_IsRegisterLocal`, the per-node simplifiers and the tree walk, 4 more), `CBrowse.c` (`CBrowse_Setup`), `CInline.c` (`CInline_GenerateDeferredFuncs`), new `StdTargetWarningHelpers.c` (`TargetSetWarningFlags`), `IroVars.c` (`IRO_ClearVarLinks`, 1 more), `IroCSE.c` (`IRO_AddExpression`), `CException.c` (the exception expression transform walk) | 16 | 3,911/3,911 (`enemy1`, `enemy4` flips) | each unit's control | CEXCEPTION5 2849, CINLINE8 161, IROCSE3 2146, IROVARS3 2846, STDTARGETWARN 372 fail; not caught (no beyond-baseline fail, or only `enemy*` flips): CBROWSE3, IROTRANSFORM, reworked into stderr markers and rerun with batch 17 | see column |
| Batch of 2026-11-08 night, part 6: `CDecl.c` part 10 (`CDecl_CompleteClass`, `CDecl_AddDefaultCopyConstructor`, `CDecl_AddDefaultAssignmentOperator`), `CClass.c` part 7 (`CClass_CopyConstructor`, `CClass_PathCleanup`, `CClass_CheckStaticAccess`), `CExpr.c` part 8 (`CExpr_NewDyadicNode`, `CExpr_IntConstConvert`, pointer addition, 2 more) | 11 | 3,911/3,911 (`enemy1`, `enemy4` flips) | each unit's control | CCLASS8 249, CDECL10 707, CEXPR11 652, CEXPR12 754 fail; not caught (no beyond-baseline fail, or only `enemy*` flips): CCLASS7, reworked into stderr markers and rerun with batch 17 | see column |

One-off flips: a full run may show one or two `enemy*.cpp` cases as intermittent (they differed
once and matched on retry). The original flips against itself on these files too. Sampling the
42 `enemy` cases 30 times each on one frozen hybrid build (2026-11-06, `MWRECOMP_ONLY` choosing
the hooks): no hooks 11 flips in 1,260 runs, all hooks 12, only the units ported before 11-06
15, only those ported on 11-06 20. The hooks make no clear difference; the rate moves with
conditions instead (the same no-hooks sample gave 2 in 1,260 on an idle machine and 11 while
other suites ran), which fits time-dependent leftovers such as tick counts from the user-break
checks. See "The original is not fully deterministic".

## Reference: mweppc (GameCube compiler decomp)

[doldecomp/mweppc](https://github.com/doldecomp/mweppc) (CC0) decompiles the GameCube `mwcceppc.exe`
2.x (front end 2.4.7, about 4,000 functions exact), with a clang build that behaves like the original.
The tools look for a checkout next to this repository or where `MWEPPC` points, with the original
GameCube compilers in its `build/compilers/` (decomp.dev archive).

`tools/gcmatch.py` compares every function of ours with GC 2.0's. Addresses (via `.reloc`) and
call targets are masked; results are in `docs/data/gcmatch_GC_2_0.json`.

| | functions |
|---|---|
| ours | 4,403 |
| byte-identical to a GC 2.0 function | **1,078** |
| same instruction shapes (≥ 90%) | 48 |
| no match | 3,277 |

- **Byte-identical:** mostly the CodeWarrior command-line driver (`CL*.c`, `Option.c`, `Files.c`,
  the plugin and dropin glue), the runtime and MSL, `Targets.c..MsDos.c`, `Resources.c`, and a few
  front-end helpers. Port these first: copy the mweppc source, hook the function, test.
- **Front end and optimizer** (`CParser`, `CExpr`, `CDecl`, `CClass`, templates, `Iro*`): similar but
  not identical. Of the functions in files both compilers have, 664 are ≥ 90% similar by fuzzy
  instruction comparison, 544 are 70–90% and 1,935 are below 70%. mweppc gives names, structure and
  types, but each function needs checking against our binary, and struct layouts will differ.
- **MIPS back end** (`mips.c`, `InstrSelection.c`, `MIPS_Peephole.c`, `ELFgen.c`, `ObjGen.c`,
  `InlineAsmMIPS.c`, `VU0`, scheduler, `DWARF.c`): ours alone. `decomp/` already covers much of its logic.
- GC 1.3 to 2.7 are all about equally close to ours (≈ 960 identical functions each); GC 1.0–1.2.5
  much less. The GC 3.0 alphas and Wii builds scored 0 only because the quick function finder in that
  scan doesn't handle them.

## Next steps

1. **Decide how to reproduce leftover memory** (section above: the `hascall` of float-constant call
   arguments comes from stale stack bytes via reused memory; the `la` rematerialization check reads
   one directly; the full list is under "All readers found"). Still open: confirm or rule out the
   `IRO_RemoveLabels` byte, and write a test that puts a leftover on a float constant's `cost`.
2. **Grow the corpus where coverage is low** (the gaps table): a C++ test suite, driver options and
   error paths, Csmith-style random programs, PCH. Before porting a function, check in
   `coverage.json` that the corpus enters it, and run all of its blocks. The `magicu` bug passed every
   probe until `tests/d01_divconst.c` was added.
3. **Port the byte-identical driver/runtime functions** from mweppc (1,078 functions) to prove the
   pipeline at volume, and adopt mweppc's x87 emulation and headers.
4. **Front end and IRO** module by module, using mweppc as the base and our binary as the authority.
5. **MIPS back end** from `decomp/` and Ghidra.
6. **Standalone build**: once nothing calls into the original, link `src` with the startup
   code into a normal exe and run the whole corpus against it.
