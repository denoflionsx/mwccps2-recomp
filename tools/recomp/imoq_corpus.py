"""Snapshot the .hack//Infection sources as a difftest corpus.

Copies src/, include/ and tools/mwcc_warmup.cpp from the game repo (read-only; path from
tools/imoq/gamepath.py) to build/recomp/corpus/imoq/, and writes manifest.json there with one
case per compile unit: the exact mwccps2 command the game's build.ninja runs (flags, per-unit opt,
warmup file first, MWCIncludes=include). Units built with another compiler (cc303) are skipped.

usage: python tools/recomp/imoq_corpus.py
then:  python tools/recomp/difftest.py --cand ... --manifest build/recomp/corpus/imoq/manifest.json
"""

import json
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/imoq"))
import gamepath  # noqa: E402

OUT = ROOT / "build/recomp/corpus/imoq"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # copy inside WSL with tar (reading many files through \\wsl.localhost is slow)
    dest = gamepath.wsl(str(OUT))
    cmd = f"cd {gamepath.G_WSL} && tar cf - src include tools/mwcc_warmup.cpp | (cd '{dest}' && tar xf -)"
    p = subprocess.run(["wsl", "bash", "-lc", cmd], capture_output=True, text=True)
    if p.returncode:
        sys.exit(p.stderr)
    ninja = subprocess.run(["wsl", "cat", f"{gamepath.G_WSL}/build.ninja"], capture_output=True, text=True).stdout
    cfg = subprocess.run(["wsl", "cat", f"{gamepath.G_WSL}/configure.py"], capture_output=True, text=True).stdout
    flags = shlex.split(re.search(r"^MWCC_FLAGS = '(.*)'", cfg, re.M)[1])

    cases, skipped = [], []
    blocks = re.split(r"\n(?=build )", ninja)
    for b in blocks:
        m = re.match(r"build (\S+) \| \S+: mwcc (\S+)", b)
        if not m:
            continue
        src = m[2]
        var = dict(re.findall(r"^\s+(\w+) = ?(.*)$", b, re.M))
        if "mwcc" in var:  # another compiler
            skipped.append(src)
            continue
        warmup = var.get("warmup", "tools/mwcc_warmup.cpp").strip()
        args = [*flags, *shlex.split(var.get("opt", "")), "-o", "{out}", *([warmup] if warmup else []), src]
        # the same source can be built for several targets (main, overlays) with different flags
        target = m[1].split("/")[1]
        name = f"imoq/{target}/" + src.replace("/", ".")
        cases.append({"name": name, "args": args, "env": {"MWCIncludes": "include"}})
    names = [c["name"] for c in cases]
    assert len(set(n.lower() for n in names)) == len(names), "duplicate case names"
    (OUT / "manifest.json").write_text(json.dumps({"cwd": ".", "cases": cases}, indent=1), encoding="utf-8")
    print(f"{len(cases)} cases -> {OUT / 'manifest.json'}; skipped {len(skipped)} built with another compiler")


if __name__ == "__main__":
    main()
