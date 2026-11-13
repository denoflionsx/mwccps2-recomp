# Output that depends on memory the compiler never wrote

The original's output is not a function of its input alone. It reads memory it never wrote, and
what it finds there depends on what ran before: earlier functions of the same file, the paths of
the files it opened, the code that was called on the way. Everything below was found by tracing
a difference to its cause. What follows from it for this project:

- A port has to leave the same bytes behind as the function it replaces, or be kept away from the
  readers (the first `mem.c` port failed three game units over the registers its `memset` saved).
- An extension must not leave anything behind at all: `HOOK_WRAP` handlers run on their own stack.
- Even the address of ported code shows ([HYBRID.md](HYBRID.md), "Where the hybrid's image sits").

## Paths and file names (2026-11-13, measured on .hack//Infection under wibo)

- **The length of the working directory's path reaches the code.** Opening an included file makes
  a name node of its full path in the permanent heap (0x43b582), so every later object there
  moves by the path's length; debug records hold one full path per function. A floating constant's
  node can take its stale `hascall` byte from a pointer into that heap (byte 1 of the pointer:
  zero only for the first 256 bytes of a 64 KB block). With one game checkout, units changed
  between directory path lengths (`WORLD::Init` for 81 to 95 characters, for one).
- **`#line N "path"` in the main file moves code in the original.** The main file has no name
  node; the directive's handler (0x440420) makes one of the real path and pins it. 24 of 219 game
  units compiled differently with such a directive on the main file's first line; in headers, or
  without the path, none did. The handler also renames the file through the driver's path tables
  (0x4419f0), which depends on which directories exist.
- The `line_path` extension ([EXTENSIONS.md](EXTENSIONS.md)) is built around both: with the
  directive on the first line of every file, 219 of 219 units compile to the same code as without,
  at every directory path length from 10 to 110.

`MWRECOMP_ALLOC_TRACE` (`ext/alloc_trace.c`) prints what the compiler's heaps hand out; the first
line that differs between two runs is where they parted. That is how the name node above was found.

## Leftovers in operands and nodes

The original's output can depend on leftover memory contents, not just on its input. The section
heading is historical: the cause turned out to be uninitialized memory, not address ordering.

