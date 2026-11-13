"""Build the hybrid compiler: build/mwccps2.exe, the original with the recompiled code merged in.

  python tools/build.py                  everything: hybrid/, src/ (the ported units), ext/
  python tools/build.py --ports none     hybrid/ and ext/ only: the original's code with the extensions
  python tools/build.py --out DIR --cflags=-DRECOMP_NEGATIVE_CONTROL_X    another build, e.g. a control
  python tools/build.py --image FILE --out DIR    merge an image linked elsewhere (obj/mwrecomp.dll of
                                                  a build: the recompiled code without the original)

The sources are compiled with clang for i686-pc-windows-msvc and linked by lld-link into an image
of their own. The merge then copies that image's sections into a copy of the exe (behind the
exe's own, at IMAGE_AT), adds its imports to the exe's and makes hybrid/hook.c's mwrecomp_entry
the entry point. The result is one exe; the original file is not changed.

Needs clang, lld-link and llvm-dlltool (LLVM) on PATH, and the original in orig/ (tools/setup.py).
The import library for KERNEL32 is generated from hybrid/kernel32.def, so no Windows SDK is needed.
"""
import argparse
import concurrent.futures as cf
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from common import BUILD, DATA, IMAGE_BASE, ORIG, PE, ROOT, functions, need_orig  # noqa: E402

CFLAGS = ["--target=i686-pc-windows-msvc", "-O2", "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
          "-fwrapv", "-fno-strict-aliasing", "-fno-delete-null-pointer-checks", "-mno-sse",
          "-Wall", "-Wno-unused-function", "-Wno-unused-variable", "-DRECOMP_HYBRID=1",
          f"-I{ROOT / 'include'}", f"-I{ROOT / 'hybrid'}"]
HEADERS = [ROOT / "include", ROOT / "hybrid"]
ORIG_ENTRY = 0x1000  # hybrid/hook.c jumps there (0x401000)
# Where the hybrid's own image goes. Addresses of ported code end up in memory the compiler later
# reads as leftovers (return addresses on the stack, docs/ENTROPY.md), so the address shows in its
# output: right behind the exe (0x595000) three game units compiled differently from the original,
# here none does. A section without contents fills the gap.
IMAGE_AT = 0x1000000
WRITE = 0x80000000


def tool(*names):
    """The first of these programs that exists, with or without an LLVM version suffix."""
    for name in names:
        for candidate in [name] + [f"{name}-{v}" for v in range(21, 13, -1)]:
            if shutil.which(candidate):
                return candidate
    sys.exit(f"{names[0]} not found: install LLVM (clang, lld) and put it on PATH")


def run(cmd):
    p = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if p.returncode:
        sys.exit(f"failed: {' '.join(map(str, cmd))}\n{p.stdout}{p.stderr}")
    return p.stderr.strip()


def align(n, a):
    return (n + a - 1) & ~(a - 1)


def unit_sources(ports):
    out = sorted((ROOT / "hybrid").glob("*.c")) + sorted((ROOT / "ext").glob("*.c"))
    if ports == "all":
        out += sorted((ROOT / "src").rglob("*.c"))
    stems = [s.stem.lower() for s in out]
    twice = sorted({s for s in stems if stems.count(s) > 1})
    if twice:
        sys.exit(f"two sources with one name: {twice}")
    return out


def check_hooks(sources, exe):
    """Every HOOK and HOOK_WRAP address must be a function start, used once, and no port may
    replace a function an extension reaches into (ORIG_SITE). A wrap's moved bytes are checked
    against the exe when iced-x86 is installed."""
    known = functions()
    hooks, wraps, sites = [], [], []
    for src in sources:
        text = src.read_text(encoding="utf-8")
        for m in re.finditer(r"^HOOK\((0x[0-9a-fA-F]+),\s*(\w+)\)", text, re.M):
            hooks.append((int(m[1], 16), m[2], src.name))
        for m in re.finditer(r"^HOOK_WRAP\((\w+),\s*(0x[0-9a-fA-F]+),\s*(\d+),\s*(-?\d+),", text, re.M):
            wraps.append((int(m[2], 16), m[1], src.name, int(m[3]), int(m[4])))
        sites += [(int(m[1], 16), src.name) for m in re.finditer(r"ORIG_SITE\((0x[0-9a-fA-F]+)\)", text)]
    for addr, name, src, *_ in hooks + wraps:
        if addr not in known:
            sys.exit(f"{src}: {name} at {addr:#x} is not a known function start")
    for kind, rows in (("hooked", hooks), ("wrapped", wraps)):
        addrs = [r[0] for r in rows]
        twice = sorted({hex(a) for a in addrs if addrs.count(a) > 1})
        if twice:
            sys.exit(f"{kind} twice: {twice}")
    hooked = {h[0] for h in hooks}
    for site, src in sites:
        inside = [fn for fn, blocks in known.items() if any(start <= site < start + size for start, size in blocks)]
        if not inside:
            sys.exit(f"{src}: ORIG_SITE({site:#x}) is in no known function")
        if hooked & set(inside):
            sys.exit(f"{src}: ORIG_SITE({site:#x}) is inside {inside[0]:#x}, which a port now replaces")
    try:
        from iced_x86 import Decoder, OpKind
    except ImportError:
        if wraps:
            print("note: iced-x86 is not installed, so the wraps' moved instructions were not checked")
        return hooks, wraps
    for addr, name, src, moved, rel_at in wraps:
        pos, rel = 0, -1
        for ins in Decoder(32, exe.bytes(addr, 32), ip=addr):
            if pos >= moved:
                break
            if any(ins.op_kind(i) in (OpKind.NEAR_BRANCH16, OpKind.NEAR_BRANCH32) for i in range(ins.op_count)):
                if ins.len != 5 or rel >= 0:  # only one e8/e9 rel32 can be re-aimed
                    sys.exit(f"{src}: wrap {name} would move a branch it cannot re-aim at {ins.ip:#x}")
                rel = pos + 1
            pos += ins.len
        if pos != moved or moved < 5 or rel != rel_at:
            sys.exit(f"{src}: wrap {name} moves {moved} bytes with rel_at {rel_at}; "
                     f"the exe's instructions end at {pos} with a rel32 at {rel}")
    return hooks, wraps


