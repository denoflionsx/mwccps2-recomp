"""Static census of operand-field accesses inside PCode instructions (back end only).

PCode instructions keep their operands at +0x34 + 22*k. For every `[reg + disp]` access (reg not
esp) in a back-end function with disp in that area, this records slot k, field f = byte offset
in the 22-byte record, size, and read or write. Fields that some builder leaves unset (VReg.c)
and that nothing writes later are where leftovers can be read.

usage: python tools/opfields.py [--field 18] [--json docs/data/opfields.json]
"""
import sys
import argparse
import json
import re
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
from common import BUILD, DATA, ORIG_EXE, ROOT  # noqa: E402
SZ = {"byte": 1, "word": 2, "dword": 4, "qword": 8}
BACKEND = 0x48B210
NSLOTS = 6


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--field", type=int, action="append", help="list the accesses touching this field")
    ap.add_argument("--json")
    a = ap.parse_args()
    pe = common.PE(ORIG_EXE)
    base, img = pe.base, pe.image
    names = common.optional_json("names.json")
    mod = common.optional_json("modmap.json")
    pat = re.compile(r"(byte|word|dword|qword) ptr \[(e[a-z]{2})(?: \+ e[a-z]{2}(?:\*\d)?)? \+ (0x[0-9a-f]+)\]")
    acc = defaultdict(lambda: {"read": set(), "write": set()})
    detail = []
    for line in open(DATA / "blocks.tsv", encoding="utf-8"):
        s, n, f = (int(x, 16) for x in line.split("\t"))
        if f < BACKEND:
            continue
        for i in common.disasm(img[s - base : s - base + n], s):
            m = pat.search(i.op_str)
            if not m or m[2] == "esp" or i.mnemonic in ("lea",):
                continue
            disp, size = int(m[3], 16), SZ[m[1]]
            if not 0x34 <= disp < 0x34 + 22 * NSLOTS:
                continue
            write = i.op_str.startswith(m[0]) and i.mnemonic not in ("cmp", "test", "push")
            rel = disp - 0x34
            slot, field = divmod(rel, 22)
            for b in range(size):
                fb = (rel + b) % 22
                acc[fb]["write" if write else "read"].add(f)
            k = f"{f:x}"
            detail.append({"addr": f"{i.address:x}", "func": k,
                           "name": names.get(k) or mod.get(k, {}).get("name"), "file": mod.get(k, {}).get("file"),
                           "slot": slot, "field": field, "size": size, "write": write,
                           "ins": f"{i.mnemonic} {i.op_str}"})
    print("field  read-in-funcs  write-in-funcs")
    for fb in range(22):
        print(f"{fb:5}  {len(acc[fb]['read']):13}  {len(acc[fb]['write']):14}")
    for fld in a.field or []:
        print(f"-- accesses touching field {fld}")
        for d in detail:
            if d["field"] <= fld < d["field"] + d["size"]:
                print(f"  {d['addr']} {'W' if d['write'] else 'R'} slot{d['slot']} {d['ins']:44} {d['name']} ({d['file']})")
    if a.json:
        Path(a.json).write_text(json.dumps(detail, indent=1), encoding="utf-8")


if __name__ == "__main__":
    main()
