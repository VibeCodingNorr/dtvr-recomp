# SPDX-License-Identifier: GPL-3.0-only
"""Edition profiles of the installer, mirroring src/edition.{hpp,cpp}.

An edition is identified only by the SHA-256 of the original rally.exe, never by the
presence of files. Each profile names the language letter that `dat/lan.inf` must
start with and the original files that must exist below `program/` (lower case; the
ISO reader and the directory import normalise case). Evidence for the US English
retail edition is in docs/media-variants.md ("Edition profiles").
"""

from pathlib import Path

GERMAN_RETAIL = {
    "id": "de-retail",
    "name": "Die total verrückte Rallye (German retail)",
    "executable_sha256": "aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83",
    "language": "d",
    "required": ("dat/lan.inf", "dat/d/rally_d.dat", "dat/d/sync.dat", "map/europe.inf",
                 "map/europe.str", "map/europe.map", "map/europe.pit", "map/europe_d.tow",
                 "map/0/part0000.sbb", "sbs/d.sbs", "butt/1/map.bmp"),
    "overview": "butt/1/map.bmp",
    # CD audio layouts of German images are keyed by ISO hash in tools/cd_audio.py.
    "known_isos": {},
}

ENGLISH_RETAIL = {
    "id": "en-us-retail",
    "name": "Dr. Drago's Madcap Chase (US English retail)",
    "executable_sha256": "a8374ca9e39d70b4508efefa782b7135c650792be36600289356a51de819225a",
    "language": "e",
    "required": ("dat/lan.inf", "dat/e/rally_e.dat", "dat/e/sync.dat", "map/europe.inf",
                 "map/europe.str", "map/europe.map", "map/europe.pit", "map/europe_e.tow",
                 "map/0/part0000.sbb", "sbs/d.sbs", "butt/6/map.bmp", "butt/8/map.bmp",
                 "butt/1/map.bmp"),
    "overview": "butt/6/map.bmp",
    # The 2048-byte data track of a release that ships its music as separate Ogg files
    # (tools/release_ogg.py); the image itself holds no CD-DA.
    "known_isos": {"26260860553361b356a8884b290c659ab6cedf2e29645fbcf2f8f0c511551fc7": "no-cd-da-in-image"},
}

# German first; the limited US build c8b7dab6… is deliberately not a profile (user decision).
PROFILES = (GERMAN_RETAIL, ENGLISH_RETAIL)


def profile_for_executable(sha256):
    """The profile of an executable hash, or None for an unsupported version."""
    sha256 = (sha256 or "").lower()
    return next((profile for profile in PROFILES if profile["executable_sha256"] == sha256), None)


def profile_by_id(edition):
    return next((profile for profile in PROFILES if profile["id"] == edition), None)


def require_profile(sha256, kind="disc"):
    profile = profile_for_executable(sha256)
    if profile is None:
        raise ValueError(f"Unsupported original executable; this {kind} version is not supported")
    return profile


def missing_files(profile, names):
    """Required `program/...` paths absent from `names` (lower-case paths below the source root)."""
    return sorted(f"program/{name}" for name in profile["required"] if f"program/{name}" not in names)


def language_from_bytes(data):
    """The language letter (lower case) from the contents of dat/lan.inf, as src/data_paths.cpp."""
    letter = data[:1]
    if not letter or not letter.isalpha():
        raise ValueError("dat/lan.inf does not start with a language letter")
    return letter.decode("ascii").lower()


def require_language(profile, letter):
    """lan.inf must belong to the executable's edition (no mixing of EXE and language data)."""
    if letter != profile["language"]:
        raise ValueError(f"dat/lan.inf names language {letter.upper()}, but rally.exe belongs to "
                         f"{profile['name']} (language {profile['language'].upper()})")


def installed_profile(manifest):
    """The edition of an installation manifest.

    The field "edition" (written since round E2) must name a known profile and agree with
    "executable_sha256"; without it the hash decides. Manifests with neither field (only
    synthetic test installations) count as German, the only edition older installers took.
    """
    edition, sha256 = manifest.get("edition"), manifest.get("executable_sha256")
    if edition is not None:
        profile = profile_by_id(edition)
        if profile is None:
            raise ValueError(f"Installation has an unknown edition: {edition}")
        if sha256 is not None and sha256 != profile["executable_sha256"]:
            raise ValueError("Installation manifest: edition and executable hash disagree")
        return profile
    if sha256 is not None:
        profile = profile_for_executable(sha256)
        if profile is None:
            raise ValueError("Installation has an unsupported executable version")
        return profile
    return GERMAN_RETAIL


def require_same_edition(previous, profile):
    """Updates never turn an installation into another edition."""
    installed = installed_profile(previous)
    if installed["id"] != profile["id"]:
        raise ValueError(f"This installation holds {installed['name']}; the chosen original data is "
                         f"{profile['name']}. Install other editions into a new directory instead of --update")


def directory_language(program):
    """The lan.inf letter of an original program directory (case-insensitive names)."""
    directory = next((p for p in Path(program).iterdir() if p.name.lower() == "dat" and p.is_dir()), None)
    file = next((p for p in directory.iterdir() if p.name.lower() == "lan.inf"), None) if directory else None
    if file is None or file.is_symlink() or not file.is_file():
        raise ValueError("dat/lan.inf missing; cannot verify the edition")
    with file.open("rb") as stream:
        return language_from_bytes(stream.read(1))