def compile_all(sources, obj, extra, jobs):
    """Compile what changed: a source newer than its object, any header change, other flags."""
    clang = tool("clang")
    flags = CFLAGS + extra
    stamp = obj / "flags.txt"
    fresh = stamp.exists() and stamp.read_text() == " ".join(flags)
    newest_header = max(h.stat().st_mtime for d in HEADERS for h in d.rglob("*.h"))
    todo = []
    for src in sources:
        o = obj / (src.stem + ".obj")
        if not (fresh and o.exists() and o.stat().st_mtime > max(src.stat().st_mtime, newest_header)):
            todo.append((src, o))
    with cf.ThreadPoolExecutor(jobs) as ex:
        for warning in ex.map(lambda so: run([clang, *flags, "-c", so[0], "-o", so[1]]), todo):
            if warning:
                print(warning)
    stamp.write_text(" ".join(flags))
    return [obj / (src.stem + ".obj") for src in sources], len(todo)


def link(objs, obj, base):
    """The sources' own image, at the address where it will sit in the exe."""
    run([tool("llvm-dlltool"), "-m", "i386", "-k", "-d", ROOT / "hybrid/kernel32.def", "-l", obj / "kernel32.lib"])
    image = obj / "mwrecomp.dll"
    rsp = obj / "link.rsp"
    rsp.write_text("\n".join(f'"{o}"' for o in [*objs, obj / "kernel32.lib"]))
    run([tool("lld-link"), "/dll", "/nodefaultlib", "/entry:mwrecomp_entry", "/machine:x86", "/safeseh:no",
         "/merge:.hooks=.rdata", "/merge:.hookw=.rdata", f"/base:{base:#x}", f"/out:{image}", f"@{rsp}"])
    return PE(image)


