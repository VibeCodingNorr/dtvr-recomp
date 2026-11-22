#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Verify a supplied setup AppImage using extraction, without mounting through FUSE."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

from installer_checks import digest
from project_version import read_version


def check_appimage(artifact):
    artifact = artifact.expanduser().resolve(strict=True)
    if not artifact.is_file() or not os.access(artifact, os.X_OK):
        raise ValueError("Choose an executable setup AppImage")
    checksum_file = artifact.with_name(artifact.name + ".sha256")
    checksum = checksum_file.read_text().strip().split()
    if len(checksum) != 2 or checksum[1] != artifact.name or checksum[0] != digest(artifact):
        raise ValueError("AppImage checksum does not match its sidecar")
    with tempfile.TemporaryDirectory(prefix="dtvr-appimage-check-") as temporary:
        root = Path(temporary)
        env = dict(os.environ, TMPDIR=str(root), XDG_DATA_HOME=str(root / "user-data"),
                   XDG_CONFIG_HOME=str(root / "user-config"),
                   PYTHONHOME=str(root / "invalid-python-home"),
                   PYTHONPATH=str(root / "invalid-python-path"))

        def run(*args):
            return subprocess.run([str(artifact), "--appimage-extract-and-run", *args],
                                  cwd=root, env=env, capture_output=True, text=True, timeout=60)

        version = run("--version")
        if version.returncode or version.stdout.strip() != read_version():
            raise ValueError("Bundled version/Python startup failed: " + version.stderr[-2000:])
        help_result = run("--cli", "--help")
        if help_result.returncode or "--iso" not in help_result.stdout:
            raise ValueError("Bundled CLI startup failed: " + help_result.stderr[-2000:])
        iso = root / "invalid disc with spaces.iso"
        iso.write_bytes(b"synthetic invalid media")
        before = digest(iso)
        output = root / "new installation"
        rejected = run("--cli", "--iso", str(iso), "--output", str(output), "--check")
        if (rejected.returncode != 1 or "Installation failed:" not in rejected.stderr
                or "Traceback" in rejected.stderr or output.exists()
                or list(root.glob(".dtvr-install-*")) or digest(iso) != before):
            raise ValueError("Invalid-media handling failed: " + rejected.stderr[-2000:])
        return {"version": version.stdout.strip(), "sha256": checksum[0],
                "fuse_mount_used": False, "bundled_cli": "passed",
                "invalid_media_rejected_without_writes": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("appimage", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(check_appimage(args.appimage), indent=2))
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"AppImage check failed: {error}\n")


if __name__ == "__main__":
    main()
