"""Name the instruction that ends at an address (a watchpoint's 'next eip') and its function."""
import sys
import bisect
import json
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
from common import BUILD, DATA, ORIG_EXE, ROOT  # noqa: E402

_pe = common.PE(ORIG_EXE)
_base, _img = _pe.base, _pe.image
_rows = [l.split("\t") for l in open(DATA / "blocks.tsv", encoding="utf-8")]
_fstarts = sorted({int(r[2], 16) for r in _rows})
_names = common.optional_json("names.json")
_mod = common.optional_json("modmap.json")


def describe(nxt, call=False):
    if not 0x401000 <= nxt < 0x50c6ca:
        return "(outside the exe)"
    f = _fstarts[bisect.bisect_right(_fstarts, nxt) - 1]
    k = f"{f:x}"
    prev = None
    for i in common.disasm(_img[f - _base : nxt - _base + 16], f):
        if i.address + i.size == nxt or (i.address == nxt and i.mnemonic.startswith("rep")):
            prev = i
            break
        if i.address > nxt:
            break
    ins = f"{prev.address:x} {prev.mnemonic} {prev.op_str}" if prev else "?"
    return f"{ins}  in {k} {_names.get(k) or _mod.get(k, {}).get('name')} ({_mod.get(k, {}).get('file')})"