def merge(exe, image, out):
    """Write out: exe with image's sections behind its own, both import tables, and image's entry."""
    if exe.base != IMAGE_BASE or exe.entry != ORIG_ENTRY:
        sys.exit(f"{exe.path}: not the exe this was written for (base {exe.base:#x}, entry {exe.entry:#x})")
    delta = image.base - exe.base  # an RVA in image -> the same place as an RVA in the exe
    raw = bytearray(exe.raw)
    raw += bytes(align(len(raw), exe.file_align) - len(raw))
    names = {".text": ".mwtext", ".rdata": ".mwrdata", ".data": ".mwdata"}
    added = []  # (name, rva, virtual size, data, flags)
    for name, rva, vsize, roff, rsize, flags in image.sections:
        if name == ".reloc":  # the image sits where it was linked
            continue
        if name not in names:
            sys.exit(f"{image.path}: unexpected section {name}")
        data = bytearray(image.raw[roff:roff + min(rsize, align(vsize, exe.file_align))])
        if name == ".rdata":
            flags |= WRITE  # the loader fills the import address table in here
        added.append([names[name], rva + delta, vsize, data, flags])

    def patch(rva, value):  # a 32-bit value of the image, by its image RVA
        for _, sec_rva, vsize, data, _ in added:
            if sec_rva <= rva + delta < sec_rva + len(data):
                struct.pack_into("<I", data, rva + delta - sec_rva, value)
                return
        sys.exit(f"{image.path}: import data at {rva:#x} is outside its sections")

    # the image's import descriptors, and the name pointers they lead to, as RVAs of the exe
    table = b"".join(struct.pack("<IIIII", *d) for d in exe.import_descriptors())
    for lookup, stamp, chain, name, iat in image.import_descriptors():
        for thunks in {lookup, iat} - {0}:
            k = 0
            while True:
                entry, = struct.unpack_from("<I", image.image, thunks + 4 * k)
                if not entry:
                    break
                if not entry & 0x80000000:
                    patch(thunks + 4 * k, entry + delta)
                k += 1
        table += struct.pack("<IIIII", lookup and lookup + delta, stamp, chain, name + delta, iat + delta)
    table += bytes(20)
    end = align(added[-1][1] + added[-1][2], exe.section_align)
    added.append([".mwidata", end, len(table), bytearray(table), 0xC0000040])
    behind = align(exe.size_image, exe.section_align)
    if added[0][1] > behind:  # Windows wants the sections one behind the other: fill the gap
        added.insert(0, [".mwpad", behind, added[0][1] - behind, bytearray(), 0xC0000080])

    if exe.table + 40 * (exe.nsec + len(added)) > exe.size_headers:
        sys.exit(f"{exe.path}: no room for {len(added)} more section headers")
    expect = align(exe.size_image, exe.section_align)
    for i, (name, rva, vsize, data, flags) in enumerate(added):
        if rva != expect:
            sys.exit(f"section {name} at {rva:#x} does not follow the one before it ({expect:#x})")
        expect = align(rva + vsize, exe.section_align)
        data += bytes(align(len(data), exe.file_align) - len(data))
        struct.pack_into("<8sIIIIIIHHI", raw, exe.table + 40 * (exe.nsec + i),
                         name.encode(), vsize, rva, len(data), len(raw) if data else 0, 0, 0, 0, 0, flags)
        raw += data
    struct.pack_into("<H", raw, exe.pe + 6, exe.nsec + len(added))
    struct.pack_into("<I", raw, exe.opt + 16, image.entry + delta)
    struct.pack_into("<I", raw, exe.opt + 56, expect)
    struct.pack_into("<II", raw, exe.opt + 96 + 8, added[-1][1], len(table))
    struct.pack_into("<I", raw, exe.opt + 64, 0)  # checksum: not checked for an exe
    out.write_bytes(raw)
    out.chmod(0o755)


def write_cov_blocks(path):
    """Block starts for coverage (MWRECOMP_COV_BLOCKS): u32 count, then sorted u32 addresses."""
    starts = sorted({int(line.split("\t")[0], 16) for line in open(DATA / "blocks.tsv")})
    path.write_bytes(struct.pack(f"<I{len(starts)}I", len(starts), *starts))


def finish(exe, image, out):
    merge(exe, image, out / "mwccps2.exe")
    if (ORIG / "LMGR326B.dll").exists() and not (out / "LMGR326B.dll").exists():
        shutil.copy(ORIG / "LMGR326B.dll", out / "LMGR326B.dll")  # Windows loads the license DLL from here
    write_cov_blocks(out / "cov_blocks.bin")


def build(out=BUILD, extra=(), ports="all", jobs=os.cpu_count(), quiet=False, image_base=IMAGE_AT):
    exe = PE(need_orig())
    obj = out / "obj"
    obj.mkdir(parents=True, exist_ok=True)
    sources = unit_sources(ports)
    hooks, wraps = check_hooks(sources, exe)
    objs, compiled = compile_all(sources, obj, list(extra), jobs)
    finish(exe, link(objs, obj, image_base), out)
    print(f"built {out / 'mwccps2.exe'} with {len(hooks)} hooks and {len(wraps)} wraps from {len(sources)} sources "
          f"({compiled} compiled)")
    if not quiet:
        for addr, name, src, *_ in sorted(hooks + wraps):
            print(f"  {addr:08x} {name:30} {src}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=str(BUILD), help="output directory (default build/)")
    ap.add_argument("--cflags", default="", help="extra clang flags, e.g. for a negative-control build")
    ap.add_argument("--ports", choices=("all", "none"), default="all",
                    help="none: leave out src/, so every function is the original's and only the extensions are added")
    ap.add_argument("--image-base", type=lambda x: int(x, 0), default=IMAGE_AT,
                    help=f"link the hybrid's own image at this address (default {IMAGE_AT:#x})")
    ap.add_argument("--image", help="merge this linked image instead of compiling the sources")
    ap.add_argument("-j", type=int, default=os.cpu_count())
    ap.add_argument("-q", action="store_true", help="do not list the hooks")
    a = ap.parse_args()
    out = Path(a.out).resolve()
    if a.image:
        out.mkdir(parents=True, exist_ok=True)
        finish(PE(need_orig()), PE(a.image), out)
        print(f"merged {a.image} into {out / 'mwccps2.exe'}")
    else:
        build(out, a.cflags.split(), a.ports, a.j, a.q, a.image_base)


if __name__ == "__main__":
    main()
