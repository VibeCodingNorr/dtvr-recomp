# SPDX-License-Identifier: GPL-3.0-only
"""Read-only preflight shared by the CLI and graphical installer."""

import hashlib
import os
from pathlib import Path
import shutil
import subprocess

from cd_audio import audio_layout
from release_ogg import LABEL as RELEASE_LABEL, check_release_audio, release_profile
from iso9660 import IsoImage
from soundtrack import soundfont_info, validate_identity
from installer_dependencies import DependencyError, dependency_report
import editions

# The German reference executable; every supported edition is in tools/editions.py.
KNOWN_EXE = editions.GERMAN_RETAIL["executable_sha256"]
REPO = Path(__file__).resolve().parents[1]


def default_output():
    data_home = Path(os.environ.get("XDG_DATA_HOME", ""))
    if not data_home.is_absolute():
        data_home = Path.home() / ".local/share"
    return data_home / "dtvr-recomp"


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


def interface_font(directories=None):
    # Match src/text.cpp: a complete regular/bold/italic/bold-italic family.
    directories = directories if directories is not None else [
        Path("/usr/share/fonts"), Path("/usr/local/share/fonts"), Path.home() / ".local/share/fonts"]
    names = set()
    for directory in directories:
        for parent, _, files in os.walk(directory):
            names.update(name.lower() for name in files if (Path(parent) / name).is_file())
    families = {"Arial": {"arial.ttf", "arialbd.ttf", "ariali.ttf", "arialbi.ttf"},
                "Liberation Sans": {f"liberationsans-{face}.ttf" for face in
                                    ("regular", "bold", "italic", "bolditalic")}}
    return next((family for family, files in families.items() if files <= names), None)


def preflight(iso, output, jobs=1, soundfonts=None, music_style="classic",
              music_pack_id="rendered-gm", music_pack_name="Rendered SoundFont", update=False,
              directory=None, cd_audio_dir=None):
    if type(jobs) is not int or not 1 <= jobs <= 256:
        raise ValueError("Build jobs must be between 1 and 256")
    if (iso is None) == (directory is None):
        raise ValueError("Choose exactly one original ISO or directory")
    if cd_audio_dir is not None and iso is None:
        raise ValueError(f"{RELEASE_LABEL}: --cd-audio-dir belongs to an --iso data image")
    output = validate_output(output)
    if iso is not None:
        iso = iso.expanduser().resolve(strict=True)
        if not iso.is_file():
            raise ValueError("Choose a readable original ISO file")
        if output in iso.parents:
            raise ValueError("Original ISO must be outside the installation destination")
    else:
        if directory.expanduser().is_symlink():
            raise ValueError("Original directory must not be a symbolic link")
        directory = directory.expanduser().resolve(strict=True)
        if directory == output or output in directory.parents or directory in output.parents:
            raise ValueError("Original directory and installation destination must not overlap")
    previous = None
    if update:
        from installer_update import update_info
        if not output.is_dir():
            raise ValueError("Choose an existing installation for --update")
        previous = update_info(output)
    elif output.exists():
        raise ValueError(f"Target already exists; choose a new directory: {output}")
    source = {}
    if directory is not None:
        from installer_directory import inspect_directory
        source = inspect_directory(directory)
        profile = editions.profile_by_id(source["edition"])
        program = Path(source["directory"])
        if program == output or output in program.parents or program in output.parents:
            raise ValueError("Original directory and installation destination must not overlap")
        file_count = len(source["source_files"])
        source_bytes = sum(entry["size"] for entry in source["source_files"].values())
        iso_hash, sector_size, has_cd = None, None, False
    else:
        with IsoImage(iso) as image:
            files = [entry for entry in image.files() if entry[0].parts[0] == "program"]
            entries = {str(entry[0]): entry for entry in files}
            entry = entries.get("program/rally.exe")
            profile = editions.require_profile(
                hashlib.sha256(image.read(entry[1], entry[2])).hexdigest() if entry is not None else None)
            missing = editions.missing_files(profile, entries.keys())
            if missing:
                raise ValueError("Original disc is missing required game data: " + ", ".join(missing))
            language = entries["program/dat/lan.inf"]
            editions.require_language(profile, editions.language_from_bytes(
                image.read(language[1], min(language[2], 1))))
            sector_size = image.sector_size
        file_count, source_bytes = len(files), sum(entry[2] for entry in files)
        iso_hash = digest(iso)
        has_cd = audio_layout(iso_hash, iso.stat().st_size) is not None
    if previous is not None:
        editions.require_same_edition(previous, profile)
    cd_audio_source = "original-cd-da" if has_cd else None
    if cd_audio_dir is not None:
        # Release Ogg tracks: only for the pinned image of their edition (tools/release_ogg.py).
        release = release_profile(iso_hash)
        if release is None:
            raise ValueError(f"{RELEASE_LABEL}: only accepted with the pinned English retail data image; "
                             "this image belongs to another edition or release")
        if release["edition"] != profile["id"]:
            raise ValueError(f"{RELEASE_LABEL}: only accepted for edition {release['edition']}; "
                             "this image holds another edition")
        cd_audio_dir = check_release_audio(cd_audio_dir, release)
        if cd_audio_dir == output or output in cd_audio_dir.parents or cd_audio_dir in output.parents:
            raise ValueError("Release audio directory and installation destination must not overlap")
        cd_audio_source = RELEASE_LABEL  # copied, not converted: no encoder needed
    banks = []
    if soundfonts:
        validate_identity(music_pack_id, music_pack_name, music_style)
        banks = [soundfont_info(Path(path)) for path in soundfonts]
    font = interface_font()
    dependencies = dependency_report(font, audio=has_cd, render=bool(soundfonts))
    if not dependencies["ready"]:
        raise DependencyError(dependencies)
    requirements = dependencies["requirements"]
    parent = output.parent
    while not parent.exists():
        parent = parent.parent
    if not parent.is_dir() or not os.access(parent, os.W_OK | os.X_OK):
        raise ValueError("Installation destination is not writable")
    retained_bytes = sum(path.stat().st_size for path in output.rglob("*") if path.is_file()) if update else 0
    reserve = 512 * 1024 * 1024 + 2 * source_bytes + retained_bytes
    free = shutil.disk_usage(parent).free
    if free < reserve:
        raise ValueError(f"Not enough free disk space: reserve {reserve // 2**20} MiB for import/build")
    return {**source, "source_kind": "directory" if directory is not None else "iso",
            "iso": str(iso) if iso is not None else None, "output": str(output), "iso_sha256": iso_hash,
            "iso_sector_size": sector_size, "original_executable_sha256": profile["executable_sha256"],
            "edition": profile["id"],
            "program_files": file_count, "cd_audio_supported": cd_audio_source is not None,
            "cd_audio_source": cd_audio_source,
            "cd_audio_dir": str(cd_audio_dir) if cd_audio_dir is not None else None,
            "soundfonts": banks, "dependencies": requirements,
            "update": update, "previous_version": previous.get("project_version") if previous else None,
            "interface_font": font, "dependency_report": dependencies,
            "free_bytes": free, "estimated_reserve_bytes": reserve}
