"""Summarize block coverage recorded by `difftest.py --cov`.

usage: python tools/covreport.py build/difftest/<run> [--md docs/data/coverage.md]

Reads <run>/cov/*.cov (one byte per block of docs/data/blocks.tsv) and writes:
  <run>/coverage.json   per function: blocks, blocks run, which cases enter it
  --md file             per-module table, the corpus cases that add the most, never-entered modules
"""

import sys
import argparse
import bisect
import json
import zlib
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
from common import BUILD, DATA, ORIG_EXE, ROOT  # noqa: E402


def load_blocks():
    starts, funcs = [], []
    for line in open(DATA / "blocks.tsv", encoding="utf-8"):
        a, size, f = (int(x, 16) for x in line.split("\t"))
        starts.append(a)
        funcs.append(f)
    order = sorted(range(len(starts)), key=lambda i: starts[i])
    return [starts[i] for i in order], [funcs[i] for i in order]


def module_lookup():
    mod = common.optional_json("modmap.json")
    keys = sorted(int(k, 16) for k in mod)
    files = [mod[f"{k:x}"].get("file") or "?" for k in keys]
    names = {int(k, 16): v.get("name") for k, v in mod.items()}
    try:
        user = common.optional_json("names.json")
        for k, v in (user.get("functions", user) if isinstance(user, dict) else {}).items():
            try:
                names[int(k, 16)] = v if isinstance(v, str) else v.get("name", names.get(int(k, 16)))
            except (ValueError, AttributeError):
                pass
    except FileNotFoundError:
        pass

    def lookup(va):
        i = bisect.bisect_right(keys, va) - 1
        return files[i] if i >= 0 else "?"

    return lookup, names


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("run")
    ap.add_argument("--md")
    a = ap.parse_args()
    run = Path(a.run).resolve()
    starts, bfunc = load_blocks()
    n = len(starts)
    module_of, names = module_lookup()

    union = bytearray(n)
    armed = bytearray(n)
    per_case = {}
    for f in sorted((run / "cov").glob("*.cov")):
        cov = zlib.decompress(f.read_bytes())
        assert len(cov) == n, f"{f.name}: {len(cov)} blocks, expected {n} (blocks.tsv changed?)"
        hit = {i for i, v in enumerate(cov) if v == 1}
        per_case[f.stem] = hit
        for i in hit:
            union[i] = 1
        for i, v in enumerate(cov):
            if v:
                armed[i] = 1

    # functions already replaced by recompiled code (their original blocks never run)
    replaced = set(common.hook_addresses())

    # per function
    fn = defaultdict(lambda: {"blocks": 0, "run": 0, "cases": 0})
    for i in range(n):
        if armed[i] and bfunc[i] not in replaced:
            d = fn[bfunc[i]]
            d["blocks"] += 1
            d["run"] += union[i]
    entry_index = {s: i for i, s in enumerate(starts)}
    for case, hit in per_case.items():
        for f in {bfunc[i] for i in hit}:
            if entry_index.get(f) in hit:
                fn[f]["cases"] += 1

    # per module
    mods = defaultdict(lambda: {"funcs": 0, "entered": 0, "blocks": 0, "run": 0, "replaced": 0})
    for f in replaced:
        mods[module_of(f)]["replaced"] += 1
    for f, d in fn.items():
        m = mods[module_of(f)]
        m["funcs"] += 1
        m["entered"] += d["run"] > 0
        m["blocks"] += d["blocks"]
        m["run"] += d["run"]

    # greedy: which cases add the most new blocks
    left = dict(per_case)
    seen, greedy = set(), []
    while left and len(greedy) < 25:
        best = max(left, key=lambda c: len(left[c] - seen))
        gain = len(left[best] - seen)
        if not gain:
            break
        greedy.append((best, gain))
        seen |= left.pop(best)
    unique = defaultdict(int)
    for i in range(n):
        if union[i]:
            owners = [c for c, h in per_case.items() if i in h]
            if len(owners) == 1:
                unique[owners[0]] += 1

    tot_b, tot_r = sum(armed), sum(union)
    tot_f, tot_e = len(fn), sum(d["run"] > 0 for d in fn.values())
    out = {
        "cases": len(per_case),
        "blocks": tot_b,
        "blocks_run": tot_r,
        "functions": tot_f,
        "functions_entered": tot_e,
        "modules": mods,
        "functions_detail": {
            f"{f:x}": dict(d, module=module_of(f), name=names.get(f)) for f, d in sorted(fn.items())
        },
        "greedy": greedy,
        "unique_blocks_by_case": dict(sorted(unique.items(), key=lambda kv: -kv[1])),
    }
    (run / "coverage.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
    print(f"{len(per_case)} cases: blocks {tot_r}/{tot_b} ({100 * tot_r / tot_b:.1f}%), "
          f"functions entered {tot_e}/{tot_f} ({100 * tot_e / tot_f:.1f}%)")

    if a.md:
        L = [
            "# Corpus coverage of mwccps2.exe",
            "",
            f"Generated by `tools/covreport.py` from `{run.relative_to(ROOT).as_posix()}` "
            f"({len(per_case)} cases: corpus × flag sets). A block counts as run if any case executes it.",
            "",
            f"- **Blocks run: {tot_r:,} / {tot_b:,} ({100 * tot_r / tot_b:.1f}%)**",
            f"- **Functions entered: {tot_e:,} / {tot_f:,} ({100 * tot_e / tot_f:.1f}%)**",
            f"- Replaced by recompiled code (not counted above): {len(replaced)}",
            "",
            "## By module",
            "",
            "Sorted by blocks not run. Functions already replaced by recompiled code are left out of the",
            "other columns (their original blocks no longer run).",
            "",
            "| module | functions | entered | blocks | run | % run | not run | replaced |",
            "|---|---:|---:|---:|---:|---:|---:|---:|",
        ]
        for name, m in sorted(mods.items(), key=lambda kv: -(kv[1]["blocks"] - kv[1]["run"])):
            pct = 100 * m["run"] / m["blocks"] if m["blocks"] else 0
            L.append(f"| {name} | {m['funcs']} | {m['entered']} | {m['blocks']} | {m['run']} | {pct:.0f}% | "
                     f"{m['blocks'] - m['run']} | {m['replaced'] or ''} |")
        L += ["", "## Cases that add the most", "",
              "Greedy order: each row is the case that adds the most blocks not yet run by the rows above.",
              "", "| case | new blocks |", "|---|---:|"]
        L += [f"| {c} | {g} |" for c, g in greedy]
        L += ["", "## Cases with blocks no other case runs", "", "| case | unique blocks |", "|---|---:|"]
        L += [f"| {c} | {u} |" for c, u in list(out["unique_blocks_by_case"].items())[:25]]
        Path(a.md).parent.mkdir(parents=True, exist_ok=True)
        Path(a.md).write_text("\n".join(L) + "\n", encoding="utf-8")
        print("wrote", a.md)


if __name__ == "__main__":
    main()
