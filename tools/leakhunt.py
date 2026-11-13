"""Find which leaked operand bytes change the compiler's output (docs/ENTROPY.md, "Address-dependent output").

Uses the experiment build (build.py --out build_opfill
--cflags=-DRECOMP_OPFILL_EXPERIMENT), where every operand-record builder fills its unset bytes from
settings (src/VReg.c). Two copies of that build run side by side with different
mwrecomp.env files:
  base     every unset byte 00
  variant  the same, but builder `id`'s unset bytes (or one byte) = ALT
A case that differs means some code reads that builder's leftover byte and acts on it.

  phase 0  base vs base (noise)
  phase 1  per builder id, all its unset bytes, ALT = ff and 01, whole corpus
  phase 2  per builder with differences, each byte alone, on the cases phase 1 found

usage: python tools/leakhunt.py [--ids 0-7] [--alts ff,01] [--out docs/data/leaks.json]
"""
import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
from common import BUILD, DATA, ORIG_EXE, ROOT  # noqa: E402
SRC = BUILD / "hybrid_opfill"
WORK = BUILD / "leak"
MANIFEST = BUILD / "corpus/imoq/manifest.json"
NAMES = {0: "fn_497a30 GPR", 1: "fn_497a70 FPR", 2: "fn_497fb0 reg", 3: "fn_497ff0 new pair",
         4: "fn_498070 kind3", 5: "fn_4980b0 kind3", 6: "fn_4980e0 reg", 7: "fn_4980e0 mem"}
# bytes each builder sets (the others leak)
SET = {0: {0, 1, 4, 5}, 1: {0, 1, 4, 5}, 2: set(range(6)), 3: set(range(6)), 4: {0},
       5: {0} | set(range(2, 18)), 6: set(range(6)), 7: {0} | set(range(2, 10))}


def make(name, env):
    d = WORK / name
    d.mkdir(parents=True, exist_ok=True)
    for f in ("mwccps2.exe", "LMGR326B.dll", "cov_blocks.bin"):
        if (SRC / f).exists():
            shutil.copy(SRC / f, d / f)
    (d / "mwrecomp.env").write_text("".join(f"{k}={v}\n" for k, v in env.items()), encoding="ascii")
    return d / "mwccps2.exe"


def run(tag, ref, cand, only=None):
    cmd = [sys.executable, str(ROOT / "tools/difftest.py"), "--ref", str(ref), "--cand", str(cand),
           "--retries", "1", "--keep", "none", "--run", f"leak_{tag}", "--manifest", str(MANIFEST)]
    if only is not None:
        f = WORK / f"only_{tag}.txt"
        f.write_text("".join(f"{s}\t{fl}\n" for s, fl in only), encoding="utf-8")
        cmd += ["--only", str(f)]
    subprocess.run(cmd, capture_output=True, text=True)
    rep = json.load(open(BUILD / f"difftest/leak_{tag}/report.json", encoding="utf-8"))
    bad = [(r["src"], r["flags"]) for r in rep["results"] if not r["ok"]]
    inter = [(r["src"], r["flags"]) for r in rep["results"] if str(r.get("note")).startswith("intermittent")]
    return bad, inter


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ids", default="0-7")
    ap.add_argument("--alts", default="ff,01")
    ap.add_argument("--out", default=str(DATA / "leaks.json"))
    a = ap.parse_args()
    lo, _, hi = a.ids.partition("-")
    ids = list(range(int(lo), int(hi or lo) + 1))
    base_env = {"MWRECOMP_OPFILL": "00"}
    base = make("base", base_env)
    base2 = make("base2", base_env)
    result = {"noise": None, "builders": {}}

    bad, inter = run("noise", base, base2)
    result["noise"] = {"fail": bad, "intermittent": inter}
    print(f"phase 0: base vs base: {len(bad)} differ, {len(inter)} intermittent", flush=True)

    for i in ids:
        for alt in a.alts.split(","):
            cand = make(f"id{i}_{alt}", dict(base_env, MWRECOMP_OPFILL_IDS=f"{1 << i:x}", MWRECOMP_OPFILL_ALT=alt))
            bad, inter = run(f"id{i}_{alt}", base, cand)
            entry = result["builders"].setdefault(str(i), {"name": NAMES[i], "alts": {}})
            entry["alts"][alt] = {"fail": bad, "intermittent": inter, "bytes": {}}
            print(f"phase 1: id {i} ({NAMES[i]}) alt {alt}: {len(bad)} cases differ", flush=True)
            if not bad:
                continue
            for b in range(22):
                if b in SET[i]:
                    continue
                cand = make(f"id{i}_{alt}_b{b}", dict(base_env, MWRECOMP_OPFILL_IDS=f"{1 << i:x}",
                                                       MWRECOMP_OPFILL_ALT=alt, MWRECOMP_OPFILL_BYTES=f"{1 << b:x}"))
                bbad, _ = run(f"id{i}_{alt}_b{b}", base, cand, only=bad)
                if bbad:
                    entry["alts"][alt]["bytes"][str(b)] = bbad
                    print(f"  byte {b}: {len(bbad)} cases, e.g. {bbad[:3]}", flush=True)
            Path(a.out).write_text(json.dumps(result, indent=1), encoding="utf-8")
    Path(a.out).write_text(json.dumps(result, indent=1), encoding="utf-8")
    print("wrote", a.out)


if __name__ == "__main__":
    main()
