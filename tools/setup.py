"""Fetch what the build needs and the repository cannot hold: the original compiler.

  python tools/setup.py

Downloads the compiler as decomp.me publishes it (mwccps2.exe 3.0.1 build 020123 with its license
DLL) into orig/, and on Linux wibo, which runs it. Both are checked against known SHA-256 sums.
To use files you already have, set MWCC_ORIG to the compiler's folder (and WIBO to wibo) instead.
"""
import hashlib
import io
import os
import sys
import tarfile
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import ORIG, ORIG_ARCHIVE_SHA256, ORIG_EXE, ORIG_EXE_SHA256, ORIG_URL, WIBO_SHA256, WIBO_URL  # noqa: E402


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def fetch(url, expected):
    print("downloading", url)
    request = urllib.request.Request(url, headers={"User-Agent": "mwccps2-recomp-setup"})
    data = urllib.request.urlopen(request, timeout=120).read()
    if sha256(data) != expected:
        sys.exit(f"{url}: unexpected contents (sha256 {sha256(data)})")
    return data


def main():
    ORIG.mkdir(parents=True, exist_ok=True)
    if not (ORIG_EXE.exists() and sha256(ORIG_EXE.read_bytes()) == ORIG_EXE_SHA256):
        with tarfile.open(fileobj=io.BytesIO(fetch(ORIG_URL, ORIG_ARCHIVE_SHA256)), mode="r:gz") as archive:
            for member in archive.getmembers():
                name = Path(member.name).name
                if member.isfile() and name in ("mwccps2.exe", "LMGR326B.dll"):
                    (ORIG / name).write_bytes(archive.extractfile(member).read())
        if sha256(ORIG_EXE.read_bytes()) != ORIG_EXE_SHA256:
            sys.exit(f"{ORIG_EXE}: not the expected compiler")
    print("original compiler:", ORIG_EXE)
    if sys.platform != "win32" and not os.environ.get("WIBO"):
        wibo = ORIG / "wibo"
        if not (wibo.exists() and sha256(wibo.read_bytes()) == WIBO_SHA256):
            wibo.write_bytes(fetch(WIBO_URL, WIBO_SHA256))
            wibo.chmod(0o755)
        print("wibo:", wibo)


if __name__ == "__main__":
    main()
