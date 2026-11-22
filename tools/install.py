#!/usr/bin/env python3
"""Build a native DTVR installation from the user's original ISO, without root."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from inspect_game import inspect_game
from iso9660 import IsoImage
from cd_audio import audio_layout, convert_audio

KNOWN_EXE = "aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83"
REPO = Path(__file__).resolve().parent.parent


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def install(iso, output, jobs):
    iso = iso.resolve(strict=True)
    output = output.absolute()
    if output.exists() or output.is_symlink():
        raise ValueError(f"Target already exists; choose a new directory: {output}")
    for command in ("cmake", "c++", "pkg-config"):
        if not shutil.which(command):
            raise ValueError(f"Missing build dependency: {command}")
    subprocess.run(["pkg-config", "--atleast-version=2.0.10", "sdl2"], check=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".dtvr-install-", dir=output.parent) as temporary:
        stage = Path(temporary) / "installation"
        data = stage / "data"
        data.mkdir(parents=True)
        print("Reading original ISO...", flush=True)
        with IsoImage(iso) as image:
            sector_size = image.sector_size
            count = image.extract_program(data)
        analysis = inspect_game(data)
        if analysis["executable"]["sha256"] != KNOWN_EXE:
            raise ValueError("Unsupported rally.exe version; preserve this image for version analysis")
        for required in ("butt/1/map.bmp", "map/0/part0000.sbb", "sbs/d.sbs"):
            if not (data / "program" / required).is_file():
                raise ValueError(f"Required original data missing: {required}")
        print(f"Validated original executable; extracted {count} files.", flush=True)
        iso_hash = digest(iso)
        layout = audio_layout(iso_hash, iso.stat().st_size)
        if layout is not None:
            audio = convert_audio(iso, data / "music", layout)
        else:
            audio = {"status": "no-known-layout", "regions": []}
            print("No verified audio-region profile for this image; no CD audio converted.", flush=True)
        build = Path(temporary) / "build"
        subprocess.run(["cmake", "-S", str(REPO), "-B", str(build),
                        "-DCMAKE_BUILD_TYPE=Release"], check=True)
        subprocess.run(["cmake", "--build", str(build), "--parallel", str(jobs)], check=True)
        subprocess.run(["ctest", "--test-dir", str(build), "--output-on-failure"], check=True)
        subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage)], check=True)
        env = dict(os.environ, SDL_VIDEODRIVER="dummy")
        binary = stage / "bin/dtvr-recomb"
        for view in ([], ["--tiles"], ["--sprites"]):
            subprocess.run([str(binary), "--data", str(data), "--smoke-test", *view],
                           env=env, check=True)
        manifest = {"schema_version": 1, "capability": "native-data-viewer",
                    "iso_sha256": iso_hash, "iso_sector_size": sector_size,
                    "cd_audio": audio,
                    "executable_sha256": KNOWN_EXE,
                    "files": {p.relative_to(stage).as_posix(): digest(p)
                              for p in sorted(stage.rglob("*")) if p.is_file()}}
        (stage / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        launcher = stage / "play"
        launcher.write_text('#!/bin/sh\nset -eu\n'
                            'install_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                            'exec "$install_dir/bin/dtvr-recomb" --data "$install_dir/data" "$@"\n')
        launcher.chmod(0o755)
        if output.exists() or output.is_symlink():
            raise ValueError(f"Target appeared during installation: {output}")
        stage.rename(output)
    print(f"Installed native viewer. Start with: {output / 'play'}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=REPO / "local/dtvr")
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 8))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    try:
        install(args.iso, args.output, args.jobs)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Installation failed: {error}\n")


if __name__ == "__main__":
    main()
