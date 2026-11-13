"""Check one hand-ported unit: build, full suite, negative control(s), rebuild normal.

usage: python tools/portcheck.py [--no-suite] CONTROL [CONTROL ...]
  CONTROL is the macro suffix: CLFILES runs the suite with -DRECOMP_NEGATIVE_CONTROL_CLFILES.

Prints the hook count, the suite summary (with the names of intermittent cases) and each
control's pass/fail. A control that is caught should fail cases; the normal build is rebuilt
at the end either way.

Targeted controls: the suite run records which hooked functions each case reaches (difftest.py
--hookcov, kept in build/hookcov_map.json). Each control then reruns only the cases that
reach a hook whose code can run the control (tools/ctlreach.py); every other case runs the
control build exactly as the normal one, so it would pass. --full runs every control on the whole
suite, and controls ctlreach.py cannot place always do.

Early stop: a control ends once STOP_AFTER cases fail (the flaky units, FLAKY, run last and do not
count), since that already shows it is caught; only uncaught controls run all their cases.
--all-fails runs them to the end (to get the full fail count).
"""
import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
from common import BUILD, DATA, ORIG_EXE, ROOT  # noqa: E402
import ctlreach  # noqa: E402
PY = sys.executable
CAND = BUILD / "mwccps2.exe"
# the suite run stores the reference compiler's results here; the control runs reuse them
REFCACHE = BUILD / "refcache"
HOOKMAP = BUILD / "hookcov_map.json"
# a control is caught once this many cases fail; units that fail on their own do not count
STOP_AFTER = 10
FLAKY = r"enemy|webnews|audio|pgrider"
MANIFESTS = [str(BUILD / "corpus/imoq/manifest.json"), str(ROOT / "tests/cli/manifest.json")]


def build(cflags=None):
    cmd = [PY, str(ROOT / "tools/build.py")]
    if cflags:
        cmd.append(f"--cflags={cflags}")
    for attempt in range(3):  # a file still held by an exiting compiler process fails a build
        r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
        out = r.stdout + r.stderr
        line = next((l for l in out.splitlines() if l.startswith("built ")), None)
        if not r.returncode and line:
            return line
        if "PermissionError" not in out and "permission denied" not in out.lower():
            break
        time.sleep(5)
    sys.exit(f"build failed ({cflags or 'normal'}):\n{out[-3000:]}")


def suite(extra):
    r = subprocess.run([PY, str(ROOT / "tools/difftest.py"), "--cand", str(CAND), "--manifest", *MANIFESTS,
                        *extra], capture_output=True, text=True, cwd=ROOT)
    out = r.stdout + r.stderr
    summary = next((l for l in out.splitlines() if l.startswith("{") and '"cases"' in l), None)
    report = next((l.split("report:", 1)[1].strip() for l in out.splitlines() if l.startswith("report:")), None)
    if not summary:
        sys.exit(f"suite failed:\n{out[-3000:]}")
    s = json.loads(summary)
    flips = []
    if report and Path(report).exists():
        results = json.load(open(report))["results"]
        flips = [c["src"].rsplit("/", 1)[-1] for c in results if "intermittent" in (c.get("note") or "")]
        if "--hookcov" in extra:
            HOOKMAP.write_text(json.dumps({f"{c['src']}\t{c['flags']}": c.get("hooks") for c in results}),
                               encoding="utf-8")
    return s, flips


def targeted_cases(control, hookmap, files, hooks):
    """The cases that can run the control's code, as an --only file: (path, count, total); None
    means all of them."""
    if hookmap is None:
        return None
    r = ctlreach.reach(control, files, hooks)
    if r is None:
        return None
    if any(v is None for v in hookmap.values()):
        return None  # a case without hook coverage
    keep = sorted(k for k, v in hookmap.items() if r & set(v))
    path = BUILD / f"portcheck_only_{control}.txt"
    path.write_text("".join(k + "\n" for k in keep), encoding="utf-8")
    return path, len(keep), len(hookmap)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("controls", nargs="*")
    ap.add_argument("--no-suite", action="store_true")
    ap.add_argument("--full", action="store_true", help="run every control on the whole suite")
    ap.add_argument("--all-fails", action="store_true",
                    help=f"run each control's cases to the end (default: stop after {STOP_AFTER} real fails)")
    a = ap.parse_args()
    try:
        print(build())
        if not a.no_suite:
            s, flips = suite(["--ref-cache", str(REFCACHE), "--ref-cache-mode", "write", "--hookcov"])
            print(f"suite: pass {s['pass']} fail {s['fail']} of {s['cases']}, intermittent {s['intermittent']} {flips}",
                  flush=True)
        hookmap = None
        if not a.full and HOOKMAP.exists():
            hookmap = json.loads(HOOKMAP.read_text(encoding="utf-8"))
            if a.no_suite:
                print(f"(targeted controls use the hook coverage of an earlier suite run: {HOOKMAP})")
        files, hooks = ctlreach.load()
        for c in a.controls:
            t = targeted_cases(c, hookmap, files, hooks)
            if t and t[1] == 0:
                print(f"control {c}: pass 0 fail 0 (no case reaches its code)", flush=True)
                continue
            build(f"-DRECOMP_NEGATIVE_CONTROL_{c}")
            cache = ["--ref-cache", str(REFCACHE), "--ref-cache-mode", "read"] if REFCACHE.exists() else []
            only = ["--only", str(t[0])] if t else []
            stop = [] if a.all_fails else ["--stop-after", str(STOP_AFTER), "--stop-ignore", FLAKY]
            s, _ = suite(["--keep", "none", "--retries", "0", "--timeout", "20", *cache, *only, *stop])
            scope = f" (targeted: {t[1]} of {t[2]} cases)" if t else " (full suite)"
            if s.get("stopped_early"):
                scope += f", caught: stopped after {s['cases']} of {s['planned']}"
            print(f"control {c}: pass {s['pass']} fail {s['fail']}{scope}", flush=True)
    finally:
        build()


if __name__ == "__main__":
    main()
