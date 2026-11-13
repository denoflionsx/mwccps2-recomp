"""Build the hybrid compiler: the original mwccps2.exe plus a stand-in LMGR326B.dll that
redirects every function in recomp/src to its recompiled version.

Output: build/recomp/hybrid/{mwccps2.exe, LMGR326B.dll}. Test it with
  python tools/recomp/difftest.py --cand build/recomp/hybrid/mwccps2.exe

Needs clang, lld-link and llvm-dlltool (LLVM) on PATH. The import library for kernel32 is
generated, so no Windows SDK is needed.
"""

import argparse
import filecmp
import json
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ORIG_DIR = ROOT / "recomp_goal/mwcps2-3.0.1-020123"
OUT = ROOT / "build/recomp/hybrid"
OBJ = OUT / "obj"

KERNEL32 = ["VirtualProtect@16", "FlushInstructionCache@12", "GetCurrentProcess@0",
            "GetEnvironmentVariableA@12", "ExitProcess@4", "GetStdHandle@4", "WriteFile@20",
            "AddVectoredExceptionHandler@8", "CreateFileA@28", "CreateFileMappingA@24", "MapViewOfFile@20",
            "ReadFile@20", "CloseHandle@4", "GetFileSize@8", "VirtualAlloc@16", "GlobalAlloc@8",
            "GetModuleFileNameA@12", "SetEnvironmentVariableA@8"]
CFLAGS = ["--target=i686-pc-windows-msvc", "-O2", "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
          "-fwrapv", "-fno-strict-aliasing", "-fno-delete-null-pointer-checks", "-mno-sse",
          "-Wall", "-Wno-unused-function", "-Wno-unused-variable", "-DRECOMP_HYBRID=1",
          f"-I{ROOT / 'recomp/include'}"]


def run(cmd):
    p = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if p.returncode:
        sys.exit(f"failed: {' '.join(map(str, cmd))}\n{p.stdout}{p.stderr}")
    if p.stderr.strip():
        print(p.stderr.strip())


def check_hooks(sources):
    """Every HOOK address must be a function start known to x86map."""
    known = {f["va"]: f for f in json.load(open(ROOT / "docs/x86map.json", encoding="utf-8"))}
    hooks = []
    for src in sources:
        for m in re.finditer(r"^HOOK\((0x[0-9a-fA-F]+),\s*(\w+)\)", src.read_text(encoding="utf-8"), re.M):
            addr = int(m[1], 16)
            if addr not in known:
                sys.exit(f"{src.name}: HOOK({m[1]}, {m[2]}) is not a known function start")
            hooks.append((addr, m[2], src.name))
    dup = {a for a, *_ in hooks if sum(1 for b, *_ in hooks if b == a) > 1}
    if dup:
        sys.exit(f"hooked twice: {sorted(hex(a) for a in dup)}")
    return hooks


def write_cov_blocks(path):
    """Block starts for coverage (MWRECOMP_COV_BLOCKS): u32 count, then sorted u32 addresses."""
    import struct

    starts = sorted({int(line.split("\t")[0], 16) for line in open(ROOT / "docs/recomp/blocks.tsv")})
    path.write_bytes(struct.pack(f"<I{len(starts)}I", len(starts), *starts))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=str(OUT), help="output directory")
    ap.add_argument("--cflags", default="", help="extra clang flags, e.g. for a negative-control build")
    a = ap.parse_args()
    out = Path(a.out)
    obj = out / "obj"
    build(out, obj, a.cflags.split())


def build(OUT, OBJ, extra):
    OBJ.mkdir(parents=True, exist_ok=True)
    sources = sorted((ROOT / "recomp/src").glob("*.c"))
    hooks = check_hooks(sources)

    # kernel32 import library from a generated .def
    deffile = OBJ / "kernel32.def"
    deffile.write_text("LIBRARY KERNEL32.dll\nEXPORTS\n" + "".join(f"    {n}\n" for n in KERNEL32))
    run(["llvm-dlltool", "-m", "i386", "-k", "-d", deffile, "-l", OBJ / "kernel32.lib"])

    objs = []
    for src in [ROOT / "recomp/hybrid/hook.c", ROOT / "recomp/hybrid/poke.c", *sources]:
        o = OBJ / (src.stem + ".obj")
        run(["clang", *CFLAGS, *extra, "-c", src, "-o", o])
        objs.append(o)

    lmgr_def = OBJ / "lmgr.def"
    # The license calls go on to the real FlexLM DLL (copied as LMGR326B_real.dll). Stubbing them
    # (like mweppc's lmgr_stub) makes some -O4,p schedules vary from run to run: the original's
    # output depends on the process state the real DLL leaves behind (see docs/RECOMP.md).
    lmgr_def.write_text("LIBRARY LMGR326B\nEXPORTS\n"
                        "    lp_checkin = LMGR326B_real.lp_checkin @189\n"
                        "    lp_checkout = LMGR326B_real.lp_checkout @190\n"
                        "    lp_errstring = LMGR326B_real.lp_errstring @191\n")
    run(["lld-link", "/dll", "/nodefaultlib", "/entry:DllMain", "/machine:x86", "/safeseh:no",
         "/merge:.hooks=.rdata", "/base:0x60000000", "/dynamicbase:no", f"/def:{lmgr_def}", f"/out:{OUT / 'LMGR326B.dll'}", *objs, OBJ / "kernel32.lib"])
    copy_same(ORIG_DIR / "mwccps2.exe", OUT / "mwccps2.exe")
    copy_same(ORIG_DIR / "LMGR326B.dll", OUT / "LMGR326B_real.dll")
    write_cov_blocks(OUT / "cov_blocks.bin")
    print(f"built {OUT / 'LMGR326B.dll'} with {len(hooks)} hooks from {len(sources)} sources")
    for addr, name, f in hooks:
        print(f"  {addr:08x} {name:30} {f}")


def copy_same(src, dst):
    """Copy src to dst unless dst is already identical (it can be held open for a moment by a
    compiler process that is still exiting, or a virus scanner); retry briefly when locked."""
    if dst.exists() and filecmp.cmp(src, dst, shallow=False):
        return
    for i in range(20):
        try:
            shutil.copy(src, dst)
            return
        except PermissionError:
            if i == 19:
                raise
            time.sleep(0.5)


if __name__ == "__main__":
    main()
