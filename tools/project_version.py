#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Single project version and release-tag validation."""

import argparse
from pathlib import Path
import re
import subprocess

REPO = Path(__file__).resolve().parents[1]
PATTERN = r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"


def version_tuple(value):
    if not isinstance(value, str) or not re.fullmatch(PATTERN, value):
        raise ValueError("Version must be MAJOR.MINOR.PATCH without leading zeroes")
    return tuple(int(part) for part in value.split("."))


def read_version(root=REPO):
    value = (root / "VERSION").read_text().strip()
    version_tuple(value)
    return value


def validate_release(tag, version, previous=None):
    current = version_tuple(version)
    if tag != f"v{version}":
        raise ValueError("Release tag must exactly match v<VERSION>")
    if previous is not None and current <= version_tuple(previous):
        raise ValueError("Every new release must increase the project version")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release-tag")
    parser.add_argument("--previous")
    parser.add_argument("--check-tag-history", action="store_true")
    args = parser.parse_args()
    try:
        version = read_version()
        if args.previous and not args.release_tag:
            parser.error("--previous requires --release-tag")
        if args.release_tag:
            validate_release(args.release_tag, version, args.previous)
        if args.check_tag_history:
            if not args.release_tag:
                parser.error("--check-tag-history requires --release-tag")
            tags = subprocess.check_output(["git", "tag", "--list"], cwd=REPO, text=True)
            for tag in tags.splitlines():
                if tag != args.release_tag and re.fullmatch("v" + PATTERN, tag):
                    validate_release(args.release_tag, version, tag[1:])
    except (OSError, ValueError) as error:
        parser.exit(1, f"Version check failed: {error}\n")
    print(version)


if __name__ == "__main__":
    main()
