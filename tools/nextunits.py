"""List mweppc units with byte-identical functions not yet ported, by how many of those the corpus runs.

Default: units whose functions are all byte-identical and none ported. --partial: every unit, counting
only its byte-identical, unported functions that the corpus runs (units with some ported are included).
"""
import collections, glob, json, re, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
from common import BUILD, DATA, ORIG_EXE, ROOT  # noqa: E402
d = json.load(open(DATA / "gcmatch_GC_2_0.json"))
ported = set()
for f in (str(p) for p in (ROOT / "src").rglob("*.c")):
    for m in re.finditer(r"^HOOK\(0x([0-9a-f]+)", open(f, encoding="utf-8").read(), re.M):
        ported.add(m[1])
covs = sorted((str(p) for p in (BUILD / "difftest").glob("*/coverage.json")), key=lambda p: Path(p).stat().st_mtime)
cov = json.load(open(covs[-1], encoding="utf-8"))["functions_detail"] if covs else {}
by = collections.defaultdict(lambda: [0, 0, 0, 0])
for a, v in d.items():
    f = v.get("file")
    if not f:
        continue
    b = by[f]
    b[0] += 1
    b[1] += v["kind"] == "exact"
    b[2] += a in ported
    c = cov.get(a)
    b[3] += bool(c and c.get("cases"))
if "--partial" in sys.argv:
    left = collections.Counter()
    for a, v in d.items():
        c = cov.get(a)
        if v.get("file") and v["kind"] == "exact" and a not in ported and c and c.get("cases"):
            left[v["file"]] += 1
    for f, k in left.most_common():
        print(f"{k:3d} unported exact+run / {by[f][0]:3d}  {f}")
    sys.exit()
rows = sorted(((f, *b) for f, b in by.items() if b[0] == b[1] and b[2] == 0), key=lambda r: -r[4])
for f, n, ex, p, run in rows:
    if run:
        print(f"{run:3d} run / {n:3d}  {f}")
