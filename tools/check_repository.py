#!/usr/bin/env python3
"""Check tracked files for bundled game content and machine-specific data paths."""

from pathlib import Path
import re
import subprocess

REPO = Path(__file__).resolve().parents[1]
FORBIDDEN_SUFFIXES = {
    ".iso", ".bin", ".cue", ".toc", ".exe", ".dll", ".drv", ".wav", ".ogg",
    ".mp3", ".flac", ".mid", ".midi", ".bmp", ".png", ".jpg", ".jpeg", ".gif",
    ".sbs", ".sbb", ".flc", ".map", ".dat", ".tow", ".asm",
}
PRIVATE_PATH = re.compile(r"(?<![\w./-])/(?:" + "|".join(("media", "mnt", "home", "Users")) + r")/[^\s`\"']+")


def check_file(name, content, symlink=False):
    if symlink:
        return "tracked symlinks are not allowed"
    if Path(name).suffix.lower() in FORBIDDEN_SUFFIXES:
        return "game/media/generated binary extension is not allowed"
    try:
        text = content.decode("utf-8")
    except UnicodeDecodeError:
        return "tracked files must be UTF-8 source or documentation"
    if "\x00" in text:
        return "binary content is not allowed"
    if PRIVATE_PATH.search(text):
        return "machine-specific data path found; use a generic user-supplied path"
    return None


def main():
    entries = subprocess.check_output(["git", "ls-files", "--stage", "-z"], cwd=REPO)
    failures = []
    for entry in entries.split(b"\0"):
        if not entry:
            continue
        metadata, raw_name = entry.split(b"\t", 1)
        name = raw_name.decode("utf-8")
        path = REPO / name
        if metadata.startswith(b"120000"):
            failures.append(f"{name}: tracked symlinks are not allowed")
            continue
        if not path.exists():
            failures.append(f"{name}: tracked file is missing")
            continue
        error = check_file(name, path.read_bytes())
        if error:
            failures.append(f"{name}: {error}")
    if failures:
        print("\n".join(failures))
        return 1
    print("Repository source-only checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