1. **The license DLL.** With mweppc's 2.5 KB FlexLM stub in place of the real `LMGR326B.dll`, the
   original exe compiled `src/source/acces.cpp` (`-O4,p`) to one of two schedules, roughly half the
   time each (18 vs 22 in 40 runs). With the real DLL it gives the same one in 40 of 40 runs. The real
   DLL loads at a fixed address and pulls in `USER32` and `ADVAPI32`. So the hybrid, at
   the time a stand-in for that DLL, forwarded to the real one and loaded at a fixed base
   (`0x60000000`; it had defaulted to `0x10000000`, the real DLL's own base), and gave the stable
   schedule in 40 of 40 runs. The merged exe leaves the license DLL alone.
2. **Rare variation in the hybrid.** Even with no hooks installed, the hybrid takes another path on
   about one game unit per 195-unit run. The changed units are always `enemy*.cpp`, and the change is
   the order in which float constants (`1.0`, `0.5`, `0.06`) are loaded into registers. About 1,200
   compiles of the original against itself showed none. Run alone, both the original and the hybrid
   gave the same code in 30 of 30 runs.

   Heap addresses are not the cause: shifting every heap allocation with `MWRECOMP_PAD` (up to 1 MB)
   never changed the output. The cause is below.

### Found: call arguments that are float constants read an uninitialized flag

Traced on `enemy3.cpp` (`-O4,p`), 2026-11-06. The chain, start to end:

1. **Register operands carry stack leftovers.** The operand builders `fn_497fb0` (register from
   vreg/class/count) and `fn_4980e0` (operand from an expression result) build a 22-byte operand
   record in a local and copy all 22 bytes out, but set only bytes 0–5 for a register (kind, class,
   count, vreg). Bytes 6–21 are whatever was on the stack. (`fn_4980e0`'s memory case leaves byte 1
   and bytes 10–21; its kind-3 case copies `fn_4980b0`'s record, whose byte 1 and 18–21 are stale too.)
2. `build_call` (`0x4a4560`) builds the `jal`'s second operand, `$ra` (vreg 31, class 0, count 2), with
   `fn_497fb0` and copies it into the PCode instruction (instruction offset `+0x4a`, so its byte 11
   is at `+0x55`).
3. **The per-function memory is reused.** After the function is compiled, that memory holds the next
   function's IR. `IRO_BuildExpr` (`0x4c3c10`) copies a 40-byte expression node out of it with
   `rep movsd`: the stale byte lands at **offset 5 of an `EFLOATCONST` node (kind `0x33`) = `hascall`**.
4. **Nobody sets it.** The back-end pre-pass `ISel_PrepareExpr` returns early for float constants it
   builds inline (always at speed, `,p`), leaving `cost` (+1) and `hascall` (+5) as they were
   (den's notes on `InstrSelection.c`).
5. **Call lowering reads it.** `fn_4a4960` (`FunctionCalls.c`) evaluates first every argument whose
   `hascall` is nonzero (`cmp byte ptr [ecx+5], 0` at `0x4a4a33`). A nonzero leftover moves that
   float constant's `lui/ori/mtc1` ahead of the other arguments: the reordering seen in the game.

Why it varies: the stack bytes in step 1 are left over from earlier calls, and some of them are
parts of pointers. Pointers change with ASLR, and the coverage traps and the stub DLL change what
the earlier calls left there. In the original with the real DLL, the byte that decides this case is
nonzero on every run we've seen.

Evidence: in the experiment build, zeroing byte 11 of exactly one operand (call #56 of
`fn_497fb0`, from `build_call`) gives the flipped `enemy3.cpp`, and any nonzero value the normal one.
A hardware watchpoint on that byte follows it through `build_call` → `fn_4a77b0` → `IRO_BuildExpr`
to the read in `fn_4a4960`.

**How much it matters.** Forcing all unset operand bytes to one value, against the original, over the
whole corpus: `00` changes 8 game units, `5a` 12 (mostly different ones), `ff` 11. So the original
has zero leftovers at some sites and nonzero at others. **No constant fill matches;** a rebuilt
compiler has to reproduce the actual leftover values. `5a`/`ff` also change `p136_spill.c` at `-O1`
(more spilling, a bigger frame): a **second reader**, traced the same way. Only byte 1 of one
operand matters there: call #24 of `fn_4980e0`, a memory operand (kind 2 → 4) built by
`fn_48bd60` (`mips.c`) for an `la r, sym`. `fn_4980e0` never sets byte 1 for memory operands, and
`is_rematerializable` (`0x49af10`) requires it to be 0 (`cmp byte ptr [ecx+0x4b], 0`) before an `la`
of a small global or a stack object is rebuilt at each use instead of spilled. The original sees 0
in this probe.

### All readers found (2026-11-06)

**The builders.** `tools/opbuilders.py` finds every function that builds a 22-byte operand
record in a local and copies it all out. Seven do, all in `VReg.c` (table at the top of
[src/backend/VReg.c](../src/backend/VReg.c)); `fn_4981b0` (labels) clears its record first. The GPR and
FPR builders (`fn_497a30`, `fn_497a70`) don't even set the register count (bytes 2–3).

**Which leaks reach the output** (`tools/leakhunt.py`, results in `docs/data/leaks.json`).
The experiment build replaced all seven builders. Base: every unset byte `00`. Variant: one
builder's unset bytes (then one byte) `ff`, and again `01`. Whole corpus, 3,875 cases each; base
against base: 0 differences.

| builder | leaked byte | cases changed | reader (traced with `tools/tracecase.py`) |
|---|---|---|---|
| `fn_497fb0` register | 11 | 6 | `hascall` of an `EFLOATCONST` argument (enemy3) |
| | 13 | 2 | same (enemyU) |
| | 21 | 1 | same (enemyL) |
| `fn_497ff0` new pair | 9 | 5 | same (boss01; also passed by value through `fn_48bd60` → `fn_498250`, unused there) |
| | 17 | 2 | same (area02) |
| `fn_4980e0` register | 17 | 1 | same (enemyL) |
| `fn_4980e0` memory | 1 | 6 | `is_rematerializable`, `la` symbol operand byte 1 (field, three calls) |
| | 19 | 2 | `hascall` of an `EFLOATCONST` argument (audio) |
| `fn_497a30`, `fn_497a70`, `fn_498070`, `fn_4980b0` | any | 0 | — |

`ff` and `01` always change the same cases: every reader seen is a zero/nonzero test. Which byte
of an old operand ends up under the float constant's `hascall` depends only on where the node
lands in the reused memory, hence the many different byte numbers for one reader.

**The readers, then:**

1. **`hascall` (+5) of inline float constants**, read by call lowering `fn_4a4960` (an argument
   with a nonzero byte is evaluated first). Lines 880/893 of the same function read it for the
   callee and the struct-return target too, but those are never float constants. Seen in 7 of 8 rows.
2. **`is_rematerializable` (`0x49af10`)**, byte 1 of an `la`'s symbol operand. Seen in 1 row, and in
   `p136_spill.c`.
3. **`cost` (+1) of inline float constants**, read by `ISel_EvalBinary` (`0x49b2d0`: right operand
   first when `right->cost > left->cost + 2`), and summed into every parent's cost by
   `ISel_PrepareExpr` (`0x4b9af4`). **Static only:** the leak hunt never showed a value-dependent
   difference, so no leftover landed on a float constant's cost in the corpus. It is the same
   kind of leftover as `hascall` (`ISel_PrepareExpr` leaves both alone), so expect it in other code.
   A scan for comparisons on byte +1 in the back end finds no other reader of expression-node
   cost; the many optimizer hits are the IR's own node kind byte.
4. **Possible: `IRO_RemoveLabels` (`0x4d31e0`)** removes a block's label when the block's byte
   `+0x39` is 0. In one trace (enemyL) a leftover reached that byte with no clearing write in
   between, although `fn_4c0760` (called first) and block creation normally set it. Not seen to
   change output; needs confirming.

**Static census.** `tools/opfields.py` counts reads and writes of every operand field inside
PCode instructions (`+0x34 + 22·k`). It's too noisy to use as it stands: other structures in the
back end share those displacements.

**What this means for the rebuild.** Exact matching needs the same leftovers:
- the same per-function arena allocator (same sizes, order and reuse), so a new node lands on the
  same old bytes;
- every struct copied with its uninitialized parts, exactly as the original does;
- the stack contents that the operand builders pick up, at least for the bytes that reach a reader.

The first two follow from a faithful decomp. The third is the hard part: it means modelling what the
original's earlier calls left on the stack. Options, to decide later: emulate a shadow stack for the
few builders that leak; or find the readers (`hascall` of `EFLOATCONST`, the spill case) and compute
the value the original would have seen. mweppc reproduces similar cases one by one.

### Tools for this kind of hunt

All in the hybrid, off unless the variable is set (`hybrid/hook.c`, `hybrid/poke.c`):

| Env var | Effect |
|---|---|
| `MWRECOMP_PAD=n,...` | `GlobalAlloc` these sizes first: shifts every heap address |
| `MWRECOMP_COV_ARM=lo,hi` / `MWRECOMP_COV_SKIP=lo,hi` | arm only / leave out a range of coverage traps |
| `MWRECOMP_POKE=lo,hi,byte` | at `0x49c025`, fill `[esp-hi, esp-lo)` with `byte` (no exception) |
| `MWRECOMP_WATCH=off,max,depth,mask` | hardware watchpoint on `esp-off` at `0x49c025`; logs each access, follows string-move copies |
| `MWRECOMP_AFTERCALL=off,byte` | after `build_call` returns, write `byte` at its entry `esp-off` |

`tools/trapbisect.py <case>` bisects the coverage traps down to the ones that flip a case.
`tools/leakhunt.py` runs the per-builder, per-byte leak experiment over the corpus, and
`tools/tracecase.py <case> <id> <byte>` finds the call whose leak matters and traces it to
its reader (`MWRECOMP_OPFILL_TRACE_MASK` picks which copies to follow; the watchpoint follows
`movs`, `rep movs` and `push [reg+disp]`). A `mwrecomp.env` file next to the exe
(`NAME=VALUE` lines) sets these variables for that folder only, so two builds can be compared.
`src/backend/VReg.c`, built with `--cflags=-DRECOMP_OPFILL_EXPERIMENT`, replaces all seven operand
builders and fills their unset bytes from settings (`MWRECOMP_OPFILL`, `_ALT`, `_IDS`, `_BYTES`,
`_NTH`, `_LOG`, `_TRACE=id,n,byte`; documented at the top of the file). Only DR0 is usable: arming a second
debug register made the process exit silently, and long watches on the stack can do the same.

## The original is not fully deterministic

Running the original twice on the same input gave different objects in about 4% of cases, all with `-g`.
In every case only `.debug` differed:

- **DWARF entry order** and the references between entries change from run to run. They probably
  depend on heap addresses, which Windows randomizes. mweppc reports the same class of
  host-dependent behaviour.
- **Anonymous type names** (`@anon3`, `@anon10`) are numbered in a run-dependent order.
- **Crash messages** print the faulting address, which also varies.

So `difftest.py` compares `.debug` sections as sorted lists of entries, with address and reference
values masked (including the references inside `AT_mod_u_d_type` and `AT_subscr_data` blocks) and
`@anonN` numbers removed. It compares `.rel.debug` as a multiset of (type, symbol), and masks the
address in "At address:" lines. Five runs of the original against itself then pass 915/915.
Negative checks: `-O4,p` vs `-O2` objects and an `int`→`long` struct change are still reported as
different.

This weakens the check for `-g` output: a candidate that points a type reference at the wrong entry
would still pass. To fix that: compare entries by resolving each reference to its target's content.

**37 corpus cases crash the original** (access violation; the VU0 probes and `p62_bmem` under
several flag sets). A rebuilt compiler must crash the same way, with the same message (address masked).
