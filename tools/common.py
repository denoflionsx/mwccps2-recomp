"""Shared paths and compiler launch commands."""
import json
import os
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
ORIG = Path(os.environ.get("MWCC_ORIG", ROOT / "orig"))
ORIG_EXE = ORIG / "mwccps2.exe"
IMAGES = json.loads((ROOT / "config/images.json").read_text())


def launch(executable):
    """The command prefix that runs `executable`: nothing for a native binary, wibo for a
    Windows image on another host."""
    if sys.platform == "win32":
        return []
    with Path(executable).open("rb") as stream:
        if stream.read(2) != b"MZ":
            return []
    wibo = os.environ.get("WIBO") or (str(ORIG / "wibo") if (ORIG / "wibo").exists() else shutil.which("wibo"))
    if not wibo:
        sys.exit("wibo is needed to run the compiler here: run tools/setup.py, or set WIBO")
    return [wibo]
