"""Match functions of mwccps2.exe against a mweppc-decompiled GameCube mwcceppc.exe.

Each function's bytes are normalized: dwords the .reloc table patches (absolute
addresses) and rel32 call/jmp targets become wildcards. Functions then match
  exact  - same normalized bytes
  close  - same length and >= 90% equal instruction shapes (mnemonic + operand kinds)
Writes a JSON map (our va -> their name, kind) and prints a per-source-file summary.

usage: python tools/gcmatch.py [--theirs GC/2.0] [--out docs/data/gcmatch_GC_2_0.json]
"""

import os
import sys
import argparse
import hashlib
import json
import re
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
from common import BUILD, DATA, ORIG_EXE, ROOT  # noqa: E402
MWEPPC = Path(os.environ.get("MWEPPC", ROOT.parent / "mweppc"))  # a checkout of doldecomp/mweppc
OURS = ORIG_EXE


class Image:
    def __init__(self, path):
        self.pe = common.PE(path)
        self.base = self.pe.base
        self.data = self.pe.image
        self.relocs = self.pe.relocs()

    def bytes(self, va, size):
        off = va - self.base
        return self.data[off : off + size]



def normalize(img, va, size):
    """Return (masked bytes, instruction shape list)."""
    code = bytearray(img.bytes(va, size))
    for r in range(va, va + size):
        if r in img.relocs:
            code[r - va : r - va + 4] = b"\xaa\xaa\xaa\xaa"
    shapes = []
    for ins in common.disasm(img.bytes(va, size), va):
        b = ins.bytes
        if ins.mnemonic in ("call", "jmp") and b[0] in (0xE8, 0xE9) and len(b) == 5:
            o = ins.address - va + 1
            code[o : o + 4] = b"\xbb\xbb\xbb\xbb"
        op = re.sub(r"0x[0-9a-f]+", "N", ins.op_str)
        shapes.append(f"{ins.mnemonic} {op}")
    return bytes(code), shapes


def our_functions():
    return common.our_functions()


def their_functions(version):
    out = []
    pat = re.compile(r"^(\S+) = \.text:0x([0-9A-F]+); // type:function size:0x([0-9A-F]+)")
    for line in open(MWEPPC / f"config/{version}/symbols.txt", encoding="utf-8"):
        m = pat.match(line)
        if m:
            out.append((int(m[2], 16), int(m[3], 16), m[1]))
    # source file of each function from splits.txt
    files = []
    cur = None
    for line in open(MWEPPC / f"config/{version}/splits.txt", encoding="utf-8"):
        if line and not line[0].isspace() and line.rstrip().endswith(":"):
            cur = line.strip()[:-1]
        m = re.match(r"\s+\.text\s+start:0x([0-9A-F]+) end:0x([0-9A-F]+)", line)
        if m and cur:
            files.append((int(m[1], 16), int(m[2], 16), cur))

    def file_of(va):
        for s, e, f in files:
            if s <= va < e:
                return f
        return None

    return [(va, size, name, file_of(va)) for va, size, name in out]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--theirs", default="GC_2_0")
    ap.add_argument("--exe", default=None, help="their original exe (default build/compilers/GC/<ver>/mwcceppc.exe)")
    ap.add_argument("--out", default=None)
    a = ap.parse_args()
    ver_dir = a.theirs.replace("GC_", "").replace("_", ".")
    exe = Path(a.exe) if a.exe else MWEPPC / f"build/compilers/GC/{ver_dir}/mwcceppc.exe"
    ours_img, theirs_img = Image(OURS), Image(exe)

    theirs = their_functions(a.theirs)
    by_hash = defaultdict(list)
    by_len = defaultdict(list)
    for va, size, name, f in theirs:
        code, shapes = normalize(theirs_img, va, size)
        by_hash[hashlib.sha1(code).digest()].append((name, f, va))
        by_len[len(shapes)].append((name, f, va, shapes))

    result = {}
    stats = Counter()
    per_file = defaultdict(Counter)
    for va, size, name, ofile in our_functions():
        code, shapes = normalize(ours_img, va, size)
        # our sizes can include trailing padding: trim zero bytes
        h = hashlib.sha1(code).digest()
        hit = by_hash.get(h)
        # our sizes may include the zero padding up to the next 16-byte boundary
        k = len(code)
        while not hit and k > 0 and len(code) - k < 16 and code[k - 1] == 0:
            k -= 1
            hit = by_hash.get(hashlib.sha1(code[:k]).digest())
        kind = None
        if hit:
            kind, (tname, tfile, tva) = "exact", hit[0]
        else:
            best = None
            n = len(shapes)
            for cand_n in (n, n - 1, n + 1, n - 2, n + 2):
                for tname, tfile, tva, tsh in by_len.get(cand_n, ()):
                    if len(tsh) != len(shapes) and abs(len(tsh) - len(shapes)) > 2:
                        continue
                    same = sum(x == y for x, y in zip(shapes, tsh))
                    score = same / max(len(shapes), len(tsh), 1)
                    if best is None or score > best[0]:
                        best = (score, tname, tfile, tva)
            if best and best[0] >= 0.9 and n >= 4:
                kind = "close"
                _, tname, tfile, tva = best
        stats[kind or "none"] += 1
        per_file[ofile or "?"][kind or "none"] += 1
        if kind:
            result[f"{va:x}"] = {"kind": kind, "name": tname, "file": tfile, "their_va": f"{tva:x}", "our_file": ofile}

    print(f"ours: {sum(stats.values())} functions vs {a.theirs}: {len(theirs)}")
    print(dict(stats))
    print(f"{'our module':32} {'exact':>6} {'close':>6} {'none':>6}")
    for f, c in sorted(per_file.items(), key=lambda kv: -sum(kv[1].values())):
        print(f"{f:32} {c['exact']:6} {c['close']:6} {c['none']:6}")
    out = Path(a.out) if a.out else DATA / f"gcmatch_{a.theirs}.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=1), encoding="utf-8")
    print("wrote", out)


if __name__ == "__main__":
    main()
