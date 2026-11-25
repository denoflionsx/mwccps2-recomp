# Extensions

Behaviour the original compiler does not have, for projects that rebuild a game byte for byte with
it. An extension is attached with `HOOK_WRAP` ([HYBRID.md](HYBRID.md)) and does nothing unless it
is asked for, so a build that has them still gives the original's output:

```
MWRECOMP_EXT=line_path,stale_hascall,stale_helper_args,name_counter      # or: all
```

Set it in the environment, or as a line of `mwrecomp.env` next to the exe. The sources are in
`ext/`; `python tools/build.py --ports none` builds the original's code with only the extensions.

They exist because the compiler's output depends on memory it never wrote or never cleared
([ENTROPY.md](ENTROPY.md)). A matching project cannot reproduce the machine and the session the
game was built in; these let the sources say what those contributed.

## `line_path`: `#line N "path"` names the file in debug records

In the original, a file name in `#line` changes what messages call the file and nothing else:
debug records keep the real path. With the extension, the records of functions compiled after the
directive carry the given path, exactly as written:

```c
#line 2 "D:\\work\\game\\source\\main.cpp"
```

- `\\` stands for one backslash, as in a string literal (and as other compilers read `#line`).
- Messages keep naming the file that is really there.
- On the **first line of a file**, the path names the file from the moment it is opened, so the
  real path never enters the compiler's tables. With that line in every source and header, the
  debug records *and the code* are the same wherever the files are, and the code is the same as
  without the directives. (`2`, so that the lines keep their numbers.)
- A `#line` directive, with or without a path, leaves no trace on the compiler's stack, so adding
  one does not change the code that follows.

Measured on .hack//Infection (219 units, wibo): with the directive on the first line of all 578
sources and headers, every unit compiles to the same code as the unmarked sources do with the
original compiler; the debug records hold the directives' paths; and no unit changes over working
directory path lengths 10 to 110 (unmarked, some do).

## `stale_hascall`: `#pragma stale_hascall`

Expression nodes come from a heap that is reused from one function to the next and never cleared,
and the code that builds a literal's node leaves the node's `hascall` byte unset. The back end
never sets it for a floating constant it loads inline, and call lowering evaluates an argument
whose `hascall` is nonzero before the others. So whether a constant argument is loaded early
depends on what the node's memory held before. The pragma says what it held:

```c
#pragma stale_hascall on        // every node allocated from here on finds the byte set
#pragma stale_hascall off       // ... finds it clear
#pragma stale_hascall reset     // ... finds what the memory held: the original's behaviour
#pragma stale_hascall(1, 0, _)  // the nodes of the next numeric literals, integer or floating, in
                                // source order: set, clear, as it is. Then the state above applies.
```

Only the leftover is supplied. Code that sets the byte itself still does, so nothing changes for
nodes whose `hascall` was never stale. The state starts again with each source file, from
`MWRECOMP_HASCALL` (`on` or `off`; unset means `reset`). With the extension off, the pragma is the
original's to judge: it ignores it.

```c
callee(a, 1.0f, 2.0f, 3.0f);      // loads 1.0, 2.0, 3.0 when the bytes are clear

#pragma stale_hascall(1, 0, 1)
callee(a, 1.0f, 2.0f, 3.0f);      // loads 1.0, 3.0, then 2.0
```

`MWRECOMP_HASCALL_TRACE=1` prints each numeric literal's node to stderr: file, line, kind, the
literal's bits, the byte as found (`stale`) and as left (`hascall`). That is how to learn what a
build that already matches relies on.

`MWRECOMP_HASCALL_REPLAY=file` plays a recorded run back: the file holds one `0` or `1` per
numeric literal, in the order the trace prints them (the `stale` column of a run that matches),
and each literal's node gets that byte whatever the pragmas and the heap say.

Measured on .hack//Infection: with `MWRECOMP_HASCALL=off`, 177 of 4,904 functions (in 77 of 216
units) compile differently from the original's output; with `on`, 208 (in 83 units). So the
leftover matters for a few percent of the functions, and those can carry it in their source.
The literals are nearly the whole story: recording each unit's literals and replaying them with
every other node's byte forced clear leaves 4 functions different (in boss04, boss05, enemyG and
kyviaGomora). Of 274,761 literals, 58,000 find the byte set: 33,736 in a unit's own source and
24,264 in included files.

