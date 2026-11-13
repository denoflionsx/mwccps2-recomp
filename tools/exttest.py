"""Test the extensions (docs/EXTENSIONS.md) of a hybrid build against the original compiler.

  python tools/exttest.py [--cand build/mwccps2.exe]

  idle        with no extension asked for, and with all of them on but unused, every source in tests/
              compiles to the same bytes as with the original
  line_path   tests/ext/line_main.cpp: the paths of its #line directives are the file names of the
              debug records, the code is the same as without the extension, and the whole object is
              the same from two folders whose paths differ in length
  stale_hascall  tests/ext/hascall.cpp: the pragma moves constant arguments ahead, per literal and
              per region, and is left to the original when the extension is off
  stale_helper_args  tests/ext/helperargs.cpp: the pragma keeps values out of the registers it
              names ahead of a helper call, exactly as an earlier file of the run that adds
              doubles does

Exit code 1 when a check fails.
"""
import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
from common import HYBRID_EXE, ROOT  # noqa: E402

FLAGS = ["-c", "-g", "-O2,p", "-nostdinc", "-nosyspath", "-I."]
EXT = ROOT / "tests" / "ext"
failed = []


def check(name, ok, detail=""):
    print(("ok    " if ok else "FAIL  ") + name + (f": {detail}" if detail and not ok else ""))
    if not ok:
        failed.append(name)


def compile_in(folder, exe, source, ext=None, env=None):
    """Compile folder/source there; returns (object bytes or None, what the compiler printed)."""
    full = dict(os.environ, **(env or {}))
    full.pop("MWRECOMP_EXT", None)
    if ext:
        full["MWRECOMP_EXT"] = ext
    out = Path(folder) / "out.o"
    out.unlink(missing_ok=True)
    lang = ["-lang", "c++"] if source.endswith(".cpp") else []
    p = subprocess.run([*common.launch(), str(exe), *FLAGS, *lang, source, "-o", "out.o"], cwd=folder, capture_output=True, env=full)
    return (out.read_bytes() if out.exists() else None), (p.stdout + p.stderr).decode("latin-1")


def compile_after(folder, exe, first, source):
    """Compile folder/first and then folder/source in one run, no extension on; returns source's object bytes or None."""
    full = dict(os.environ)
    full.pop("MWRECOMP_EXT", None)
    out = Path(folder) / "two"
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir()
    subprocess.run([*common.launch(), str(exe), *FLAGS, "-lang", "c++", first, source, "-o", "two"], cwd=folder, capture_output=True, env=full)
    obj = out / (Path(source).stem + ".o")
    return obj.read_bytes() if obj.exists() else None


