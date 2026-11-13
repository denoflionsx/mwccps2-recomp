"""What the tools share: where things are, how to start a compiler, and how to read a PE file.

  ROOT           the repository
  ORIG           the original compiler's folder (orig/, or $MWCC_ORIG): mwccps2.exe, LMGR326B.dll
  BUILD          build output (build/): mwccps2.exe is the hybrid
  DATA           analysis data kept in the repository (docs/data/)

On Linux the compilers run under wibo (orig/wibo, or $WIBO); launch() gives the command prefix.
"""
import os
import shutil
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ORIG = Path(os.environ.get("MWCC_ORIG", ROOT / "orig"))
ORIG_EXE = ORIG / "mwccps2.exe"
BUILD = ROOT / "build"
HYBRID_EXE = BUILD / "mwccps2.exe"
DATA = ROOT / "docs" / "data"
SOURCE_DIRS = ("hybrid", "src", "ext")

# the original, as published by decomp.me (its license DLL included)
ORIG_URL = "https://github.com/decompme/compilers/releases/download/compilers/mwcps2-3.0.1-020123.tar.gz"
ORIG_ARCHIVE_SHA256 = "72d823d85943c92ca13378e93d56ac1870298ccbfda0ef8a2efb9650692e968c"
ORIG_EXE_SHA256 = "488675890770ef9940a0db12ae2181e729bad55bccd8991e0d370754e4a021b8"
WIBO_URL = "https://github.com/decompals/wibo/releases/download/1.2.0/wibo-x86_64"
WIBO_SHA256 = "13f86a2d618f0dbe67179d349625345eabf9b46450295cb4c904e49f6aff85af"

IMAGE_BASE = 0x400000


def launch():
    """The command that starts a Windows exe here: nothing on Windows, wibo elsewhere."""
    if sys.platform == "win32":
        return []
    wibo = os.environ.get("WIBO") or (str(ORIG / "wibo") if (ORIG / "wibo").exists() else shutil.which("wibo"))
    if not wibo:
        sys.exit("wibo is needed to run the compiler here: run tools/setup.py, or set WIBO")
    return [wibo]


def need_orig():
    if not ORIG_EXE.exists():
        sys.exit(f"{ORIG_EXE} is missing: run tools/setup.py, or point MWCC_ORIG at the compiler's folder")
    return ORIG_EXE


def sources():
    """Every C source of the hybrid: its own code, the ported units, the extensions."""
    return sorted(p for d in SOURCE_DIRS for p in (ROOT / d).rglob("*.c"))


def functions():
    """{function start: [(block start, size), ...]} of the original, from Ghidra's basic blocks."""
    out = {}
    for line in open(DATA / "blocks.tsv", encoding="utf-8"):
        start, size, fn = (int(x, 16) for x in line.split("\t"))
        out.setdefault(fn, []).append((start, size))
    return out


