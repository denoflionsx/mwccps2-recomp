"""Snapshot the .hack//Infection decompilation's sources as a difftest corpus.

Copies src/ and include/ (and the context unit the game's build compiles first) from a checkout of
the game repository to build/corpus/imoq/, and writes manifest.json there with one case per compile
unit: the mwccps2 command the game's build.ninja runs for it (the rule's flags, the unit's flags,
the context unit first, MWCIncludes=include). Units built with another compiler version are
skipped. The checkout is only read; run its configure.py first so that build.ninja exists.

usage: python tools/imoq_corpus.py path/to/IMOQ_Decomp
then:  python tools/difftest.py --cand build/mwccps2.exe --manifest build/corpus/imoq/manifest.json
"""
import json
import re
import shlex
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import BUILD  # noqa: E402

OUT = BUILD / "corpus" / "imoq"
OURS = "mwcps2-3.0.1-020123"


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    game = Path(sys.argv[1])
    if not (game / "build.ninja").exists():
        sys.exit(f"{game / 'build.ninja'} is missing: run the game's configure.py")
    ninja = (game / "build.ninja").read_text(encoding="utf-8").replace("$\n", "")
    rule = re.search(r"^rule mwcc\n\s+command = (.*)$", ninja, re.M)[1]
    base = shlex.split(re.search(r" -- (.*?) \$flags", rule)[1])

    cases, skipped, contexts = [], [], set()
    for block in re.split(r"\n(?=build )", ninja):
        m = re.match(r"build (.+?): +mwcc (\S+)", block)
        if not m:
            continue
        src = m[2]
        var = dict(re.findall(r"^\s+(\w+) = ?(.*)$", block, re.M))
        if OURS not in var.get("compiler", ""):
            skipped.append(src)
            continue
        context = var.get("context", "").replace("--context", "").split()
        contexts.update(context)
        args = [*base, *shlex.split(var.get("flags", "")), *(["-lang", "c++"] if src.lower().endswith(".cpp") else []),
                "-o", "{out}", *context, src]
        # the same source can be built for several targets (main, overlays) with different flags
        target = next((p.split("/")[1] for p in m[1].split() if p.startswith("build/") and p.count("/") > 2), "main")
        cases.append({"name": f"imoq/{target}/" + src.replace("/", "."), "args": args, "env": {"MWCIncludes": "include"}})
    names = [c["name"] for c in cases]
    assert len({n.lower() for n in names}) == len(names), "duplicate case names"

    OUT.mkdir(parents=True, exist_ok=True)
    for folder in ("src", "include"):
        shutil.rmtree(OUT / folder, ignore_errors=True)
        shutil.copytree(game / folder, OUT / folder)
    for context in contexts:
        (OUT / context).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(game / context, OUT / context)
    (OUT / "manifest.json").write_text(json.dumps({"cwd": ".", "cases": cases}, indent=1), encoding="utf-8")
    print(f"{len(cases)} cases -> {OUT / 'manifest.json'}; skipped {len(skipped)} built with another compiler")


if __name__ == "__main__":
    main()
