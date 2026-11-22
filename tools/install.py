#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build a native DTVR installation from the user's original ISO, without root."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

from inspect_game import inspect_game
from iso9660 import IsoImage
from cd_audio import audio_layout, convert_audio
from soundtrack import render_pack
from installer_checks import KNOWN_EXE, REPO, default_output, validate_output, digest, preflight
from installation_manifest import write_manifest, verify_installation
from project_version import read_version


def install(iso, output, jobs, soundfonts=None, music_style="classic",
            music_pack_id="rendered-gm", music_pack_name="Rendered SoundFont"):
    print("DTVR_STAGE:1:Checking original media and build dependencies", flush=True)
    report = preflight(iso, output, jobs, soundfonts, music_style, music_pack_id, music_pack_name)
    iso, output = Path(report["iso"]), Path(report["output"])
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".dtvr-install-", dir=output.parent) as temporary:
        stage = Path(temporary) / "installation"
        data = stage / "data"
        data.mkdir(parents=True)
        print("DTVR_STAGE:2:Importing original game data", flush=True)
        with IsoImage(iso) as image:
            sector_size = image.sector_size
            count = image.extract_program(data)
        analysis = inspect_game(data)
        if analysis["executable"]["sha256"] != KNOWN_EXE:
            raise ValueError("Unsupported rally.exe version; preserve this image for version analysis")
        for required in ("butt/1/map.bmp", "map/0/part0000.sbb", "sbs/d.sbs"):
            if not (data / "program" / required).is_file():
                raise ValueError(f"Required original data missing: {required}")
        # Original Windows binaries are validation inputs, not native runtime dependencies.
        for name in ("rally.exe", "sbtimer.dll", "wavemix.dll", "wavemix.ini"):
            path = data / "program" / name
            if path.is_file():
                path.unlink()
        print(f"Validated original executable; extracted {count} files.", flush=True)
        print("DTVR_STAGE:3:Preparing local Ogg audio", flush=True)
        iso_hash = report["iso_sha256"]
        layout = audio_layout(iso_hash, iso.stat().st_size)
        if layout is not None:
            audio = convert_audio(iso, data / "music", layout)
        else:
            audio = {"status": "no-known-layout", "regions": []}
            print("No verified audio-region profile for this image; no CD audio converted.", flush=True)
        packs = []
        if soundfonts:
            pack = render_pack(data / "program/snd/midi", soundfonts,
                               data / "music/packs" / music_pack_id,
                               music_pack_id, music_pack_name, music_style)
            packs.append({"id": pack["id"], "name": pack["name"],
                          "directory": f"data/music/packs/{music_pack_id}", "cue_count": len(pack["cues"])})
        build = Path(temporary) / "build"
        print("DTVR_STAGE:4:Configuring and compiling the native Linux build", flush=True)
        subprocess.run(["cmake", "-S", str(REPO), "-B", str(build),
                        "-DCMAKE_BUILD_TYPE=Release"], check=True)
        subprocess.run(["cmake", "--build", str(build), "--parallel", str(jobs)], check=True)
        print("DTVR_STAGE:5:Testing and installing the native build", flush=True)
        subprocess.run(["ctest", "--test-dir", str(build), "--output-on-failure"], check=True)
        subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage)], check=True)
        env = dict(os.environ, SDL_VIDEODRIVER="dummy")
        binary = stage / "bin/dtvr-recomb"
        print("DTVR_STAGE:6:Checking the installed runtime", flush=True)
        for view in ([], ["--tiles"], ["--sprites"], ["--world"]):
            subprocess.run([str(binary), "--data", str(data), "--smoke-test", *view],
                           env=env, check=True)
        launcher = stage / "play"
        launcher.write_text('#!/bin/sh\nset -eu\n'
                            'install_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                            'exec "$install_dir/bin/dtvr-recomb" --data "$install_dir/data" "$@"\n')
        launcher.chmod(0o755)
        print("DTVR_STAGE:7:Verifying files and completing installation", flush=True)
        write_manifest(stage, {"capability": "native-linux-prototype",
                              "project_version": read_version(),
                              "iso_sha256": iso_hash, "iso_sector_size": sector_size,
                              "cd_audio": audio, "soundtrack_packs": packs,
                              "executable_sha256": KNOWN_EXE})
        verify_installation(stage)
        if output.exists() or output.is_symlink():
            raise ValueError(f"Target appeared during installation: {output}")
        stage.rename(output)
    print(f"Installed native build. Start with: {output / 'play'}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", action="version", version=f"DTVR Recomb Setup {read_version()}")
    parser.add_argument("--iso", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=default_output(),
                        help="New installation directory outside the checkout (default: XDG data directory)")
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 8))
    parser.add_argument("--soundfont", type=Path, action="append", default=[],
                        help="Render a complete Ogg pack; repeat for ordered base/overlay banks")
    parser.add_argument("--music-style", choices=("classic", "dry"), default="classic")
    parser.add_argument("--music-pack-id", default="rendered-gm")
    parser.add_argument("--music-pack-name", default="Rendered SoundFont")
    parser.add_argument("--check", action="store_true", help="Check ISO, dependencies and destination without writing files")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if not args.soundfont and (args.music_style != "classic" or args.music_pack_id != "rendered-gm"
                               or args.music_pack_name != "Rendered SoundFont"):
        parser.error("Music pack options require --soundfont")
    try:
        if args.check:
            print(json.dumps(preflight(args.iso, args.output, args.jobs, args.soundfont,
                                       args.music_style, args.music_pack_id, args.music_pack_name), indent=2))
        else:
            install(args.iso, args.output, args.jobs, args.soundfont, args.music_style,
                    args.music_pack_id, args.music_pack_name)
    except KeyboardInterrupt:
        parser.exit(130, "Installation cancelled; temporary staging was cleaned up.\n")
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        parser.exit(1, f"Installation failed: {error}\n")


if __name__ == "__main__":
    main()
