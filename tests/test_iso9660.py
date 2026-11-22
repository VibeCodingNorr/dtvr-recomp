"""Synthetic ISO fixtures: no original game data required."""

from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from iso9660 import IsoImage


def both16(value):
    return struct.pack("<H", value) + struct.pack(">H", value)


def both32(value):
    return struct.pack("<I", value) + struct.pack(">I", value)


def record(name, block, size, directory=False):
    length = 33 + len(name) + (len(name) % 2 == 0)
    data = bytearray(length)
    data[0] = length
    data[2:10] = both32(block)
    data[10:18] = both32(size)
    data[25] = 2 if directory else 0
    data[28:32] = both16(1)
    data[32] = len(name)
    data[33:33 + len(name)] = name
    return data


def fixture(filename=b"RALLY.EXE;1", raw=False):
    image = bytearray(24 * 2048)
    pvd = 16 * 2048
    image[pvd:pvd + 7] = b"\x01CD001\x01"
    image[pvd + 80:pvd + 88] = both32(24)
    image[pvd + 128:pvd + 132] = both16(2048)
    image[pvd + 156:pvd + 190] = record(b"\x00", 20, 2048, True)
    root = record(b"\x00", 20, 2048, True) + record(b"\x01", 20, 2048, True)
    root += record(b"PROGRAM", 21, 2048, True)
    image[20 * 2048:20 * 2048 + len(root)] = root
    program = record(filename, 22, 4)
    image[21 * 2048:21 * 2048 + len(program)] = program
    image[22 * 2048:22 * 2048 + 4] = b"test"
    if not raw:
        return image
    result = bytearray()
    for at in range(0, len(image), 2048):
        result += b"\x00" + b"\xff" * 10 + b"\x00" + b"\x00\x00\x00\x01"
        result += image[at:at + 2048] + bytes(288)
    return result


class IsoTests(unittest.TestCase):
    def test_extract_both_sector_formats(self):
        for raw in (False, True):
            with self.subTest(raw=raw), tempfile.TemporaryDirectory() as temporary:
                path = Path(temporary)
                iso = path / "test.iso"
                iso.write_bytes(fixture(raw=raw))
                with IsoImage(iso) as image:
                    self.assertEqual(image.sector_size, 2352 if raw else 2048)
                    self.assertEqual(image.extract_program(path / "out"), 1)
                    with self.assertRaises(FileExistsError):
                        image.extract_program(path / "out")
                self.assertEqual((path / "out/program/rally.exe").read_bytes(), b"test")

    def test_reject_bad_images(self):
        bad_extent = fixture()
        bad_extent[21 * 2048 + 2:21 * 2048 + 10] = both32(1000)
        cycle = fixture()
        cycle[20 * 2048 + 68 + 2:20 * 2048 + 68 + 10] = both32(20)
        for data in (b"bad", fixture()[:-2048], fixture(b"../ESCAPE;1"),
                     fixture(b"..;1"), bad_extent, cycle):
            with self.subTest(length=len(data)), tempfile.TemporaryDirectory() as temporary:
                path = Path(temporary)
                iso = path / "test.iso"
                iso.write_bytes(data)
                with self.assertRaises(ValueError):
                    with IsoImage(iso) as image:
                        image.extract_program(path / "out")


if __name__ == "__main__":
    unittest.main()
