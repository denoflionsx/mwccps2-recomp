"""Differential test: run two mwccps2 builds on a corpus and compare everything they produce.

A case is (source file, flag set). Both compilers run in the same scratch directory on the same
relative file name, so paths written into the output (DWARF, .file) are identical. Compared:
exit code, stdout, stderr (compiler path replaced by "CC") and the object file bytes.

usage:
  python tools/recomp/difftest.py                       # original vs itself (determinism check)
  python tools/recomp/difftest.py --cand path/to/mwccps2.exe
  python tools/recomp/difftest.py --corpus probes/src --flags O0,O4p -j 8 --keep fails
  python tools/recomp/difftest.py --cand build/recomp/hybrid/mwccps2.exe --cov   # + block coverage

Flag sets are named in FLAGSETS; pass several with commas. Results go to
build/recomp/difftest/<run>/report.json; failing cases keep their outputs next to it.
"""

import argparse
import concurrent.futures as cf
import hashlib
import json
import os
import pickle
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REF = ROOT / "recomp_goal/mwcps2-3.0.1-020123/mwccps2.exe"

BASE = ["-c", "-nostdinc", "-nosyspath", "-I."]
FLAGSETS = {
    "O0": ["-O0"],
    "O1": ["-O1"],
    "O2": ["-O2"],
    "O3": ["-O3"],
    "O4": ["-O4"],
    "O4p": ["-O4,p"],
    "O4s": ["-O4,s"],
    "O4pg": ["-O4,p", "-g"],
    # .hack//Infection's flags
    "imoq": ["-O4,p", "-g", "-Cpp_exceptions", "off", "-RTTI", "off", "-str", "readonly"],
    # driver modes and front-end options (reach code the codegen flag sets don't)
    "E": ["-E"],
    "EP": ["-EP"],
    "M": ["-M"],
    "make": ["-make"],
    "strict": ["-O2", "-ansi", "strict", "-w", "all"],
    "wall": ["-O4,p", "-w", "all"],
    "uchar": ["-O4,p", "-char", "unsigned", "-inline", "all,deferred", "-enum", "min"],
    "fpoff": ["-O2", "-fp", "off"],
    "sdata0": ["-O2", "-sdatathreshold", "0"],
    "cxx": ["-O2", "-lang", "c++"],
    "optdump": ["-O4,p", "-opt", "display"],
}
CORE_FLAGS = "O0,O2,O4p,O4pg,imoq"
DEFAULT_FLAGS = ",".join(FLAGSETS)
SOURCE_EXT = {".c", ".cpp", ".cp", ".cc"}


def corpus_files(dirs):
    for d in dirs:
        d = Path(d)
        files = [d] if d.is_file() else sorted(p for p in d.rglob("*") if p.suffix in SOURCE_EXT)
        yield from files


def run_one(cc, workdir, src_name, flags, timeout, env=None, inputs=()):
    obj = workdir / "out.o"
    # start from the inputs only: drop whatever the previous run left behind
    for f in workdir.iterdir():
        if f.name not in inputs:
            f.unlink()
    cmd = [str(cc), *BASE, *flags, src_name, "-o", "out.o"]
    t = time.perf_counter()
    try:
        p = subprocess.run(cmd, cwd=workdir, capture_output=True, timeout=timeout, env=env)
        code, out, err = p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        code, out, err = "timeout", b"", b""
    dt = time.perf_counter() - t
    def norm(b):
        b = b.replace(str(cc).encode(), b"CC").replace(b"\r\n", b"\n")
        # the crash handler prints the faulting address, which varies run to run (and between builds)
        return re.sub(rb"(At address: +)[0-9a-fA-F]{8}", rb"\1XXXXXXXX", b)

    data = obj.read_bytes() if obj.exists() else None
    # any other file the compiler wrote (dependency files, listings, ...); cov.bin is ours
    extra = {f.name: f.read_bytes() for f in sorted(workdir.iterdir())
             if f.name not in inputs and f.name not in ("out.o", "cov.bin", "hookcov.bin")}
    return {"code": code, "stdout": norm(out), "stderr": norm(err), "obj": data, "files": extra, "time": dt}


