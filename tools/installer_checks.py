# SPDX-License-Identifier: GPL-3.0-only
"""Read-only preflight shared by the CLI and graphical installer."""

import hashlib
import os
from pathlib import Path
import shutil
import subprocess

from cd_audio import audio_layout
from iso9660 import IsoImage
from soundtrack import soundfont_info, validate_identity

KNOWN_EXE = "aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83"
REPO = Path(__file__).resolve().parents[1]


def default_output():
    data_home = Path(os.environ.get("XDG_DATA_HOME", ""))
    if not data_home.is_absolute():
        data_home = Path.home() / ".local/share"
    return data_home / "dtvr-recomb"


def validate_output(output):
    output = output.expanduser()
    if output.is_symlink():
        raise ValueError("Choose a new directory, not a symbolic link")
    output = output.resolve()
    if output == REPO or REPO in output.parents:
        raise ValueError("Installation output must be outside the source checkout")
    return output


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def preflight(iso, output, jobs=1, soundfonts=None, music_style="classic",
              music_pack_id="rendered-gm", music_pack_name="Rendered SoundFont"):
    if type(jobs) is not int or not 1 <= jobs <= 256:
        raise ValueError("Build jobs must be between 1 and 256")
    iso = iso.expanduser().resolve(strict=True)
    if not iso.is_file():
        raise ValueError("Choose a readable original ISO file")
    output = validate_output(output)
    if output.exists():
        raise ValueError(f"Target already exists; choose a new directory: {output}")
    with IsoImage(iso) as image:
        files = [entry for entry in image.files() if entry[0].parts[0] == "program"]
        entry = next((entry for entry in files if str(entry[0]) == "program/rally.exe"), None)
        if entry is None or hashlib.sha256(image.read(entry[1], entry[2])).hexdigest() != KNOWN_EXE:
            raise ValueError("Unsupported original executable; this disc version is not supported")
        required = {"program/butt/1/map.bmp", "program/map/0/part0000.sbb", "program/sbs/d.sbs"}
        if not required.issubset({str(entry[0]) for entry in files}):
            raise ValueError("Original disc is missing required game data")
        sector_size = image.sector_size
    iso_hash = digest(iso)
    has_cd = audio_layout(iso_hash, iso.stat().st_size) is not None
    banks = []
    if soundfonts:
        validate_identity(music_pack_id, music_pack_name, music_style)
        banks = [soundfont_info(Path(path)) for path in soundfonts]
    requirements = ["cmake", "ctest", "c++", "pkg-config"]
    if has_cd or soundfonts:
        requirements += ["ffmpeg", "ffprobe"]
    if soundfonts:
        requirements.append("fluidsynth")
    missing = [name for name in requirements if not shutil.which(name)]
    if missing:
        raise ValueError("Missing dependencies: " + ", ".join(missing))
    sdl = subprocess.run(["pkg-config", "--atleast-version=2.0.10", "sdl2"], capture_output=True)
    if sdl.returncode:
        raise ValueError("SDL2 development files >= 2.0.10 are required (pkg-config sdl2)")
    if has_cd or soundfonts:
        codecs = subprocess.run(["ffmpeg", "-hide_banner", "-encoders"],
                                capture_output=True, text=True, timeout=30)
        if codecs.returncode or "libvorbis" not in codecs.stdout:
            raise ValueError("FFmpeg must provide the libvorbis encoder")
    parent = output.parent
    while not parent.exists():
        parent = parent.parent
    if not parent.is_dir() or not os.access(parent, os.W_OK | os.X_OK):
        raise ValueError("Installation destination is not writable")
    reserve = 512 * 1024 * 1024 + 2 * sum(entry[2] for entry in files)
    free = shutil.disk_usage(parent).free
    if free < reserve:
        raise ValueError(f"Not enough free disk space: reserve {reserve // 2**20} MiB for import/build")
    return {"iso": str(iso), "output": str(output), "iso_sha256": iso_hash,
            "iso_sector_size": sector_size, "original_executable_sha256": KNOWN_EXE,
            "program_files": len(files), "cd_audio_supported": has_cd,
            "soundfonts": banks, "dependencies": requirements,
            "free_bytes": free, "estimated_reserve_bytes": reserve}
