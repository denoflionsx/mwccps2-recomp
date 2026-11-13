"""Recompile a PE image into C++: functions.h, part_N.cpp, image.cpp.

config/images.json may pin the image's hash and list hook sites under its name."""
import argparse
import json
import re
from pathlib import Path

from analyze import Program
from emit import Emitter
from pe import PE, PEError, WRITABLE

ROOT = Path(__file__).resolve().parents[1]


def write_changed(path, source):
    """Leave unchanged files alone so the build recompiles only what differs."""
    if not path.exists() or path.read_text() != source:
        path.write_text(source)


def parts(program, emitter, count):
    """Function definitions split into `count` sources of similar instruction counts."""
    entries = sorted(program.functions)
    total = sum(len(program.functions[entry]) for entry in entries)
    sources = [['#include "functions.h"'] for _ in range(count)]
    seen = 0
    for entry in entries:
        sources[min(seen * count // total, count - 1)].append(emitter.function(entry))
        seen += len(program.functions[entry])
    return ["\n\n".join(source) + "\n" for source in sources]


def image_source(image, program, name):
    """The image's bytes, symbols and function table as an `Image` named `<name>_Image`."""
    def symbols(items):
        return [f"    {{{json.dumps(text)}, 0x{address:x}}}," for text, address in items]

    version = image.resource(16, 1)
    if not version[1]:
        version = image.resource(16, "VS_VERSION_INFO")
    lines = ['#include "functions.h"', '#include "guest.h"', ""]
    regions = []
    for index, (rva, size, flags) in enumerate([(0, image.header_size, 0), *image.sections]):
        data = image.read(image.base + rva, size).rstrip(b"\0")
        lines.append(f"static const u8 sRegion{index}[] = {{{','.join(map(str, data)) or '0'}}};")
        regions.append(f"    {{0x{image.base + rva:x}, 0x{size:x}, {str(bool(flags & WRITABLE)).lower()}, "
                       f"sRegion{index}, {len(data)}}},")
    imports = sorted(image.imports.items())
    lines += ["", "static const ImageRegion sRegions[] = {", *regions, "};",
              "static const ImageSymbol sImports[] = {", *symbols((text, slot) for slot, text in imports),
              "    {nullptr, 0}", "};",
              "static const ImageSymbol sExports[] = {", *symbols(sorted(image.exports.items())),
              "    {nullptr, 0}", "};",
              "static const ImageFunction sFunctions[] = {",
              *(f"    {{0x{entry:x}, sub_{entry:x}}}," for entry in sorted(program.functions)), "};",
              f"GuestFunction* {name}_Imports[{len(imports) + 1}];", "",
              f"extern const Image {name}_Image = {{",
              f"    0x{image.base:x}, 0x{image.size:x}, 0x{image.entry:x},",
              f"    sRegions, {len(regions)}, sImports, {name}_Imports, {len(imports)},",
              f"    sExports, {len(image.exports)},",
              f"    sFunctions, {len(program.functions)},",
              f"    0x{version[0]:x}, {version[1]},",
              f"    0x{image.base + image.directories[2][0]:x}, {image.directories[2][1]},",
              "};", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("name", help="prefix of the generated symbols, and key in config/images.json")
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--parts", type=int, default=64)
    arguments = parser.parse_args()
    if arguments.parts < 1:
        parser.error("--parts must be positive")
    if not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", arguments.name):
        parser.error("name must be a C++ identifier")
    configuration = json.loads((ROOT / "config/images.json").read_text()).get(arguments.name, {})
    name = arguments.name.capitalize()
    image = PE(arguments.image, configuration.get("sha256"))
    program = Program(image)
    hooks = {int(address, 16): bytes.fromhex(expected) for address, expected in configuration.get("hooks", {}).items()}
    for address, expected in hooks.items():
        if not expected or address not in program.instructions or image.read(address, len(expected)) != expected:
            raise PEError(f"hook site mismatch at {address:#x}")
    emitter = Emitter(program, hooks, f"{name}_Imports")
    arguments.out.mkdir(parents=True, exist_ok=True)
    for index, source in enumerate(parts(program, emitter, arguments.parts)):
        write_changed(arguments.out / f"part_{index}.cpp", source)
    write_changed(arguments.out / "functions.h", "\n".join([
        "#pragma once", '#include "cpu.h"', "",
        *(f"u32 sub_{entry:x}(u8* __restrict m, u32 esp);" for entry in sorted(program.functions)),
        f"extern GuestFunction* {name}_Imports[];", ""]))
    write_changed(arguments.out / "image.cpp", image_source(image, program, name))
    print(f"{name}: {len(program.instructions)} instructions in {len(program.functions)} functions")
    for form, count in sorted(emitter.unsupported.items()):
        print(f"  trapped: {form} ({count})")


if __name__ == "__main__":
    main()
