"""Snapshot IMOQ compiler inputs without building or modifying the game checkout."""
import ast
import json
import shlex
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import BUILD

OUT = BUILD / "corpus" / "imoq"


def literal(node, values):
    if isinstance(node, ast.Name):
        return values[node.id]
    if isinstance(node, ast.Dict):
        return {literal(key, values): literal(value, values) for key, value in zip(node.keys, node.values)}
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id == "dict" and not node.args:
        if any(keyword.arg is None for keyword in node.keywords):
            raise ValueError("Expanded compiler settings are unsupported")
        return {keyword.arg: literal(keyword.value, values) for keyword in node.keywords}
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
        return literal(node.left, values) + literal(node.right, values)
    if isinstance(node, ast.Subscript):
        return literal(node.value, values)[literal(node.slice, values)]
    return ast.literal_eval(node)


def settings(table, unit, defaults=None):
    result = dict(defaults or {})
    for key in sorted(table, key=len):
        if unit == key or (key.endswith("/") and unit.startswith(key)):
            result.update(table[key])
    return result


def cases(game):
    values = {}
    wanted = {"BASE_FLAGS", "MWCC_DEFAULT", "MWCC_UNITS", "GCC_UNITS", "NEWLIB"}
    for node in ast.parse((game / "configure.py").read_text(encoding="utf-8")).body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
            name = node.targets[0].id
            if name in wanted:
                values[name] = literal(node.value, values)
    for name in wanted - {"NEWLIB"}:
        if name not in values:
            raise ValueError(f"Missing compiler setting: {name}")
    result = []
    skipped = 0
    for source in sorted((game / "src").rglob("*")):
        if not source.is_file() or source.suffix.lower() not in (".c", ".cpp"):
            continue
        unit = source.relative_to(game / "src").with_suffix("").as_posix()
        compiler = settings(values["MWCC_UNITS"], unit, values["MWCC_DEFAULT"])
        if settings(values["GCC_UNITS"], unit) or compiler["version"] != "3.0.1":
            skipped += 1
            continue
        name = source.relative_to(game).as_posix()
        flags = shlex.split(values["BASE_FLAGS"]) + shlex.split(compiler["flags"])
        if source.suffix.lower() == ".cpp":
            flags += ["-lang", "c++"]
        result.append({"name": "imoq/" + name, "args": [*flags, "-o", "{out}", name],
                       "env": {"MWCIncludes": "include"}})
    if not result:
        raise ValueError("No MWCC 3.0.1 sources found")
    return result, skipped


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: python tools/imoq_corpus.py path/to/IMOQ_Decomp")
    game = Path(sys.argv[1]).resolve()
    entries, skipped = cases(game)
    destination = OUT.resolve()
    if (not destination.is_relative_to(BUILD.resolve()) or game.is_relative_to(destination)
            or destination.is_relative_to(game)):
        sys.exit("Corpus destination overlaps the checkout or is outside build/")
    destination.mkdir(parents=True, exist_ok=True)
    for folder in ("src", "include"):
        target = destination / folder
        if target.resolve().parent != destination:
            sys.exit(f"Corpus folder escapes destination: {target}")
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(game / folder, target)
    manifest = destination / "manifest.json"
    manifest.write_text(json.dumps({"cwd": ".", "cases": entries}, indent=1), encoding="utf-8")
    print(f"{len(entries)} compiler-only cases -> {manifest}; skipped {skipped} other-compiler sources")


if __name__ == "__main__":
    main()
