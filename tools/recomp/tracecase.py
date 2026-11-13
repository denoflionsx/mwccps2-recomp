"""Trace one leaked operand byte to the instruction that reads it.

Given a game case, a builder id and a byte (from docs/recomp/leaks.json), bisects over that
builder's calls to find the one whose byte changes the output (base 00 vs ff), then reruns that
call with the watchpoint following every copy and prints each access with its instruction.

usage: python tools/recomp/tracecase.py imoq/gcmn/src.source.enemyU.cpp 2 13
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HYB = ROOT / "build/recomp/hybrid_opfill/mwccps2.exe"
MANIFEST = ROOT / "build/recomp/corpus/imoq/manifest.json"
sys.path.insert(0, str(Path(__file__).parent))


def compile_case(case, env_extra, out):
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    env = dict(os.environ, **case.get("env", {}), **env_extra)
    args = [a.replace("{out}", str(out)) for a in case["args"]]
    p = subprocess.run([str(HYB), *args], cwd=MANIFEST.parent, capture_output=True, text=True, env=env)
    objs = {f.name: f.read_bytes() for f in sorted(out.glob("*.o"))}
    return objs, p.stderr


def same(x, y):
    """Equal up to what the original varies from run to run (difftest's comparison)."""
    from difftest import same_modulo_debug_order

    return x.keys() == y.keys() and all(x[k] == y[k] or same_modulo_debug_order(x[k], y[k]) for k in x)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("case")
    ap.add_argument("id", type=int)
    ap.add_argument("byte", type=int)
    a = ap.parse_args()
    m = json.load(open(MANIFEST, encoding="utf-8"))
    case = next(c for c in m["cases"] if c["name"] == a.case)
    tmp = Path(tempfile.mkdtemp(prefix="mwtc_"))
    base = {"MWRECOMP_OPFILL": "00"}
    alt = dict(base, MWRECOMP_OPFILL_IDS=f"{1 << a.id:x}", MWRECOMP_OPFILL_BYTES=f"{1 << a.byte:x}",
               MWRECOMP_OPFILL_ALT="ff")
    h0, _ = compile_case(case, base, tmp / "o")
    _, err = compile_case(case, dict(base, MWRECOMP_OPFILL_COUNT="1"), tmp / "o")
    ncalls = next(int(l.split()[-1], 16) for l in err.splitlines()
                  if "builder calls" in l and int(l.split()[-2], 16) == a.id)
    def flips(n):
        env = dict(alt, MWRECOMP_OPFILL_NTH=f"{n:x}")
        # must differ twice: a single difference can be run-to-run noise
        return all(not same(compile_case(case, env, tmp / "o")[0], h0) for _ in range(2))

    flipping = [n for n in range(ncalls) if flips(n)]
    print(f"{a.case} id {a.id} byte {a.byte}: {ncalls} calls, flipping: {flipping}")
    from_where = __import__("where_lib")
    for n in flipping[:2]:
        _, log = compile_case(case, dict(base, MWRECOMP_OPFILL_TRACE=f"{a.id:x},{n:x},{a.byte:x}",
                                         MWRECOMP_OPFILL_TRACE_MAX="30", MWRECOMP_OPFILL_TRACE_DEPTH=os.environ.get("TRACE_DEPTH", "63")), tmp / "o")
        print(f"-- call {n}")
        for line in log.splitlines():
            if "outside" in line:
                continue
            extra = ""
            if "next eip" in line:
                extra = "   " + from_where.describe(int(line.split()[3], 16))
            elif "caller, out" in line:
                extra = "   builder called from " + from_where.describe(int(line.split()[-2], 16), call=True)
            print("  " + line + extra)


if __name__ == "__main__":
    main()
