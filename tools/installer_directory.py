# SPDX-License-Identifier: GPL-3.0-only
"""Bounded, read-only inventory and verified import of an original game folder."""

import hashlib
import json
import os
from pathlib import Path
import shutil
import stat


def file_digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def program_directory(source):
    source = source.expanduser()
    if source.is_symlink():
        raise ValueError("Original directory must not be a symbolic link")
    source = source.resolve(strict=True)
    if not source.is_dir():
        raise ValueError("Choose an original game directory")
    children = list(source.iterdir())
    if not any(path.name.lower() == "rally.exe" for path in children):
        matches = [path for path in children if path.name.lower() == "program"]
        if len(matches) != 1 or matches[0].is_symlink() or not matches[0].is_dir():
            raise ValueError("Original directory requires rally.exe or a program/ subdirectory")
        source = matches[0]
    return source


def inventory(program):
    files = {}
    total = 0
    entries = 0

    def walk(directory, parts):
        nonlocal total, entries
        if len(parts) > 16:
            raise ValueError("Original directory exceeds supported depth")
        names = set()
        with os.scandir(directory) as children:
            for child in children:
                entries += 1
                if entries > 20000:
                    raise ValueError("Original directory has too many entries")
                name = child.name.lower()
                if (name in names or name in (".", "..") or "\\" in name
                        or any(ord(char) < 32 for char in name)):
                    raise ValueError("Unsafe or case-colliding original directory entry")
                names.add(name)
                mode = child.stat(follow_symlinks=False).st_mode
                if stat.S_ISDIR(mode):
                    walk(Path(child.path), (*parts, name))
                elif stat.S_ISREG(mode):
                    size = child.stat(follow_symlinks=False).st_size
                    total += size
                    if size > 64 * 1024 * 1024 or total > 2 * 1024**3:
                        raise ValueError("Original directory exceeds supported file/total size")
                    files["/".join(("program", *parts, name))] = {
                        "size": size, "sha256": file_digest(Path(child.path))}
                else:
                    raise ValueError("Original directory contains a linked or special entry")

    walk(program, ())
    return files


def inspect_directory(source, known_exe):
    program = program_directory(source)
    files = inventory(program)
    if files.get("program/rally.exe", {}).get("sha256") != known_exe:
        raise ValueError("Unsupported original executable; this directory version is not supported")
    required = {"program/butt/1/map.bmp", "program/map/0/part0000.sbb", "program/sbs/d.sbs"}
    missing = sorted(required - files.keys())
    if missing:
        raise ValueError("Original directory is missing required game data: "
                         + ", ".join(missing)
                         + ". Choose a complete program directory or use --iso with the original disc image.")
    encoded = json.dumps(files, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return {"directory": str(program), "source_files": files,
            "source_sha256": hashlib.sha256(encoded).hexdigest()}


def import_directory(report, data):
    program = Path(report["directory"])
    expected = report["source_files"]
    if inventory(program) != expected:
        raise ValueError("Original directory changed after validation")

    def copy(directory, parts):
        with os.scandir(directory) as children:
            for child in children:
                relative = (*parts, child.name.lower())
                if child.is_dir(follow_symlinks=False):
                    copy(Path(child.path), relative)
                elif child.is_file(follow_symlinks=False):
                    name = "/".join(("program", *relative))
                    target = data / name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(child.path, target)
                    if (name not in expected or file_digest(target) != expected[name]["sha256"]
                            or target.stat().st_size != expected[name]["size"]):
                        raise ValueError("Original directory changed during import")
                else:
                    raise ValueError("Original directory contains a linked or special entry")

    copy(program, ())
    if inventory(program) != expected:
        raise ValueError("Original directory changed during import")
    return len(expected)