class PE:
    """A PE file read by hand: headers, sections, the image as loaded, imports and base relocations."""

    def __init__(self, path):
        self.path = Path(path)
        raw = self.raw = self.path.read_bytes()
        self.pe = struct.unpack_from("<I", raw, 0x3C)[0]
        self.nsec, = struct.unpack_from("<H", raw, self.pe + 6)
        optsize, = struct.unpack_from("<H", raw, self.pe + 20)
        self.opt = self.pe + 24
        self.entry, = struct.unpack_from("<I", raw, self.opt + 16)
        self.base, self.section_align, self.file_align = struct.unpack_from("<III", raw, self.opt + 28)
        self.size_image, self.size_headers = struct.unpack_from("<II", raw, self.opt + 56)
        ndirs, = struct.unpack_from("<I", raw, self.opt + 92)
        self.dirs = [struct.unpack_from("<II", raw, self.opt + 96 + 8 * i) for i in range(ndirs)]
        self.table = self.opt + optsize
        self.sections = []  # (name, rva, virtual size, file offset, raw size, flags)
        image = bytearray(self.size_image)
        for i in range(self.nsec):
            name, vsize, rva, rsize, roff, _, _, _, _, flags = struct.unpack_from("<8sIIIIIIHHI", raw, self.table + 40 * i)
            self.sections.append((name.rstrip(b"\0").decode(), rva, vsize, roff, rsize, flags))
            n = min(vsize, rsize) if vsize else rsize
            image[rva:rva + n] = raw[roff:roff + n]
        self.image = bytes(image)

    def section(self, name):
        return next(s for s in self.sections if s[0] == name)

    def bytes(self, va, size):
        return self.image[va - self.base:va - self.base + size]

    def cstr(self, rva):
        return self.image[rva:self.image.index(b"\0", rva)].decode("latin-1")

    def import_descriptors(self):
        """The raw 20-byte import descriptors, without the null one that ends the table."""
        out, rva = [], self.dirs[1][0]
        while rva:
            d = struct.unpack_from("<IIIII", self.image, rva)
            if not d[3]:
                break
            out.append(d)
            rva += 20
        return out

    def imports(self):
        """{address of the import table slot: 'DLL!name'}"""
        out = {}
        for lookup, _, _, name, iat in self.import_descriptors():
            dll = self.cstr(name)
            slot = 0
            while True:
                entry, = struct.unpack_from("<I", self.image, (lookup or iat) + 4 * slot)
                if not entry:
                    break
                what = "#%d" % (entry & 0xFFFF) if entry & 0x80000000 else self.cstr(entry + 2)
                out[self.base + iat + 4 * slot] = f"{dll}!{what}"
                slot += 1
        return out

    def relocs(self):
        """Addresses of the 32-bit values the loader would rebase (HIGHLOW entries)."""
        out = set()
        rva, size = self.dirs[5] if len(self.dirs) > 5 else (0, 0)
        end = rva + size
        while rva < end:
            page, block = struct.unpack_from("<II", self.image, rva)
            if not block:
                break
            for k in range((block - 8) // 2):
                e, = struct.unpack_from("<H", self.image, rva + 8 + 2 * k)
                if e >> 12 == 3:
                    out.add(self.base + page + (e & 0xFFF))
            rva += block
        return out


class Ins:
    """One instruction, with the names the tools grew up with (capstone's)."""
    __slots__ = ("address", "size", "bytes", "mnemonic", "op_str")

    def __repr__(self):
        return f"{self.address:08x} {self.mnemonic} {self.op_str}"


_formatter = None
_STRING_OPS = {  # capstone names the implied operands of string instructions
    "movs": "{m} ptr es:[edi], {m} ptr [esi]", "stos": "{m} ptr es:[edi], {r}", "scas": "{r}, {m} ptr es:[edi]",
    "cmps": "{m} ptr [esi], {m} ptr es:[edi]", "lods": "{r}, {m} ptr [esi]",
}
_STRING_SIZE = {"b": ("byte", "al"), "w": ("word", "ax"), "d": ("dword", "eax")}


def disasm(code, va):
    """The instructions of code at va (iced-x86), spelled the way capstone's Intel syntax spells
    them: `mov eax, dword ptr [esp + 0x10]`. The tools match these strings. (Checked against
    capstone over every block of the exe; a handful of rare forms still differ.)"""
    global _formatter
    try:
        from iced_x86 import Decoder, Formatter, FormatterSyntax, MemorySizeOptions, OpKind
    except ImportError:
        sys.exit("this tool needs iced-x86: pip install iced-x86")
    if _formatter is None:
        f = _formatter = Formatter(FormatterSyntax.INTEL)
        f.hex_prefix, f.hex_suffix, f.uppercase_hex = "0x", "", False
        f.space_after_operand_separator = True
        f.space_between_memory_add_operators = True
        f.memory_size_options = MemorySizeOptions.ALWAYS
        f.show_branch_size = False
        f.branch_leading_zeros = False
        f.rip_relative_addresses = False
        f.signed_immediate_operands = False
        f.signed_memory_displacements = True
    code = bytes(code)
    for raw in Decoder(32, code, ip=va):
        if raw.is_invalid:
            break
        ins = Ins()
        ins.address, ins.size = raw.ip, raw.len
        ins.bytes = code[raw.ip - va:raw.ip - va + raw.len]
        text = _formatter.format(raw)
        ins.mnemonic, _, ins.op_str = text.partition(" ")
        if ins.mnemonic in ("rep", "repe", "repne", "lock") and ins.op_str:
            more, _, ins.op_str = ins.op_str.partition(" ")
            ins.mnemonic += " " + more
        last = ins.mnemonic.split(" ")[-1]
        if last[:4] in _STRING_OPS and last[4:] in _STRING_SIZE and len(last) == 5:
            m, r = _STRING_SIZE[last[4:]]
            ins.op_str = _STRING_OPS[last[:4]].format(m=m, r=r)
        elif last == "lea":  # no size on lea's operand
            ins.op_str = ins.op_str.replace("dword ptr ", "").replace("word ptr ", "").replace("byte ptr ", "")
        elif last == "fwait":
            ins.mnemonic = "wait"
        elif last[0] == "f":  # the x87 stack top is implied
            if ins.op_str.startswith("st, "):
                ins.op_str = ins.op_str[4:]
            elif ins.op_str.endswith(", st"):
                ins.op_str = ins.op_str[:-4]
            elif not ins.op_str and raw.op_count == 2:
                ins.op_str = "st(1)"
            ins.op_str = ins.op_str.replace("tbyte ptr", "xword ptr")
        elif last == "imul" and raw.op_count == 3 and ins.op_str.count(",") == 1:
            ins.op_str = ins.op_str.split(",")[0] + ", " + ins.op_str
        # an immediate that was sign-extended from a byte reads as negative
        for i in range(raw.op_count):
            kind = raw.op_kind(i)
            if kind in (OpKind.IMMEDIATE8TO32, OpKind.IMMEDIATE8TO16):
                bits = 16 if kind == OpKind.IMMEDIATE8TO16 else 32
                value = raw.immediate(i) & ((1 << bits) - 1)
                if value >> (bits - 1) and last not in ("and", "or", "xor", "test"):
                    neg = (1 << bits) - value
                    old = "0x%x" % value
                    new = "-%s" % (neg if neg < 10 else "0x%x" % neg)
                    if ins.op_str.endswith(old):
                        ins.op_str = ins.op_str[:-len(old)] + new
        yield ins


def optional_json(name):
    """docs/data/<name> if it is there, else an empty dict: names.json (address -> name) and
    modmap.json (address -> {name, file}) are Ghidra exports that are not in the repository."""
    import json

    path = DATA / name
    return json.load(open(path, encoding="utf-8")) if path.exists() else {}


def our_functions():
    """(address, size, name, source file) of the original's functions: from the Ghidra exports
    x86map.json and modmap.json in docs/data/ when they are there, else from the block list
    (then without names and files)."""
    listed, mod = optional_json("x86map.json"), optional_json("modmap.json")
    if listed:
        return [(f["va"], f["size"], f.get("name"), mod.get(f"{f['va']:x}", {}).get("file")) for f in listed]
    return [(fn, max(start + size for start, size in blocks) - fn, None, None) for fn, blocks in sorted(functions().items())]


def hook_addresses():
    """{original address: replacement's name} of every HOOK() in the sources."""
    import re

    out = {}
    for src in sources():
        for m in re.finditer(r"^HOOK\((0x[0-9a-fA-F]+),\s*(\w+)\)", src.read_text(encoding="utf-8"), re.M):
            out[int(m[1], 16)] = m[2]
    return out
