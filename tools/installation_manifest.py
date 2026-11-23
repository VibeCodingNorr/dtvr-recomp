# SPDX-License-Identifier: GPL-3.0-only
"""Complete local installation manifests, without executing installed content."""

import json
import os
from pathlib import Path, PurePosixPath
import re

from installer_checks import digest

FORMAT = "dtvr-recomp-installation"
# Manifest format -> runtime binary; installations of the former name DTVR Recomb
# stay verifiable so that they can be updated (the update writes the new format).
RUNTIMES = {FORMAT: "bin/dtvr-recomp", "dtvr-recomb-installation": "bin/dtvr-recomb"}


def write_manifest(root, metadata, excluded=()):
    files = {}
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise ValueError("Installation must not contain symbolic links")
        relative = path.relative_to(root)
        if (path.is_file() and path != root / "manifest.json"
                and relative.parts[0] != "saves" and relative.as_posix() not in excluded):
            files[path.relative_to(root).as_posix()] = digest(path)
    manifest = {**metadata, "schema_version": 2, "format": FORMAT, "files": files}
    (root / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def verify_installation(root):
    root = root.expanduser().resolve(strict=True)
    manifest_path = root / "manifest.json"
    if manifest_path.is_symlink() or manifest_path.stat().st_size > 8 * 1024 * 1024:
        raise ValueError("Invalid installation manifest")
    manifest = json.loads(manifest_path.read_text())
    if (not isinstance(manifest, dict) or manifest.get("schema_version") != 2
            or manifest.get("format") not in RUNTIMES):
        raise ValueError("Unsupported or legacy manifest; verification requires a new installer build")
    files = manifest.get("files")
    if not isinstance(files, dict) or not 1 <= len(files) <= 20000:
        raise ValueError("Invalid installation file table")
    runtime = RUNTIMES[manifest["format"]]
    if not {"play", runtime, "data/program/butt/1/map.bmp"}.issubset(files):
        raise ValueError("Installation is missing required runtime entries")
    for name, expected in files.items():
        relative = PurePosixPath(name)
        if (not name or relative.is_absolute() or ".." in relative.parts or "\\" in name
                or str(relative) != name or name == "manifest.json"):
            raise ValueError("Unsafe manifest path")
        if not isinstance(expected, str) or not re.fullmatch(r"[a-f0-9]{64}", expected):
            raise ValueError("Invalid manifest hash")
        path = root.joinpath(*relative.parts)
        if path.resolve() != path or path.is_symlink() or not path.is_file():
            raise ValueError(f"Missing or linked installation file: {name}")
        if digest(path) != expected:
            raise ValueError(f"Installation file hash mismatch: {name}")
        if name in ("play", runtime) and not os.access(path, os.X_OK):
            raise ValueError(f"Runtime file is not executable: {name}")
    return {"files_verified": len(files), "soundtrack_packs": manifest.get("soundtrack_packs", [])}


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("installation", type=Path)
    args = parser.parse_args()
    try:
        report = verify_installation(args.installation)
    except (OSError, ValueError, TypeError) as error:
        parser.exit(1, f"Verification failed: {error}\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
