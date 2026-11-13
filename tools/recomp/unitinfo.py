"""Everything needed to hand-port one mweppc unit, in one listing (docs/RECOMP.md recipe).

Prints the unit's source, then for each of its functions our address, size and corpus coverage
(from the latest coverage.json), then our disassembly with call targets and absolute addresses
named from docs/recomp/addrmap_GC_2_0.json, and each string they point at.

usage: python tools/recomp/unitinfo.py src/driver/CLToolExec.c [--no-source] [--cov <difftest run dir>]
"""
import argparse
import json
import re
import struct
import sys
from pathlib import Path

import capstone
import pefile

ROOT = Path(__file__).resolve().parents[2]
MWEPPC = ROOT.parent / "mweppc"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("unit")
    ap.add_argument("--no-source", action="store_true")
    covs = sorted((ROOT / "build/recomp/difftest").glob("*/coverage.json"), key=lambda p: p.stat().st_mtime)
    ap.add_argument("--cov", default=str(covs[-1].parent if covs else ROOT / "build/recomp/difftest/cov5"),
                    help="a difftest run with coverage.json (default: the newest)")
    a = ap.parse_args()

    amap = json.load(open(ROOT / "docs/recomp/addrmap_GC_2_0.json", encoding="utf-8"))["symbols"]
    by_ours = {int(v["ours"], 16): k for k, v in amap.items()}
    cov = {}
    cj = Path(a.cov) / "coverage.json"
    if cj.exists():
        cov = json.load(open(cj, encoding="utf-8"))["functions_detail"]

    # the unit's functions, from mweppc's splits and symbols
    rng = []
    cur = None
    for line in open(MWEPPC / "config/GC_2_0/splits.txt", encoding="utf-8"):
        if line.strip().endswith(":") and not line[0].isspace():
            cur = line.strip()[:-1]
        m = re.match(r"\s+\.text\s+start:0x([0-9A-F]+) end:0x([0-9A-F]+)", line)
        if m and cur == a.unit:
            rng.append((int(m[1], 16), int(m[2], 16)))
    funcs = []
    pat = re.compile(r"^(\S+) = \.text:0x([0-9A-F]+); // type:function size:0x([0-9A-F]+)")
    for line in open(MWEPPC / "config/GC_2_0/symbols.txt", encoding="utf-8"):
        m = pat.match(line)
        if m and any(s <= int(m[2], 16) < e for s, e in rng):
            funcs.append(m[1])

    if not a.no_source:
        print(open(MWEPPC / a.unit, encoding="utf-8", errors="replace").read())
    print("=" * 100)
    pe = pefile.PE(str(ROOT / "recomp_goal/mwcps2-3.0.1-020123/mwccps2.exe"))
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

    def string_at(addr):
        off = addr - base
        if not 0 <= off < len(img):
            return None
        s = img[off : off + 120].split(b"\0")[0]
        return s.decode("latin1") if len(s) >= 2 and all(32 <= c < 127 or c in (9, 10, 13) for c in s) else None

    def name(addr):
        n = by_ours.get(addr)
        if n:
            return n
        s = string_at(addr)
        return repr(s) if s else None

    for f in funcs:
        v = amap.get(f)
        if not v:
            print(f"-- {f}: no address in our exe (not in the map)")
            continue
        ours, size = int(v["ours"], 16), v["size"]
        c = cov.get(f"{ours:x}")
        cstr = f"blocks run {c['run']}/{c['blocks']} in {c['cases']} cases" if c else "no coverage data"
        print(f"-- {f} @ {ours:x}, {size} bytes, {cstr}")
        for i in md.disasm(img[ours - base : ours - base + size], ours):
            notes = []
            for h in re.findall(r"0x[0-9a-f]+", i.op_str):
                n = name(int(h, 16))
                if n:
                    notes.append(n)
            print(f"  {i.address:08x}  {i.mnemonic:6} {i.op_str:40} {'; ' + ', '.join(notes) if notes else ''}")


if __name__ == "__main__":
    main()