def sections(obj):
    """[(name, contents)] of an ELF object, in file order."""
    shoff, = struct.unpack_from("<I", obj, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", obj, 0x2E)
    heads = [struct.unpack_from("<IIIIIIIIII", obj, shoff + i * shentsize) for i in range(shnum)]
    strings = heads[shstrndx][4]
    return [(obj[strings + h[0]:obj.index(b"\0", strings + h[0])].decode(), obj[h[4]:h[4] + h[5]] if h[1] != 8 else b"")
            for h in heads]


def code(obj):
    return [(n, d) for n, d in sections(obj) if n in (".text", ".data", ".rodata", ".sdata")]


def debug_files(obj):
    """The file names in the debug records, each once, sorted."""
    data = b"".join(d for n, d in sections(obj) if n == ".debug")
    names = set()
    for piece in data.split(b"\0"):  # a name follows its attribute code (38 00) and ends in a null
        if piece.endswith((b".cpp", b".h")):
            start = len(piece)
            while start and 32 <= piece[start - 1] < 127:
                start -= 1
            names.add(piece[start:].decode("ascii"))
    return sorted(names)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cand", default=str(HYBRID_EXE))
    a = ap.parse_args()
    cand, orig = Path(a.cand).resolve(), common.need_orig()

    with tempfile.TemporaryDirectory(prefix="mwext_") as tmp:
        tmp = Path(tmp)
        # idle: the same bytes as the original
        work = tmp / "idle"
        shutil.copytree(ROOT / "tests", work, ignore=shutil.ignore_patterns("cli", "ext"))
        for src in sorted(p.name for p in work.iterdir() if p.suffix in (".c", ".cpp")):
            ref, _ = compile_in(work, orig, src)
            off, _ = compile_in(work, cand, src)
            on, _ = compile_in(work, cand, src, ext="all")
            check(f"idle {src}", ref == off == on, "objects differ from the original's")

        # line_path
        work = tmp / "line"
        shutil.copytree(EXT, work)
        ref, _ = compile_in(work, orig, "line_main.cpp")
        off, _ = compile_in(work, cand, "line_main.cpp")
        on, _ = compile_in(work, cand, "line_main.cpp", ext="line_path")
        check("line_path off: the original's object", ref is not None and ref == off)
        check("line_path on: compiles", on is not None)
        if on and off:
            want = ["D:\\work\\game\\source\\head.h", "D:\\work\\game\\source\\main.cpp", "D:\\work\\game\\source\\other.cpp"]
            check("line_path on: debug records name the directives' paths", debug_files(on) == want, str(debug_files(on)))
            check("line_path off: debug records name the real files", all("line_" in n for n in debug_files(off)), str(debug_files(off)))
            check("line_path on: code unchanged", code(on) == code(off))
        longer = tmp / "a_folder_with_a_much_longer_name_than_the_other_one"
        shutil.copytree(EXT, longer)
        far_on, _ = compile_in(longer, cand, "line_main.cpp", ext="line_path")
        far_off, _ = compile_in(longer, cand, "line_main.cpp")
        check("line_path on: the same object from another folder", on is not None and on == far_on)
        check("line_path off: the folder shows in the object", off != far_off)

        # stale_hascall
        work = tmp / "hascall"
        shutil.copytree(EXT, work)
        off, _ = compile_in(work, cand, "hascall.cpp")
        ref, _ = compile_in(work, orig, "hascall.cpp")
        on, log = compile_in(work, cand, "hascall.cpp", ext="stale_hascall", env={"MWRECOMP_HASCALL_TRACE": "1"})
        check("stale_hascall off: the original's object", ref is not None and ref == off)
        check("stale_hascall on: compiles", on is not None)
        if on and off:
            # plain's constants find whatever the memory held, so it is not compared
            plain, marked, all_on, all_off = [d for n, d in sections(on) if n == ".text"]
            check("stale_hascall: the list moves the marked constants ahead", marked != all_off and len(marked) == len(all_off))
            check("stale_hascall: with every constant marked their order stays", all_on == all_off)
            traced = [line.split()[-1] for line in log.splitlines() if line.startswith("mwrecomp: literal")]
            check("stale_hascall: the trace shows what each literal was given",
                  traced[3:] == ["1", "0", "1", "1", "1", "1", "0", "0", "0"], " ".join(traced))
        (work / "bad.cpp").write_text("#pragma stale_hascall maybe\nint x;\n")
        bad, log = compile_in(work, cand, "bad.cpp", ext="stale_hascall")
        check("stale_hascall: a malformed pragma is an error", bad is None and "bad.cpp" in log)

        # stale_helper_args
        work = tmp / "helperargs"
        shutil.copytree(EXT, work)
        ref, _ = compile_in(work, orig, "helperargs.cpp")
        off, _ = compile_in(work, cand, "helperargs.cpp")
        on, log = compile_in(work, cand, "helperargs.cpp", ext="stale_helper_args", env={"MWRECOMP_HELPER_ARGS_TRACE": "1"})
        check("stale_helper_args off: the original's object", ref is not None and ref == off)
        check("stale_helper_args on: compiles", on is not None)
        if on and off:
            plain_before, plain_after = [d for n, d in sections(off) if n == ".text"]
            before, after = [d for n, d in sections(on) if n == ".text"]
            check("stale_helper_args off: the two functions compile alike", plain_before == plain_after)
            check("stale_helper_args on: the function after the pragma keeps out of a1",
                  before == plain_before and after != before and len(after) == len(before))
            # f12 is the conversion in Before(), compiled by then: the masks keep what every call adds
            check("stale_helper_args: the trace shows the masks", "after the pragma: general 00000030 floating 00001000" in log, log)
            # what the pragma stands for: an earlier file of the same run that passed two words to a helper
            two = compile_after(work, cand, "helperargs_warm.cpp", "helperargs.cpp")
            check("stale_helper_args: the same code as after a file that adds doubles",
                  two is not None and [d for n, d in sections(two) if n == ".text"] == [after, after])
        (work / "bad.cpp").write_text("#pragma stale_helper_args(a9)\nint x;\n")
        bad, log = compile_in(work, cand, "bad.cpp", ext="stale_helper_args")
        check("stale_helper_args: a malformed pragma is an error", bad is None and "bad.cpp" in log)

    print(f"{len(failed)} failed" if failed else "all passed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
