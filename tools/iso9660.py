"""Minimal read-only ISO9660 primary-tree reader (2048 or raw Mode 1 sectors).

No mounting, executables, Rock Ridge links or external extractors are involved.
Multi-extent files, interleaving and extended attributes are unsupported.
"""

from pathlib import PurePosixPath
import struct


class IsoImage:
    def __init__(self, path):
        self.file = path.open("rb")
        try:
            self.file.seek(0, 2)
            self.size = self.file.tell()
            for self.sector_size, self.skip in [(2048, 0), (2352, 16)]:
                self.file.seek(16 * self.sector_size + self.skip)
                if self.file.read(7) == b"\x01CD001\x01":
                    break
            else:
                raise ValueError("Unsupported image: ISO9660 primary descriptor not found")
            pvd = self.read(16, 2048)
            if self.both16(pvd, 128) != 2048:
                raise ValueError("Unsupported ISO logical block size")
            self.blocks = self.both32(pvd, 80)
            if self.blocks > self.size // self.sector_size:
                raise ValueError("Truncated ISO volume")
            self.root = self.record(pvd[156:190])
            if not self.root[2] & 2:
                raise ValueError("ISO root is not a directory")
        except Exception:
            self.file.close()
            raise

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.file.close()

    @staticmethod
    def both16(data, at):
        lo = struct.unpack_from("<H", data, at)[0]
        if lo != struct.unpack_from(">H", data, at + 2)[0]:
            raise ValueError("Inconsistent ISO integer")
        return lo

    @staticmethod
    def both32(data, at):
        lo = struct.unpack_from("<I", data, at)[0]
        if lo != struct.unpack_from(">I", data, at + 4)[0]:
            raise ValueError("Inconsistent ISO integer")
        return lo

    def read(self, block, size):
        if block < 0 or size < 0 or size > 64 * 1024 * 1024:
            raise ValueError("ISO extent exceeds supported bounds")
        end = block + (size + 2047) // 2048
        if end > getattr(self, "blocks", self.size // self.sector_size):
            raise ValueError("ISO extent outside volume")
        chunks = []
        for sector in range(block, end):
            self.file.seek(sector * self.sector_size)
            raw = self.file.read(self.sector_size)
            if len(raw) != self.sector_size:
                raise ValueError("Truncated ISO sector")
            if self.skip and (raw[:12] != b"\x00" + b"\xff" * 10 + b"\x00" or raw[15] != 1):
                raise ValueError("Unsupported raw sector: expected CD-ROM Mode 1")
            chunks.append(raw[self.skip:self.skip + min(2048, size)])
            size -= 2048
        return b"".join(chunks)

    def record(self, data):
        if len(data) < 34 or data[0] != len(data) or 33 + data[32] > len(data):
            raise ValueError("Malformed ISO directory record")
        if data[1] or data[26] or data[27] or data[25] & 0x80:
            raise ValueError("Unsupported ISO extent layout")
        if self.both16(data, 28) != 1:
            raise ValueError("Unsupported multi-volume ISO")
        return self.both32(data, 2), self.both32(data, 10), data[25], data[33:33 + data[32]]

    def files(self):
        result = []
        visited = set()
        names = set()

        def walk(directory, parent, depth):
            if depth > 16 or directory[:2] in visited:
                raise ValueError("ISO directory cycle or excessive depth")
            visited.add(directory[:2])
            if directory[1] > 8 * 1024 * 1024:
                raise ValueError("ISO directory too large")
            data = self.read(directory[0], directory[1])
            at = 0
            while at < len(data):
                length = data[at]
                if length == 0:
                    at = (at // 2048 + 1) * 2048
                    continue
                if at + length > len(data) or at % 2048 + length > 2048:
                    raise ValueError("Truncated ISO directory record")
                item = self.record(data[at:at + length])
                at += length
                if item[3] in (b"\x00", b"\x01"):
                    continue
                name = item[3].decode("ascii").split(";", 1)[0].lower().rstrip(".")
                if not name or name in (".", "..") or any(c in name for c in "/\\\x00"):
                    raise ValueError("Unsafe ISO filename")
                path = parent / name
                if path in names or len(names) >= 20000:
                    raise ValueError("Duplicate ISO path or excessive file count")
                names.add(path)
                if item[2] & 2:
                    walk(item, path, depth + 1)
                else:
                    result.append((path, item[0], item[1]))

        walk(self.root, PurePosixPath(), 0)
        return result

    def extract_program(self, target):
        entries = [e for e in self.files() if e[0].parts[0] == "program"]
        if not any(str(e[0]) == "program/rally.exe" for e in entries):
            raise ValueError("Image does not contain program/rally.exe")
        if sum(e[2] for e in entries) > 512 * 1024 * 1024:
            raise ValueError("Program data exceeds extraction limit")
        for path, block, size in entries:
            output = target.joinpath(*path.parts)
            output.parent.mkdir(parents=True, exist_ok=True)
            with output.open("xb") as destination:
                destination.write(self.read(block, size))
        return len(entries)
