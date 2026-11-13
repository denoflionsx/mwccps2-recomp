"""Fetch and verify the pinned compiler, dependencies and reference runner."""
import hashlib
import io
import os
import sys
import tarfile
import tempfile
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import BUILD, IMAGES, ORIG  # noqa: E402

# The original, as published by decomp.me.
ORIGINAL = ("https://github.com/decompme/compilers/releases/download/compilers/mwcps2-3.0.1-020123.tar.gz",
            "72d823d85943c92ca13378e93d56ac1870298ccbfda0ef8a2efb9650692e968c")
WIBO = ("https://github.com/decompals/wibo/releases/download/1.2.0/wibo-x86_64",
        "13f86a2d618f0dbe67179d349625345eabf9b46450295cb4c904e49f6aff85af")
# name: (GitHub repository, revision, archive hash). mimalloc decides where heap blocks land, so
# its revision is part of the output.
SOURCES = {
    "softfloat": ("ucb-bar/berkeley-softfloat-3", "07ebd29db0cfb55723a79fbbffe99103ef38e305",
                  "4b003e4983b53f11919c9375a589199f74b86339e4f746165812f3d9c18bfd78"),
    "mimalloc": ("microsoft/mimalloc", "69c5c5c402bf0414ff4a366697ecbd5c7578dc02",
                 "fd7e9e0dc1c067e1eb2eb9492f730be270ccf89a53d1ca0d63c573a3ab3d2cb9"),
}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def fetch(url, expected):
    print("downloading", url)
    request = urllib.request.Request(url, headers={"User-Agent": "mwccps2-recomp-setup"})
    with urllib.request.urlopen(request, timeout=120) as response:
        data = response.read()
    if sha256(data) != expected:
        sys.exit(f"{url}: unexpected contents (sha256 {sha256(data)})")
    return data


def unpack(data, destination, root):
    """Extract regular files and directories beneath the archive root."""
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
        members = []
        for member in archive.getmembers():
            path = Path(member.name)
            relative = path.relative_to(root) if path.is_relative_to(root) else None
            if relative is None or ".." in relative.parts or not (member.isfile() or member.isdir()):
                raise ValueError(f"unsafe archive member: {member.name}")
            members.append((member, destination / relative))
        for member, path in members:
            if member.isdir():
                path.mkdir(parents=True, exist_ok=True)
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(archive.extractfile(member).read())


def fetch_original(directory):
    """Place the compiler and license library named in config/images.json in `directory`."""
    images = {directory / image["file"]: image["sha256"] for image in IMAGES.values()}

    def current():
        return all(path.exists() and sha256(path.read_bytes()) == expected for path, expected in images.items())

    if not current():
        directory.mkdir(parents=True, exist_ok=True)
        with tarfile.open(fileobj=io.BytesIO(fetch(*ORIGINAL)), mode="r:gz") as archive:
            for member in archive.getmembers():
                path = directory / Path(member.name).name
                if member.isfile() and path in images:
                    path.write_bytes(archive.extractfile(member).read())
        if not current():
            sys.exit(f"{directory}: the archive does not hold the expected images")


def main():
    fetch_original(ORIG)
    if sys.platform != "win32" and not os.environ.get("WIBO"):
        wibo = ORIG / "wibo"
        if not (wibo.exists() and sha256(wibo.read_bytes()) == WIBO[1]):
            wibo.write_bytes(fetch(*WIBO))
            wibo.chmod(0o755)
    for name, (repository, revision, expected) in SOURCES.items():
        destination = BUILD / "_deps" / f"{name}-src"
        if not destination.exists():
            data = fetch(f"https://codeload.github.com/{repository}/tar.gz/{revision}", expected)
            destination.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.TemporaryDirectory(dir=destination.parent) as temporary:
                source = Path(temporary) / "source"
                unpack(data, source, f"{repository.split('/')[1]}-{revision}")
                source.rename(destination)
    print("inputs are in", ORIG, "and", BUILD / "_deps")


if __name__ == "__main__":
    main()