def _dies(data):
    """DWARF 1 entries of one .debug section, with address and reference values masked."""
    out, i = [], 0
    while i + 4 <= len(data):
        n = int.from_bytes(data[i : i + 4], "little")
        if n < 8:  # null / padding entry
            out.append(data[i : i + max(n, 4)])
            i += max(n, 4)
            continue
        e = bytearray(data[i : i + n])
        j = 6
        try:
            while j + 2 <= n:
                attr = int.from_bytes(e[j : j + 2], "little")
                form = attr & 0xF
                j += 2
                if form in (1, 2):  # ADDR, REF
                    e[j : j + 4] = b"\0\0\0\0"
                    j += 4
                elif form == 3:
                    blen = int.from_bytes(e[j : j + 2], "little")
                    blk = range(j + 2, j + 2 + blen)
                    if attr == 0x0083 and blen >= 4:  # mod_u_d_type: modifiers, then a ref
                        e[blk.stop - 4 : blk.stop] = b"\0\0\0\0"
                    elif attr == 0x00A3:  # subscr_data: FMT_ET + user_def_type ref
                        for k in blk:
                            if e[k : k + 3] == b"\x08\x72\x00":
                                e[k + 3 : k + 7] = b"\0\0\0\0"
                            elif e[k : k + 3] == b"\x08\x83\x00":
                                end = k + 5 + int.from_bytes(e[k + 3 : k + 5], "little")
                                e[end - 4 : end] = b"\0\0\0\0"
                    j = blk.stop
                elif form == 4:
                    j += 4 + int.from_bytes(e[j : j + 4], "little")
                elif form == 5:
                    j += 2
                elif form == 6:
                    j += 4
                elif form == 7:
                    j += 8
                elif form == 8:
                    j = e.index(0, j) + 1
                else:
                    break
        except ValueError:
            pass
        # anonymous types are numbered in a run-dependent order too
        # (and their digit count changes the entry length, so leave the length out)
        out.append(re.sub(rb"@anon\d+\0", b"@anon\0", bytes(e[4:])))
        i += n
    return sorted(out)


def same_modulo_debug_order(a, b):
    """True if two objects differ only in the order of DWARF entries (and the relocations to them)."""
    import io

    from elftools.elf.elffile import ELFFile

    try:
        ea, eb = ELFFile(io.BytesIO(a)), ELFFile(io.BytesIO(b))
        sa, sb = list(ea.iter_sections()), list(eb.iter_sections())
        if len(sa) != len(sb):
            return False
        for x, y in zip(sa, sb):
            if x.name != y.name or (x.name != ".debug" and x.header.sh_size != y.header.sh_size):
                return False
            dx, dy = x.data(), y.data()
            if dx == dy:
                continue
            if x.name == ".debug":
                if _dies(dx) != _dies(dy):
                    return False
            elif x.name == ".rel.debug":
                # same relocations (type, symbol), whatever entry they patch
                rx = sorted(dx[k + 4 : k + 8] for k in range(0, len(dx), 8))
                ry = sorted(dy[k + 4 : k + 8] for k in range(0, len(dy), 8))
                if rx != ry:
                    return False
            else:
                return False
        return True
    except Exception:
        return False


# Optional cache of the reference compiler's results (see --ref-cache): (directory, mode).
REFCACHE = None


def _retarget(v, name):
    """v with every temporary directory name (mwdt_ + 8 characters, as in object debug info and
    dependency files) replaced by name, the current case's one (the same length)."""
    if isinstance(v, bytes):
        return re.sub(rb"mwdt_[A-Za-z0-9_]{8}", name.encode(), v)
    if isinstance(v, str):
        return re.sub(r"mwdt_[A-Za-z0-9_]{8}", name, v)
    if isinstance(v, dict):
        return {_retarget(k, name): _retarget(x, name) for k, x in v.items()}
    return v


def ref_result(key, compute, tmpname):
    """The reference result for a case: computed, or read from / written to the cache. A cached
    result was made in another temporary directory; tmpname is this case's."""
    if not REFCACHE:
        return compute()
    d, mode = REFCACHE
    p = d / (hashlib.sha1(key.encode()).hexdigest() + ".pkl")
    if mode == "read" and p.exists():
        return _retarget(pickle.loads(zlib.decompress(p.read_bytes())), tmpname)
    r = compute()
    if mode == "write":
        p.write_bytes(zlib.compress(pickle.dumps(r)))
    return r


# --hookcov: record which hooked functions each candidate run reaches (report.json "hooks")
HOOKCOV = False