## `stale_helper_args`: `#pragma stale_helper_args`

The back end calls the runtime's helpers (float to integer, double arithmetic and the like)
through `0x4a5390`, and emits each call with two masks of the argument registers it reads
(general at `0x53ca50`, floating at `0x53ca54`). Placing an argument adds its register to a mask;
the loop that should clear the masks for the next call has no body. So they only grow: over the
calls of a function, the functions of a file, and the files of one compiler run. A helper call
counts as reading every argument register that any helper call before it used, and values computed
ahead of it are kept out of those registers. A function that converts a float (one argument)
compiles differently once a double addition (two) has been compiled, even in an earlier file of
the run, which is how a project built file after file in one process gets code that no run on a
single file gives. The pragma says what the masks hold from before:

```c
#pragma stale_helper_args(a0, a1)   // these registers count as used from here on
```

Registers are `a0` to `a7` and `f12` to `f19`. The pragma only adds: the compiler's own
bookkeeping goes on, and with the extension off the original ignores the line.

```c
t->u = a + b;       // a, b, c in v1, a1, a2 ...
t->h = (int)sy;     // ... ahead of the helper call

#pragma stale_helper_args(a0, a1)
t->u = a + b;       // a, b, c in v1, a2, a3
t->h = (int)sy;
```

`MWRECOMP_HELPER_ARGS_TRACE=1` prints both masks when a file's compile starts and after each
pragma; run it on the file that follows another in one run to see what the first one left.

Measured on .hack//Infection: a file that adds two doubles leaves `a0` and `a1` (general
`00000030`). 58 functions in 38 of 219 units depend on whether those are set when the unit starts;
32 units match retail only with them and 6 only without, in no order a single run could give.

## `name_counter`: `#pragma name_counter`

The front end names string literals, aggregate images, labels and temporaries `@N`, and
function-local statics `name$N`, from one counter per source file. Everything the file and its
headers declare and compile moves it (unnamed types, destructors defined in a class, inline
bodies, each function compiled), and the names reach the object's symbol table: its order, and
every relocation that names a symbol. When a project's headers are not the original ones, its
numbers differ although its code is the same. The pragma states the original's numbers:

```c
#pragma name_counter 663             // the next name taken is @663
#pragma name_counter 812 after 149   // the name taken 149 names after this pragma is @812
```

Names are taken in the order the compiler needs them, not in source order: a function's string
literals are named when the back end compiles it, after its body has been read. The deferred form
therefore counts names from the pragma, and a list of them at the top of a file can state every
number the file needs. Each pending number starts a run: the names after it count up from it. The
code does not change; the list starts again with each source file. With the extension off, the
original ignores the pragma.

`MWRECOMP_NAMES_TRACE=1` prints each name taken: its place in the file's sequence, its number,
and the file and line the preprocessor had reached. Comparing a trace with the original's
numbers gives the list.

## Tracing allocations

`MWRECOMP_ALLOC_TRACE=1` (or `2`, with every `lalloc` call) prints what the compiler's heaps hand
out, with the caller's address. It is not an extension and needs no `MWRECOMP_EXT`. When two runs
that should agree do not, the first line that differs between their traces is where they parted.

## Writing one

- Wrap a function, do not replace it: `HOOK_WRAP(Name, addr, moved, rel_at, before, after)` at file
  scope, with `before` returning 0 to let the function run. Handlers run on their own stack and may
  call the original's functions.
- Check `Ext_On("name")` first and change nothing when it is false.
- A place inside an original function that the extension relies on (a return address, say) is
  written `ORIG_SITE(0x...)`: the build then fails if a port replaces that function.
- Per-file state is reset from `CompileStart_Before` (`ext/ext.c`).
- A pragma is handed over by `Pragma_Before` (`ext/ext.c`), the one wrap on the `#pragma` handler:
  add the name there and return where the pragma's text ends, or 0 if it is malformed.
- Add a check to `tools/exttest.py`: the extension works, and the output is the original's when it
  is off or unused.
