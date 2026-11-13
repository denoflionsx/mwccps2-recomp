"""Map addresses in mweppc's GameCube compiler (GC 2.0 mwcceppc.exe) to ours, and name them.

Byte-identical function pairs (docs/recomp/gcmatch_GC_2_0.json, kind exact) differ only in the
dwords the .reloc table patches and in rel32 call/jmp targets. Walking each pair side by side
gives (their address, our address) for every global, constant and function those functions use.
Every mweppc symbol (config/GC_2_0/symbols.txt) that contains a mapped address then gets our
address, if all its mapped addresses agree on one offset.

Writes docs/recomp/addrmap_GC_2_0.json: {name: {"ours": hex, "theirs": hex, "kind": function|data,
"size": n, "evidence": count}} plus the conflicts it skipped.

usage: python tools/recomp/addrmap.py
"""
import json
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from gcmatch import MWEPPC, OURS, Image, their_functions  # noqa: E402

import capstone  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


def main():
    ours, theirs = Image(OURS), Image(MWEPPC / "build/compilers/GC/2.0/mwcceppc.exe")
    pairs = json.load(open(ROOT / "docs/recomp/gcmatch_GC_2_0.json", encoding="utf-8"))
    sizes = {va: size for va, size, _, _ in their_functions("GC_2_0")}
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    votes = defaultdict(Counter)  # their address -> Counter(our address)
    # a pair only votes if its normalized body is unique on both sides (identical stubs and
    # destructors were paired arbitrarily by gcmatch)
    from gcmatch import normalize, our_functions
    import hashlib

    def h(img, va, size):
        return hashlib.sha1(normalize(img, va, size)[0].rstrip(b"\0")).digest()

    their_count = Counter(h(theirs, va, size) for va, size in sizes.items())
    our_count = Counter(h(ours, va, size) for va, size, _, _ in our_functions())
    for k, v in pairs.items():
        if v["kind"] != "exact":
            continue
        ova, tva = int(k, 16), int(v["their_va"], 16)
        hb = h(theirs, tva, sizes[tva])
        if their_count[hb] > 1 or our_count[hb] > 1:
            continue
        size = sizes[tva]
        votes[tva][ova] += 1  # the function itself
        tb, ob = theirs.bytes(tva, size), ours.bytes(ova, size)
        for off in range(size - 3):
            if tva + off in theirs.relocs and ova + off in ours.relocs:
                t = struct.unpack_from("<I", tb, off)[0]
                o = struct.unpack_from("<I", ob, off)[0]
                votes[t][o] += 1
        for i in md.disasm(bytes(tb), tva):
            if i.mnemonic in ("call", "jmp") and i.bytes[0] in (0xE8, 0xE9) and len(i.bytes) == 5:
                off = i.address - tva
                t = i.address + 5 + struct.unpack_from("<i", tb, off + 1)[0]
                o = ova + off + 5 + struct.unpack_from("<i", ob, off + 1)[0]
                votes[t][o] += 1

    # their symbols, functions and data, by address
    syms = []
    pat = re.compile(r"^(\S+) = (\.\w+):0x([0-9A-F]+); // type:(\w+)(?: size:0x([0-9A-F]+))?")
    for line in open(MWEPPC / "config/GC_2_0/symbols.txt", encoding="utf-8"):
        m = pat.match(line)
        if m:
            syms.append((int(m[3], 16), int(m[5] or "1", 16), m[1], m[4], m[2]))
    syms.sort()
    starts = [s[0] for s in syms]
    import bisect

    by_sym = defaultdict(Counter)  # symbol index -> Counter(delta = ours - theirs)
    for t, c in votes.items():
        i = bisect.bisect_right(starts, t) - 1
        if i < 0:
            continue
        start, size, *_ = syms[i]
        if not start <= t < start + max(size, 1):
            continue
        for o, n in c.items():
            by_sym[i][o - t] += n
    out, conflicts = {}, {}
    for i, deltas in by_sym.items():
        start, size, name, kind, section = syms[i]
        (delta, n), *rest = deltas.most_common()
        if rest and not (n >= 3 and n >= 4 * rest[0][1]):
            conflicts[name] = {f"{d:+x}": c for d, c in deltas.items()}
            continue
        out[name] = {"ours": f"{start + delta:x}", "theirs": f"{start:x}", "kind": kind, "section": section,
                     "size": size, "evidence": n}
    path = ROOT / "docs/recomp/addrmap_GC_2_0.json"
    path.write_text(json.dumps({"symbols": out, "conflicts": conflicts}, indent=1), encoding="utf-8")
    kinds = Counter(v["kind"] for v in out.values())
    print(f"{len(out)} symbols mapped ({dict(kinds)}), {len(conflicts)} with conflicting offsets -> {path}")


if __name__ == "__main__":
    main()