def read_hookcov(path):
    """The original addresses of the hooks that ran, from an MWRECOMP_HOOKCOV file."""
    if not path.exists():
        return None
    d = path.read_bytes()
    return sorted("%x" % v for v in struct.unpack("<%dI" % (len(d) // 4), d[: len(d) // 4 * 4]) if v)


def run_case(ref, cand, src, flagname, timeout, keepdir, covdir=None):
    flags = FLAGSETS[flagname]
    with tempfile.TemporaryDirectory(prefix="mwdt_") as tmp:
        work = Path(tmp)
        # copy the source and its siblings' headers so local includes resolve
        inputs = set()
        for sib in src.parent.iterdir():
            if sib.is_file() and (sib == src or sib.suffix not in SOURCE_EXT):
                shutil.copy(sib, work / sib.name)
                inputs.add(sib.name)
        a = ref_result(f"{src}|{flagname}", lambda: run_one(ref, work, src.name, flags, timeout, inputs=inputs), work.name)
        env = None
        if covdir:
            env = dict(os.environ, MWRECOMP_COV=str(work / "cov.bin"),
                       MWRECOMP_COV_BLOCKS=str(cand.parent / "cov_blocks.bin"))
        if HOOKCOV:
            env = dict(env or os.environ, MWRECOMP_HOOKCOV=str(work / "hookcov.bin"))
        b = run_one(cand, work, src.name, flags, timeout, env, inputs)
        cov = (work / "cov.bin").read_bytes() if covdir and (work / "cov.bin").exists() else None
        hooks = read_hookcov(work / "hookcov.bin") if HOOKCOV else None
    if cov is not None:
        # one byte per block: 1 = ran, 2 = armed but not run, 0 = not armed (under a hook)
        (covdir / f"{src.parent.name}.{src.name}.{flagname}.cov").write_bytes(zlib.compress(cov))
    diffs = [k for k in ("code", "stdout", "stderr", "obj", "files") if a[k] != b[k]]
    note = None
    if diffs == ["obj"] and a["obj"] and b["obj"] and same_modulo_debug_order(a["obj"], b["obj"]):
        # the original itself varies the DWARF entry order from run to run (heap addresses)
        diffs, note = [], "debug-order"
    res = {
        "src": str(src.relative_to(ROOT) if src.is_relative_to(ROOT) else src),
        "flags": flagname,
        "ok": not diffs,
        "diff": diffs,
        "note": note,
        "ref_code": a["code"],
        "obj_sha1": hashlib.sha1(a["obj"]).hexdigest()[:12] if a["obj"] else None,
        "t_ref": round(a["time"], 3),
        "t_cand": round(b["time"], 3),
        "blocks_run": cov.count(1) if cov is not None else None,
    }
    if hooks is not None:
        res["hooks"] = hooks
    if diffs and keepdir:
        d = keepdir / f"{src.parent.name}.{src.name}.{flagname}"
        d.mkdir(parents=True, exist_ok=True)
        for tag, r in (("ref", a), ("cand", b)):
            if r["obj"]:
                (d / f"{tag}.o").write_bytes(r["obj"])
            for name, data in r["files"].items():
                (d / f"{tag}.{name}").write_bytes(data)
            (d / f"{tag}.txt").write_bytes(
                b"exit: %s\n--- stdout\n%s\n--- stderr\n%s" % (str(r["code"]).encode(), r["stdout"], r["stderr"])
            )
    return res


def run_cmd(cc, cwd, args, outdir, timeout, env):
    """Run cc with args ({out} = outdir) in cwd; return exit code, output and every file in outdir."""
    shutil.rmtree(outdir, ignore_errors=True)
    outdir.mkdir(parents=True)
    cmd = [str(cc), *(a.replace("{out}", str(outdir)) for a in args)]
    t = time.perf_counter()
    try:
        p = subprocess.run(cmd, cwd=cwd, capture_output=True, timeout=timeout, env=env)
        code, out, err = p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        code, out, err = "timeout", b"", b""
    dt = time.perf_counter() - t

    def norm(b):
        b = b.replace(str(cc).encode(), b"CC").replace(str(outdir).encode(), b"OUT").replace(b"\r\n", b"\n")
        return re.sub(rb"(At address: +)[0-9a-fA-F]{8}", rb"\1XXXXXXXX", b)

    files = {f.relative_to(outdir).as_posix(): f.read_bytes() for f in sorted(outdir.rglob("*")) if f.is_file()}
    return {"code": code, "stdout": norm(out), "stderr": norm(err), "files": files, "time": dt}


def run_manifest_case(ref, cand, base, case, timeout, keepdir, covdir=None):
    """One case of a manifest (see tools/recomp/imoq_corpus.py): a full command line run in base."""
    env = dict(os.environ, **case.get("env", {}))
    with tempfile.TemporaryDirectory(prefix="mwdt_") as tmp:
        tmp = Path(tmp)
        a = ref_result("manifest|" + case["name"], lambda: run_cmd(ref, base, case["args"], tmp / "out", timeout, env), tmp.name)
        cenv = env
        if covdir:
            cenv = dict(env, MWRECOMP_COV=str(tmp / "cov.bin"), MWRECOMP_COV_BLOCKS=str(cand.parent / "cov_blocks.bin"))
        if HOOKCOV:
            cenv = dict(cenv, MWRECOMP_HOOKCOV=str(tmp / "hookcov.bin"))
        b = run_cmd(cand, base, case["args"], tmp / "out", timeout, cenv)
        cov = (tmp / "cov.bin").read_bytes() if covdir and (tmp / "cov.bin").exists() else None
        hooks = read_hookcov(tmp / "hookcov.bin") if HOOKCOV else None
    diffs = [k for k in ("code", "stdout", "stderr") if a[k] != b[k]]
    note = None
    if a["files"].keys() != b["files"].keys():
        diffs.append("files")
    else:
        for name in a["files"]:
            x, y = a["files"][name], b["files"][name]
            if x == y:
                continue
            if name.endswith(".o") and same_modulo_debug_order(x, y):
                note = "debug-order"
            else:
                diffs.append("file:" + name)
    safe = re.sub(r"[^\w.-]", "_", case["name"])
    if cov is not None:
        (covdir / f"{safe}.cov").write_bytes(zlib.compress(cov))
    if diffs and keepdir:
        d = keepdir / safe
        d.mkdir(parents=True, exist_ok=True)
        for tag, r in (("ref", a), ("cand", b)):
            for name, data in r["files"].items():
                (d / f"{tag}.{name.replace('/', '_')}").write_bytes(data)
            (d / f"{tag}.txt").write_bytes(
                b"exit: %s\n--- stdout\n%s\n--- stderr\n%s" % (str(r["code"]).encode(), r["stdout"], r["stderr"])
            )
    res = {
        "src": case["name"],
        "flags": "manifest",
        "ok": not diffs,
        "diff": diffs,
        "note": note,
        "ref_code": a["code"],
        "obj_sha1": None,
        "t_ref": round(a["time"], 3),
        "t_cand": round(b["time"], 3),
        "blocks_run": cov.count(1) if cov is not None else None,
    }
    if hooks is not None:
        res["hooks"] = hooks
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default=str(REF))
    ap.add_argument("--cand", default=str(REF), help="candidate compiler (default: the original again)")
    ap.add_argument("--corpus", nargs="+", default=[str(ROOT / "probes/src"), str(ROOT / "tests/recomp")])
    ap.add_argument("--flags", default=DEFAULT_FLAGS, help="comma list of: " + ",".join(FLAGSETS))
    ap.add_argument("-j", type=int, default=os.cpu_count())
    ap.add_argument("--timeout", type=float, default=60)
    ap.add_argument("--run", default=time.strftime("%Y%m%d-%H%M%S"))
    ap.add_argument("--keep", choices=("fails", "none"), default="fails")
    ap.add_argument("--manifest", nargs="*", default=[],
                    help="extra cases as full command lines (e.g. build/recomp/corpus/imoq/manifest.json)")
    ap.add_argument("--no-files", action="store_true", help="skip the single-file corpus (only manifests)")
    ap.add_argument("--only", help="file of 'src<TAB>flags' lines: run only those cases")
    ap.add_argument("--retries", type=int, default=3,
                    help="rerun a failing case this many times; passing on a retry = intermittent (0 = off)")
    ap.add_argument("--cov", action="store_true",
                    help="record block coverage of the candidate (a hybrid build); see tools/recomp/covreport.py")
    ap.add_argument("--stop-after", type=int, default=0,
                    help="stop once this many cases fail (not counting --stop-ignore ones): enough for a control")
    ap.add_argument("--stop-ignore", help="regex of case names that fail on their own (run last, not counted)")
    ap.add_argument("--hookcov", action="store_true",
                    help="record which hooked functions each case reaches (a hybrid candidate); report.json \"hooks\"")
    ap.add_argument("--ref-cache", help="directory caching the reference compiler's results")
    ap.add_argument("--ref-cache-mode", choices=("write", "read"), default="read",
                    help="write: run the reference and store; read: reuse stored results (run when missing)")
    a = ap.parse_args()
    global REFCACHE, HOOKCOV
    HOOKCOV = a.hookcov
    if a.ref_cache:
        Path(a.ref_cache).mkdir(parents=True, exist_ok=True)
        REFCACHE = (Path(a.ref_cache), a.ref_cache_mode)

    ref, cand = Path(a.ref).resolve(), Path(a.cand).resolve()
    outdir = ROOT / "build/recomp/difftest" / a.run
    outdir.mkdir(parents=True, exist_ok=True)
    keepdir = outdir / "fails" if a.keep == "fails" else None
    covdir = None
    if a.cov:
        if not (cand.parent / "cov_blocks.bin").exists():
            sys.exit("--cov needs a hybrid candidate (cov_blocks.bin next to it): run build_hybrid.py")
        covdir = outdir / "cov"
        covdir.mkdir(exist_ok=True)
    cases = [] if a.no_files else [
        (s, f) for s in corpus_files(p for p in a.corpus if Path(p).exists()) for f in a.flags.split(",")]
    mcases = []
    for mf in a.manifest:
        mf = Path(mf).resolve()
        m = json.loads(mf.read_text(encoding="utf-8"))
        mcases += [(mf.parent / m.get("cwd", "."), c) for c in m["cases"]]
    if a.only:
        # keep the cases named in a file (one "src<TAB>flags" per line, as in report.json)
        keep = {tuple(l.rstrip("\n").split("\t")) for l in open(a.only, encoding="utf-8") if l.strip()}
        cases = [(s, f) for s, f in cases
                 if (str(s.relative_to(ROOT) if s.is_relative_to(ROOT) else s), f) in keep]
        mcases = [(b, c) for b, c in mcases if (c["name"], "manifest") in keep]
    print(f"{len(cases) + len(mcases)} cases, ref={ref.name}, cand={cand}", flush=True)

    results = []
    t0 = time.perf_counter()
    jobs = {}
    flaky = re.compile(a.stop_ignore) if a.stop_ignore else None
    if a.stop_after:
        # units that fail on their own go last, so the first failures are real ones
        def late(name):
            return bool(flaky and flaky.search(name))
        cases.sort(key=lambda sf: late(str(sf[0])))
        mcases.sort(key=lambda bc: late(bc[1]["name"]))
    stopped = False
    real_fails = 0
    with cf.ThreadPoolExecutor(a.j) as ex:
        for s, f in cases:
            jobs[ex.submit(run_case, ref, cand, s, f, a.timeout, keepdir, covdir)] = (run_case, (s, f))
        for base, c in mcases:
            jobs[ex.submit(run_manifest_case, ref, cand, base, c, a.timeout, keepdir, covdir)] = (
                run_manifest_case, (base, c))
        for fut in cf.as_completed(jobs):
            if fut.cancelled():
                continue
            r = fut.result()
            r["job"] = jobs[fut]
            results.append(r)
            if not r["ok"]:
                print(f"DIFF {r['src']} [{r['flags']}]: {','.join(r['diff'])}", flush=True)
                if not (flaky and flaky.search(r["src"])):
                    real_fails += 1
                if a.stop_after and real_fails >= a.stop_after and not stopped:
                    stopped = True  # enough to call it caught: drop the cases not started yet
                    for j in jobs:
                        j.cancel()
    # The hybrid occasionally takes another path on a few units even with no hooks (see
    # docs/RECOMP.md, "address-dependent output"). Rerun each failure: one that passes on a retry
    # is reported as intermittent, apart from the failures that repeat.
    for r in results:
        fn, args = r.pop("job")
        if r["ok"] or not a.retries:
            continue
        for _ in range(a.retries):
            if fn(ref, cand, *args, a.timeout, None, None)["ok"]:
                r["ok"], r["note"] = True, "intermittent:" + ",".join(r["diff"])
                print(f"  {r['src']} [{r['flags']}] passed on a retry: intermittent", flush=True)
                break
    results.sort(key=lambda r: (r["src"], r["flags"]))
    bad = [r for r in results if not r["ok"]]
    summary = {
        "ref": str(ref),
        "cand": str(cand),
        "cases": len(results),
        "pass": len(results) - len(bad),
        "fail": len(bad),
        "pass_debug_order_only": sum(r["note"] == "debug-order" for r in results),
        "intermittent": sum(str(r["note"]).startswith("intermittent") for r in results),
        "ref_exit_codes": {str(c): sum(r["ref_code"] == c for r in results) for c in {r["ref_code"] for r in results}},
        "seconds": round(time.perf_counter() - t0, 1),
        "coverage_dir": str(covdir) if covdir else None,
        "stopped_early": stopped,
        "planned": len(jobs),
    }
    (outdir / "report.json").write_text(json.dumps({"summary": summary, "results": results}, indent=1), encoding="utf-8")
    print(json.dumps(summary))
    print("report:", outdir / "report.json")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
