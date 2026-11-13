"""Compare two compilers' exit status, diagnostics, and output bytes on a corpus.

The reference defaults to the original compiler and the candidate to the reference, so
running without --cand measures how repeatable the reference itself is."""
import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
from common import BUILD, ORIG_EXE, ROOT  # noqa: E402

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
    # Driver and front-end options.
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
SOURCE_SUFFIXES = {".c", ".cpp", ".cp", ".cc"}


def compare_results(reference, candidate):
    """Names of the ways two results differ. Timeouts never match, even each other."""
    differences = [key for key in ("code", "stdout", "stderr") if reference[key] != candidate[key]]
    if "timeout" in (reference["code"], candidate["code"]):
        differences.append("timeout")
    if "obj" in reference and reference["obj"] != candidate["obj"]:
        differences.append("obj")
    if reference["files"].keys() != candidate["files"].keys():
        differences.append("files")
    for name in reference["files"].keys() & candidate["files"].keys():
        if reference["files"][name] != candidate["files"][name]:
            differences.append("file:" + name)
    return sorted(differences)


def corpus_files(paths):
    for path in map(Path, paths):
        yield from [path] if path.is_file() else sorted(p for p in path.rglob("*") if p.suffix in SOURCE_SUFFIXES)


def execute(compiler, command, cwd, timeout, env, outdir=None):
    """Run a compiler; diagnostics are normalized for its own path, the output folder, line
    endings, and the address in its crash report."""
    start = time.perf_counter()
    try:
        process = subprocess.run(command, cwd=cwd, capture_output=True, timeout=timeout, env=env)
        code, out, error = process.returncode, process.stdout, process.stderr
    except subprocess.TimeoutExpired:
        code, out, error = "timeout", b"", b""

    def normalize(text):
        text = text.replace(str(compiler).encode(), b"CC").replace(b"\r\n", b"\n")
        if outdir is not None:
            text = text.replace(str(outdir).encode(), b"OUT")
        return re.sub(rb"(At address: +)[0-9a-fA-F]{8}", rb"\1XXXXXXXX", text)

    return {"code": code, "stdout": normalize(out), "stderr": normalize(error), "time": time.perf_counter() - start}


def run_one(compiler, workdir, source, flags, timeout, inputs):
    """Compile one source in `workdir`, which holds `inputs`; everything else there is output."""
    for path in workdir.iterdir():
        if path.name not in inputs:
            path.unlink()
    command = [*common.launch(compiler), str(compiler), *BASE, *flags, source, "-o", "out.o"]
    result = execute(compiler, command, workdir, timeout, None)
    obj = workdir / "out.o"
    files = {path.name: path.read_bytes() for path in sorted(workdir.iterdir())
             if path.name not in inputs and path.name != "out.o"}
    return dict(result, obj=obj.read_bytes() if obj.exists() else None, files=files)


def run_case(reference, candidate, source, flagname, timeout, keepdir):
    with tempfile.TemporaryDirectory(prefix="mwdt_") as temporary:
        work = Path(temporary)
        # Copy the source and its non-source siblings, so local includes resolve.
        inputs = set()
        for sibling in source.parent.iterdir():
            if sibling.is_file() and (sibling == source or sibling.suffix not in SOURCE_SUFFIXES):
                shutil.copy(sibling, work / sibling.name)
                inputs.add(sibling.name)
        results = [run_one(compiler, work, source.name, FLAGSETS[flagname], timeout, inputs)
                   for compiler in (reference, candidate)]
    name = str(source.relative_to(ROOT) if source.is_relative_to(ROOT) else source)
    return case_result(name, flagname, *results, keepdir)


def run_command(compiler, cwd, arguments, outdir, timeout, env):
    """Run a manifest command; {out} in its arguments is its output folder."""
    shutil.rmtree(outdir, ignore_errors=True)
    outdir.mkdir(parents=True)
    command = [*common.launch(compiler), str(compiler), *(a.replace("{out}", str(outdir)) for a in arguments)]
    result = execute(compiler, command, cwd, timeout, env, outdir)
    files = {path.relative_to(outdir).as_posix(): path.read_bytes()
             for path in sorted(outdir.rglob("*")) if path.is_file()}
    return dict(result, files=files)


def run_manifest_case(reference, candidate, base, case, timeout, keepdir):
    env = dict(os.environ, **case.get("env", {}))
    with tempfile.TemporaryDirectory(prefix="mwdt_") as temporary:
        results = [run_command(compiler, base, case["args"], Path(temporary) / "out", timeout, env)
                   for compiler in (reference, candidate)]
    return case_result(case["name"], "manifest", *results, keepdir)


