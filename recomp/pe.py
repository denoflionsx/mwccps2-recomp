"""Checked PE32 input: mapped bytes, sections, imports, exports, relocations, resources."""
import hashlib
import struct
from pathlib import Path

EXECUTABLE = 0x20000000
WRITABLE = 0x80000000


class PEError(ValueError):
    pass


class PE:
    def __init__(self, path, expected_hash=None):
        self.raw = Path(path).read_bytes()
        self.sha256 = hashlib.sha256(self.raw).hexdigest()
        if expected_hash and self.sha256 != expected_hash:
            raise PEError(f"{path}: SHA-256 mismatch")
        if self.raw[:2] != b"MZ":
            raise PEError("missing DOS signature")
        header, = self.unpack(self.raw, "<I", 0x3c)
        if self.unpack(self.raw, "<4s", header)[0] != b"PE\0\0":
            raise PEError("missing PE signature")
        machine, count, _, _, _, optional_size, _ = self.unpack(self.raw, "<HHIIIHH", header + 4)
        optional = header + 24
        if machine != 0x14c or optional_size < 96 or self.unpack(self.raw, "<H", optional)[0] != 0x10b:
            raise PEError("expected x86 PE32")
        entry, = self.unpack(self.raw, "<I", optional + 16)
        self.base, = self.unpack(self.raw, "<I", optional + 28)
        self.size, self.header_size = self.unpack(self.raw, "<II", optional + 56)
        if not self.size or self.base + self.size > 0x100000000 or self.header_size > min(self.size, len(self.raw)):
            raise PEError("invalid image extent")
        directory_count, = self.unpack(self.raw, "<I", optional + 92)
        if directory_count > (optional_size - 96) // 8:
            raise PEError("directories exceed optional header")
        directories = [self.unpack(self.raw, "<II", optional + 96 + index * 8) for index in range(directory_count)]
        for index, (offset, size) in enumerate(directories):
            limit = len(self.raw) if index == 4 else self.size  # certificates use file offsets
            if size and offset + size > limit:
                raise PEError("directory exceeds its container")
        self.directories = directories + [(0, 0)] * (16 - len(directories))
        self.image = bytearray(self.size)
        self.image[:self.header_size] = self.raw[:self.header_size]
        self.sections = []
        for index in range(count):
            _, virtual_size, rva, raw_size, offset, _, _, _, _, flags = self.unpack(
                self.raw, "<8sIIIIIIHHI", optional + optional_size + index * 40)
            size = max(virtual_size, raw_size)
            if rva < self.header_size or rva + size > self.size or offset + raw_size > len(self.raw):
                raise PEError("section exceeds image or file")
            if any(rva < start + length and start < rva + size for start, length, _ in self.sections):
                raise PEError("overlapping sections")
            self.sections.append((rva, size, flags))
            self.image[rva:rva + raw_size] = self.raw[offset:offset + raw_size]
        self.entry = self.base + entry
        if not self.executable(self.entry):
            raise PEError("entry is outside executable sections")
        self.imports = self.read_imports()
        self.exports = self.read_exports()
        self.relocations = self.read_relocations()

    @staticmethod
    def unpack(data, format, offset):
        if offset < 0 or offset + struct.calcsize(format) > len(data):
            raise PEError("structure exceeds its container")
        return struct.unpack_from(format, data, offset)

    def read(self, address, size):
        offset = address - self.base
        if size < 0 or offset < 0 or offset + size > self.size:
            raise PEError(f"read outside image: {address:#x}+{size:#x}")
        return bytes(self.image[offset:offset + size])

    def word(self, address):
        return struct.unpack("<I", self.read(address, 4))[0]

    def executable(self, address, size=1):
        offset = address - self.base
        return any(flags & EXECUTABLE and rva <= offset and offset + size <= rva + length
                   for rva, length, flags in self.sections)

    def string(self, rva):
        end = self.image.find(0, rva) if 0 <= rva < self.size else -1
        if end < 0:
            raise PEError("string outside image")
        return self.image[rva:end].decode("latin1")

    def read_imports(self):
        """Map each import address table slot to "library!symbol"."""
        result = {}
        rva, size = self.directories[1]
        if not size:
            return result
        data = self.read(self.base + rva, size)
        for offset in range(0, size, 20):
            descriptor = self.unpack(data, "<IIIII", offset)
            if not any(descriptor):
                return result
            lookup, _, _, name, slot = descriptor
            if not name or not slot:
                raise PEError("invalid import descriptor")
            library = self.string(name)
            lookup = lookup or slot
            while value := self.word(self.base + lookup):
                self.read(self.base + slot, 4)
                symbol = f"#{value & 0xffff}" if value & 0x80000000 else self.string(value + 2)
                result[self.base + slot] = f"{library}!{symbol}"
                lookup += 4
                slot += 4
        raise PEError("unterminated import directory")

    def read_exports(self):
        """Map each export name, and "#ordinal", to its address."""
        result = {}
        rva, size = self.directories[0]
        if not size:
            return result
        if size < 40:
            raise PEError("truncated export directory")
        _, _, _, _, _, ordinal_base, count, named, functions, names, ordinals = self.unpack(
            self.image, "<IIHHIIIIIII", rva)
        addresses = self.unpack(self.image, f"<{count}I", functions)
        for index, address in enumerate(addresses):
            if rva <= address < rva + size:
                raise PEError("forwarded exports are unsupported")
            if address:
                self.read(self.base + address, 1)
                result[f"#{ordinal_base + index}"] = self.base + address
        name_rvas = self.unpack(self.image, f"<{named}I", names)
        for name_rva, index in zip(name_rvas, self.unpack(self.image, f"<{named}H", ordinals)):
            if index >= count or not addresses[index]:
                raise PEError("invalid export ordinal")
            result[self.string(name_rva)] = self.base + addresses[index]
        return result

    def read_relocations(self):
        """Addresses of the 32-bit words the loader would rebase; each holds an absolute address."""
        result = []
        rva, size = self.directories[5]
        if not size:
            return result
        data = self.read(self.base + rva, size)
        offset = 0
        while offset < size:
            page, block = self.unpack(data, "<II", offset)
            if block < 8 or block % 2 or offset + block > size:
                raise PEError("invalid relocation block")
            for entry in self.unpack(data, f"<{(block - 8) // 2}H", offset + 8):
                if entry >> 12 == 3:
                    address = self.base + page + (entry & 0xfff)
                    self.read(address, 4)
                    result.append(address)
                elif entry >> 12:
                    raise PEError("unsupported relocation type")
            offset += block
        return result

    def resource(self, resource_type, resource_name):
        """Address and size of a resource, preferring the language-neutral entry."""
        root, extent = self.directories[2]
        if not extent:
            return 0, 0
        data = self.read(self.base + root, extent)
        offset = 0
        for depth, wanted in enumerate((resource_type, resource_name, None)):
            named, ids = self.unpack(data, "<HH", offset + 12)
            selected = None
            for index in range(named + ids):
                key, target = self.unpack(data, "<II", offset + 16 + index * 8)
                if key & 0x80000000:
                    length, = self.unpack(data, "<H", key & 0x7fffffff)
                    name, = self.unpack(data, f"<{length * 2}s", (key & 0x7fffffff) + 2)
                    key = name.decode("utf-16le")
                if (key == wanted or wanted is None) and (selected is None or key == 0):
                    selected = target
            if selected is None:
                return 0, 0
            if bool(selected & 0x80000000) != (depth < 2):
                raise PEError("unexpected resource tree depth")
            offset = selected & 0x7fffffff
        rva, size = self.unpack(data, "<II", offset)
        self.read(self.base + rva, size)
        return self.base + rva, size
