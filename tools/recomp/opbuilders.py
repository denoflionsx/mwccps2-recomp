"""Find functions that build a 22-byte operand record in a stack local and copy all of it out
(5 x movsd + movsw from `lea esi, [esp+X]`), and report which bytes of the local they write first.

The bytes a builder never writes go out as stack leftovers (docs/RECOMP.md, "Address-dependent
output"). Static and path-insensitive: a byte counts as set if any path in the function writes it.

usage: python tools/recomp/opbuilders.py [--json docs/recomp/opbuilders.json]
"""
import argparse
import json
import re
from pathlib import Path

import capstone
import pefile

ROOT = Path(__file__).resolve().parents[2]
EXE = ROOT / "recomp_goal/mwcps2-3.0.1-020123/mwccps2.exe"
SZ = {"byte": 1, "word": 2, "dword": 4, "qword": 8}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--json")
    a = ap.parse_args()
    pe = pefile.PE(str(EXE))
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    rows = [l.split("\t") for l in open(ROOT / "docs/recomp/blocks.tsv", encoding="utf-8")]
    funcs = {}
    for r in rows:
        s, n, f = int(r[0], 16), int(r[1], 16), int(r[2], 16)
        funcs.setdefault(f, []).append((s, n))
    names = json.load(open(ROOT / "docs/names.json", encoding="utf-8"))
    mod = json.load(open(ROOT / "docs/modmap.json", encoding="utf-8"))
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    out = []
    for f, blocks in sorted(funcs.items()):
        ins = []
        for s, n in sorted(blocks):
            ins += list(md.disasm(img[s - base : s - base + n], s))
        # track esp offset (pushes/pops/sub/add) linearly; good enough for these small builders
        delta = 0
        writes = {}  # frame offset (relative to esp at entry after prologue) -> set of bytes
        copies = []
        for k, i in enumerate(ins):
            op = i.op_str
            if i.mnemonic == "push":
                delta += 4
            elif i.mnemonic == "pop":
                delta -= 4
            elif i.mnemonic == "sub" and op.startswith("esp, "):
                delta += int(op.split(", ")[1], 0)
            elif i.mnemonic == "add" and op.startswith("esp, "):
                delta -= int(op.split(", ")[1], 0)
            m = re.match(r"(byte|word|dword) ptr \[esp(?: \+ (0x[0-9a-f]+|\d+))?\], ", op)
            if m and i.mnemonic in ("mov", "and", "or"):
                off = int(m[2] or "0", 0) - delta
                for b in range(SZ[m[1]]):
                    writes.setdefault(off + b, set()).add(i.address)
            m = re.match(r"esi, \[esp(?: \+ (0x[0-9a-f]+|\d+))?\]", op)
            if i.mnemonic == "lea" and m:
                seq = [x.mnemonic + " " + x.op_str for x in ins[k + 1 : k + 7]]
                mv = [s for s in seq if s.startswith("movs")]
                if len(mv) >= 6 and mv[:5] == ["movsd dword ptr es:[edi], dword ptr [esi]"] * 5 and mv[5].startswith("movsw"):
                    copies.append((i.address, int(m[1] or "0", 0) - delta))
        for at, src in copies:
            # a call before the copy that took the record's address fills it (e.g. fn_4980b0): mark it
            filled_by_call = any(
                x.mnemonic == "lea" and re.match(r"e[a-z]x, \[esp(?: \+ (0x[0-9a-f]+|\d+))?\]", x.op_str)
                and int((re.search(r"\+ (0x[0-9a-f]+|\d+)", x.op_str) or [None, "0"])[1], 0) - 0 >= 0
                for x in ins if x.address < at
            )
            set_bytes = sorted(b - src for b in writes if 0 <= b - src < 22)
            unset = [b for b in range(22) if b not in set_bytes]
            k = f"{f:x}"
            out.append({
                "func": k, "name": names.get(k) or mod.get(k, {}).get("name"), "file": mod.get(k, {}).get("file"),
                "copy_at": f"{at:x}", "set": set_bytes, "unset": unset,
            })
    for o in out:
        print(f"{o['func']} {str(o['name']):32} {str(o['file']):28} copy@{o['copy_at']} unset {len(o['unset'])}: {o['unset'][:22]}")
    print(len(out), "copies of a 22-byte local")
    if a.json:
        Path(a.json).write_text(json.dumps(out, indent=1), encoding="utf-8")


if __name__ == "__main__":
    main()