def case_result(name, flags, reference, candidate, keepdir):
    """A report entry; the outputs of a failing case are kept under `keepdir`."""
    differences = compare_results(reference, candidate)
    if differences and keepdir:
        directory = keepdir / re.sub(r"[^\w.-]", "_", name + "." + flags)
        directory.mkdir(parents=True, exist_ok=True)
        for tag, result in (("ref", reference), ("cand", candidate)):
            if result.get("obj") is not None:
                (directory / f"{tag}.o").write_bytes(result["obj"])
            for filename, data in result["files"].items():
                (directory / f"{tag}.{filename.replace('/', '_')}").write_bytes(data)
            (directory / f"{tag}.txt").write_bytes(
                b"exit: %s\n--- stdout\n%s\n--- stderr\n%s" %
                (str(result["code"]).encode(), result["stdout"], result["stderr"]))
    obj = reference.get("obj")
    return {"src": name, "flags": flags, "ok": not differences, "diff": differences, "note": None,
            "ref_code": reference["code"], "obj_sha1": hashlib.sha1(obj).hexdigest()[:12] if obj else None,
            "t_ref": round(reference["time"], 3), "t_cand": round(candidate["time"], 3)}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ref", default=str(ORIG_EXE))
    parser.add_argument("--cand", default=str(ORIG_EXE))
    parser.add_argument("--corpus", nargs="*", default=[],
                        help="source files or folders, each compiled under every flag set")
    parser.add_argument("--flags", default=",".join(FLAGSETS), help="comma list of: " + ",".join(FLAGSETS))
    parser.add_argument("--manifest", nargs="*", default=[], help="manifests of full command lines")
    parser.add_argument("--only", help="file of 'name<TAB>flags' lines selecting cases, as named in report.json")
    parser.add_argument("-j", type=int, default=os.cpu_count())
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--retries", type=int, default=3,
                        help="reruns of a failed case, to label it intermittent; it still fails")
    parser.add_argument("--run", default=time.strftime("%Y%m%d-%H%M%S"), help="report folder under build/difftest/")
    arguments = parser.parse_args()
    if arguments.j is not None and arguments.j < 1:
        parser.error("-j must be positive")
    if arguments.timeout <= 0 or arguments.retries < 0:
        parser.error("timeout must be positive and retries nonnegative")
    flags = arguments.flags.split(",")
    if any(name not in FLAGSETS for name in flags):
        parser.error("unknown flag set")
    if not arguments.corpus and not arguments.manifest:
        parser.error("give --corpus or --manifest")

    reference, candidate = Path(arguments.ref).resolve(), Path(arguments.cand).resolve()
    outdir = BUILD / "difftest" / arguments.run
    keepdir = outdir / "fails"
    jobs = []
    for path in arguments.corpus:
        if not Path(path).exists():
            parser.error(f"corpus path does not exist: {path}")
    for source in corpus_files(arguments.corpus):
        jobs += [(run_case, (source, name)) for name in flags]
    for manifest in arguments.manifest:
        manifest = Path(manifest).resolve()
        contents = json.loads(manifest.read_text(encoding="utf-8"))
        jobs += [(run_manifest_case, (manifest.parent / contents.get("cwd", "."), case)) for case in contents["cases"]]
    if arguments.only:
        wanted = {tuple(line.split("\t")) for line in Path(arguments.only).read_text(encoding="utf-8").splitlines()}

        def selected(function, parameters):
            if function is run_manifest_case:
                return (parameters[1]["name"], "manifest") in wanted
            source, flags = parameters
            return (str(source.relative_to(ROOT) if source.is_relative_to(ROOT) else source), flags) in wanted
        jobs = [job for job in jobs if selected(*job)]
    if not jobs:
        parser.error("no test cases selected")
    outdir.mkdir(parents=True, exist_ok=True)
    print(f"{len(jobs)} cases, ref={reference.name}, cand={candidate}", flush=True)

    start = time.perf_counter()
    with concurrent.futures.ThreadPoolExecutor(arguments.j) as executor:
        futures = [executor.submit(function, reference, candidate, *parameters, arguments.timeout, keepdir)
                   for function, parameters in jobs]
        results = [future.result() for future in futures]
    for (function, parameters), result in zip(jobs, results):
        if result["ok"]:
            continue
        print(f"DIFF {result['src']} [{result['flags']}]: {','.join(result['diff'])}", flush=True)
        if any(function(reference, candidate, *parameters, arguments.timeout, None)["ok"]
               for _ in range(arguments.retries)):
            result["note"] = "intermittent:" + ",".join(result["diff"])
            print("  passed on a retry: intermittent", flush=True)
    results.sort(key=lambda result: (result["src"], result["flags"]))
    failed = sum(not result["ok"] for result in results)
    codes = [result["ref_code"] for result in results]
    summary = {"ref": str(reference), "cand": str(candidate), "cases": len(results), "pass": len(results) - failed,
               "fail": failed, "intermittent": sum(result["note"] is not None for result in results),
               "ref_exit_codes": {str(code): codes.count(code) for code in set(codes)},
               "seconds": round(time.perf_counter() - start, 1)}
    report = json.dumps({"summary": summary, "results": results}, indent=1)
    (outdir / "report.json").write_text(report, encoding="utf-8")
    print(json.dumps(summary))
    print("report:", outdir / "report.json")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
