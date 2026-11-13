"""Find the coverage trap whose firing changes a compile's output (stack-sensitivity hunting).

With every block armed, `difftest.py --cov` changes the code of some units: Windows' exception
handling for an int3 writes below the stack pointer, and some later code reads that memory without
having written it. This bisects MWRECOMP_COV_ARM (the range of armed blocks) down to single blocks
whose trap alone flips the output, then prints them with their functions.

usage: python tools/recomp/trapbisect.py <manifest case name> [--manifest build/recomp/corpus/imoq/manifest.json]
"""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HYB = Path(os.environ.get("MWRECOMP_HYBRID", ROOT / "build/recomp/hybrid")).resolve()


def text_hash(outdir):
    from elftools.elf.elffile import ELFFile

    h = hashlib.md5()
    for f in sorted(Path(outdir).glob("*.o")):
        with open(f, "rb") as fp:
            for s in ELFFile(fp).iter_sections():
                if s.name in (".text", ".rel.text", ".data", ".rodata", ".symtab"):
                    h.update(s.name.encode() + s.data())
    return h.hexdigest()[:12]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("case")
    ap.add_argument("--manifest", default=str(ROOT / "build/recomp/corpus/imoq/manifest.json"))
    a = ap.parse_args()
    mf = Path(a.manifest)
    m = json.loads(mf.read_text(encoding="utf-8"))
    case = next(c for c in m["cases"] if c["name"] == a.case)
    base = mf.parent / m.get("cwd", ".")
    blocks = [l.split("\t") for l in open(ROOT / "docs/recomp/blocks.tsv", encoding="utf-8")]
    starts = sorted((int(b[0], 16), int(b[2], 16)) for b in blocks)
    n = len({s for s, _ in starts})

    tmp = Path(tempfile.mkdtemp(prefix="mwtb_"))
    runs = 0

    def run(lo, hi):
        nonlocal runs
        runs += 1
        out = tmp / "out"
        subprocess.run(["cmd", "/c", "rmdir", "/s", "/q", str(out)], capture_output=True)
        out.mkdir()
        env = dict(os.environ, **case.get("env", {}), MWRECOMP_HOOKS="off")
        if hi > lo:
            env.update(MWRECOMP_COV=str(tmp / "cov.bin"), MWRECOMP_COV_BLOCKS=str(HYB / "cov_blocks.bin"),
                       MWRECOMP_COV_ARM=f"{lo},{hi}")
        args = [a_.replace("{out}", str(out)) for a_ in case["args"]]
        subprocess.run([str(HYB / "mwccps2.exe"), *args], cwd=base, capture_output=True, env=env)
        return text_hash(out)

    plain = run(0, 0)
    full = run(0, n)
    print(f"no traps {plain}, all traps {full}", flush=True)
    if plain == full:
        sys.exit("arming every block doesn't change this case")

    found = []

    def search(lo, hi):
        if run(lo, hi) == plain:
            return
        if hi - lo == 1:
            found.append(lo)
            print(f"  block #{lo} flips it", flush=True)
            return
        mid = (lo + hi) // 2
        before = len(found)
        search(lo, mid)
        search(mid, hi)
        if len(found) == before:
            print(f"  range {lo}..{hi} flips it only as a whole (several traps together)")
            found.append((lo, hi))

    search(0, n)
    uniq = sorted({s for s, _ in starts})
    func_of = dict(starts)
    names = json.load(open(ROOT / "docs/names.json", encoding="utf-8"))
    mod = json.load(open(ROOT / "docs/modmap.json", encoding="utf-8"))
    print(f"{runs} compiles")
    for f in found:
        if isinstance(f, tuple):
            print(f"range {f}")
            continue
        addr = uniq[f]
        fn = func_of[addr]
        print(f"block {addr:08x} in {fn:08x} {names.get(f'{fn:x}') or mod.get(f'{fn:x}', {}).get('name')} "
              f"({mod.get(f'{fn:x}', {}).get('file')})")


if __name__ == "__main__":
    main()
