#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Package committed installer sources and a portable Python/Tk runtime as AppImage.

Supply trusted, externally downloaded tools explicitly; this builder never downloads.
"""

import argparse
import io
import json
import os
from pathlib import Path, PurePosixPath
import platform
import shutil
import subprocess
import tarfile
import tempfile

from check_repository import check_file
from installer_checks import REPO, digest, validate_output
from project_version import read_version


def export_source(destination):
    """Export HEAD only, excluding ignored assets and concurrent working-tree edits."""
    archive = subprocess.check_output(["git", "archive", "--format=tar", "HEAD"], cwd=REPO)
    with tarfile.open(fileobj=io.BytesIO(archive)) as files:
        for member in files:
            relative = PurePosixPath(member.name)
            if relative.is_absolute() or ".." in relative.parts:
                raise ValueError(f"Unsafe source path: {member.name}")
            if member.isdir():
                continue
            if not member.isfile():
                raise ValueError(f"Source must contain regular files: {member.name}")
            content = files.extractfile(member).read()
            problem = check_file(member.name, content)
            if problem:
                raise ValueError(f"{member.name}: {problem}")
            target = destination / member.name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
            target.chmod(0o755 if member.mode & 0o111 else 0o644)


def build(python_runtime, appimagetool, runtime, output, arch):
    output = validate_output(output)
    if output.exists():
        raise ValueError("Choose a new package output directory")
    python_runtime = python_runtime.resolve()
    if not (python_runtime / "bin/python3").is_file() or (python_runtime / "pyvenv.cfg").exists():
        raise ValueError("Provide a complete relocatable Python/Tk prefix, not a virtualenv")
    if python_runtime in (Path("/"), Path("/usr"), Path("/usr/local")):
        raise ValueError("A system Python prefix cannot be bundled as a portable runtime")
    for tool in (appimagetool, runtime):
        if not tool.is_file():
            raise ValueError(f"Missing supplied AppImage tool/runtime: {tool}")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".dtvr-appimage-", dir=output.parent) as temporary:
        appdir = Path(temporary) / "AppDir"
        source = appdir / "usr/src/dtvr-recomb"
        export_source(source)
        version = read_version(source)
        if version != read_version():
            raise ValueError("Commit VERSION before packaging a release")
        filename = f"DTVR-Recomb-Setup-{version}-{arch}.AppImage"
        bundled = appdir / "usr/python"
        shutil.copytree(python_runtime, bundled, symlinks=True,
                        ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
        # Internal links are retained; external links would break on the user's machine.
        for path in bundled.rglob("*"):
            if path.is_symlink() and not path.resolve().is_relative_to(bundled.resolve()):
                raise ValueError(f"Python runtime has a nonportable symlink: {path.relative_to(bundled)}")
        env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1", PYTHONNOUSERSITE="1")
        env.pop("PYTHONHOME", None)
        env.pop("PYTHONPATH", None)
        probe = subprocess.check_output([str(bundled / "bin/python3"), "-I", "-c",
                "import sys, tkinter; print(sys.prefix)"], env=env, text=True).strip()
        if Path(probe).resolve() != bundled.resolve():
            raise ValueError("Python runtime failed the relocation/Tk import check")
        shutil.copy2(source / "packaging/AppRun", appdir / "AppRun")
        (appdir / "AppRun").chmod(0o755)
        for name in ("dtvr-recomb-setup.svg", ".DirIcon"):
            shutil.copy2(source / "assets/branding/icon.svg", appdir / name)
        (appdir / "dtvr-recomb-setup.desktop").write_text(
            "[Desktop Entry]\nType=Application\n"
            f"Name=DTVR Recomb Setup {version}\nExec=AppRun\nIcon=dtvr-recomb-setup\n"
            "Terminal=false\nCategories=Game;Utility;\n")
        metadata = {"project_version": version, "architecture": arch,
                    "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip(),
                    "appimagetool_sha256": digest(appimagetool), "runtime_sha256": digest(runtime)}
        (appdir / "build-info.json").write_text(json.dumps(metadata, indent=2) + "\n")
        artifact = Path(temporary) / filename
        subprocess.run([str(appimagetool.resolve()), "--no-appstream", "--runtime-file",
                        str(runtime.resolve()), str(appdir), str(artifact)],
                       env=dict(env, ARCH=arch), check=True)
        artifact.chmod(0o755)
        checksum = digest(artifact)
        # Publish the completed package only after the external tool succeeds.
        output.mkdir()
        artifact.rename(output / filename)
        (output / f"{filename}.sha256").write_text(f"{checksum}  {filename}\n")
    print(output / filename)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--python-runtime", type=Path, required=True)
    parser.add_argument("--appimagetool", type=Path, required=True,
                        help="Executable tool, preferably its extracted AppRun (no FUSE needed)")
    parser.add_argument("--runtime", type=Path, required=True, help="Verified type-2 AppImage runtime")
    parser.add_argument("--output", type=Path, required=True, help="New directory outside the checkout")
    parser.add_argument("--arch", choices=("x86_64", "aarch64"), default=platform.machine())
    args = parser.parse_args()
    if args.arch != platform.machine():
        parser.error("Build and test on the target architecture; cross packaging is not supported")
    try:
        build(args.python_runtime, args.appimagetool, args.runtime, args.output, args.arch)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"AppImage build failed: {error}\n")


if __name__ == "__main__":
    main()
