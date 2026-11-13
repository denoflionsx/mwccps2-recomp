"""Name the instruction that ends at an address (a watchpoint's 'next eip') and its function."""
import bisect
import json
from pathlib import Path

import capstone
import pefile

ROOT = Path(__file__).resolve().parents[2]
_pe = pefile.PE(str(ROOT / "recomp_goal/mwcps2-3.0.1-020123/mwccps2.exe"))
_base = _pe.OPTIONAL_HEADER.ImageBase
_img = _pe.get_memory_mapped_image()
_rows = [l.split("\t") for l in open(ROOT / "docs/recomp/blocks.tsv", encoding="utf-8")]
_fstarts = sorted({int(r[2], 16) for r in _rows})
_names = json.load(open(ROOT / "docs/names.json", encoding="utf-8"))
_mod = json.load(open(ROOT / "docs/modmap.json", encoding="utf-8"))
_md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def describe(nxt, call=False):
    if not 0x401000 <= nxt < 0x50c6ca:
        return "(outside the exe)"
    f = _fstarts[bisect.bisect_right(_fstarts, nxt) - 1]
    k = f"{f:x}"
    prev = None
    for i in _md.disasm(_img[f - _base : nxt - _base + 16], f):
        if i.address + i.size == nxt or (i.address == nxt and i.mnemonic.startswith("rep")):
            prev = i
            break
        if i.address > nxt:
            break
    ins = f"{prev.address:x} {prev.mnemonic} {prev.op_str}" if prev else "?"
    return f"{ins}  in {k} {_names.get(k) or _mod.get(k, {}).get('name')} ({_mod.get(k, {}).get('file')})"
