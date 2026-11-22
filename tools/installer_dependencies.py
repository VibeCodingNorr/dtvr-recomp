# SPDX-License-Identifier: GPL-3.0-only
"""Read-only dependency inventory and explicitly copied package-manager guidance."""

import platform
import re
import shutil
import subprocess

TOOLS = {"cmake": "CMake", "ctest": "Build-Tests (CTest)", "c++": "C++-Compiler",
         "pkg-config": "Entwicklungsbibliotheken finden (pkg-config)",
         "ffmpeg": "Audio konvertieren (FFmpeg)", "ffprobe": "Audio prüfen (ffprobe)",
         "fluidsynth": "Soundfont in Ogg umwandeln (FluidSynth)"}
MODULES = [("sdl2", ["--atleast-version=2.0.10", "sdl2"], "SDL2 development files >= 2.0.10"),
           ("SDL2_ttf", ["--exists", "SDL2_ttf"], "SDL2_ttf development files"),
           ("vorbisfile", ["--exists", "vorbisfile"], "vorbisfile development files"),
           ("json-c", ["--exists", "json-c >= 0.15"], "json-c development files >= 0.15")]
LABELS = {"sdl2": "SDL2 ab 2.0.10", "SDL2_ttf": "Textdarstellung (SDL2_ttf)",
          "vorbisfile": "Ogg-Wiedergabe (libvorbisfile)", "json-c": "Klangset-Kataloge (json-c ab 0.15)",
          "fonts": "Arial oder Liberation Sans: alle vier Schriftschnitte",
          "vorbis-encoder": "FFmpeg mit Ogg-Vorbis-Encoder"}
PACKAGES = {
    "debian": {"cmake": "cmake", "ctest": "cmake", "c++": "build-essential", "pkg-config": "pkg-config",
               "sdl2": "libsdl2-dev", "SDL2_ttf": "libsdl2-ttf-dev", "vorbisfile": "libvorbis-dev",
               "json-c": "libjson-c-dev", "fonts": "fonts-liberation", "ffmpeg": "ffmpeg",
               "ffprobe": "ffmpeg", "vorbis-encoder": "ffmpeg", "fluidsynth": "fluidsynth"},
    "arch": {"cmake": "cmake", "ctest": "cmake", "c++": "gcc", "pkg-config": "pkgconf",
             "sdl2": "sdl2-compat", "SDL2_ttf": "sdl2_ttf", "vorbisfile": "libvorbis",
             "json-c": "json-c", "fonts": "ttf-liberation", "ffmpeg": "ffmpeg",
             "ffprobe": "ffmpeg", "vorbis-encoder": "ffmpeg", "fluidsynth": "fluidsynth"}}


class DependencyError(ValueError):
    def __init__(self, report):
        self.report = report
        missing = [item for item in report["items"] if not item["ok"]]
        super().__init__("Missing dependencies: " + ", ".join(item["id"] for item in missing)
                         + "; " + "; ".join(item["reason"] for item in missing))


def distribution(info=None):
    if info is None:
        try:
            info = platform.freedesktop_os_release()
        except OSError:
            info = {}
    identities = [info.get("ID", ""), *info.get("ID_LIKE", "").split()]
    family = next((family for name in identities for family, names in
                   (("debian", {"debian", "ubuntu", "linuxmint", "pop"}),
                    ("arch", {"arch", "cachyos", "manjaro", "endeavouros"})) if name in names), None)
    name = re.sub(r"[\x00-\x1f\x7f]", "", info.get("PRETTY_NAME", info.get("ID", "Unbekannt")))[:120]
    return {"name": name, "family": family}


def package_command(missing, system):
    mapping = PACKAGES.get(system.get("family"))
    if not mapping or not missing:
        return ""
    packages = sorted({mapping[item] for item in missing if item in mapping})
    if not packages:
        return ""
    prefix = "sudo apt-get install " if system["family"] == "debian" else "sudo pacman -S --needed "
    return prefix + " ".join(packages)


def dependency_report(font, audio=False, render=False, system=None):
    requirements = ["cmake", "ctest", "c++", "pkg-config"]
    if audio or render:
        requirements += ["ffmpeg", "ffprobe"]
    if render:
        requirements.append("fluidsynth")
    items = []

    def add(key, ok, reason, checked=True):
        items.append({"id": key, "label": TOOLS.get(key, LABELS.get(key, key)), "ok": bool(ok),
                      "state": "ok" if ok else "missing" if checked else "unknown", "reason": reason})

    present = {name: bool(shutil.which(name)) for name in requirements}
    for name, ok in present.items():
        add(name, ok, f"Required tool: {name}")
    for key, arguments, reason in MODULES:
        ok = False
        if present["pkg-config"]:
            try:
                ok = subprocess.run(["pkg-config", *arguments], capture_output=True, timeout=15).returncode == 0
            except (OSError, subprocess.SubprocessError):
                pass
        add(key, ok, reason, checked=present["pkg-config"])
    add("fonts", bool(font), "Install a complete Arial or Liberation Sans font family (regular, bold, italic, bold italic)")
    if (audio or render) and present["ffmpeg"]:
        ok = False
        try:
            encoders = subprocess.run(["ffmpeg", "-hide_banner", "-encoders"],
                                     capture_output=True, text=True, timeout=30)
            ok = encoders.returncode == 0 and "libvorbis" in (encoders.stdout or "")
        except (OSError, subprocess.SubprocessError):
            pass
        add("vorbis-encoder", ok, "FFmpeg must provide the libvorbis encoder")
    system = system if system is not None else distribution()
    missing = [item["id"] for item in items if item["state"] == "missing"]
    return {"kind": "dependencies", "ready": all(item["ok"] for item in items), "items": items, "missing": missing,
            "distribution": system, "command": package_command(missing, system), "requirements": requirements}
