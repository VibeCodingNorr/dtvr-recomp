#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render our SVG branding to PNG/ICO assets outside the source checkout."""

import argparse
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[1]
SIZES = (16, 24, 32, 48, 64, 128, 256, 512)


def export(output):
    output = output.expanduser().resolve()
    if output == REPO or REPO in output.parents:
        raise ValueError("Branding exports must be outside the checkout")
    if output.exists():
        raise ValueError("Choose a new output directory; existing files are not overwritten")
    renderer = shutil.which("rsvg-convert")
    if not renderer:
        raise ValueError("Install librsvg's rsvg-convert to export PNG/ICO assets")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".dtvr-branding-", dir=output.parent) as temporary:
        stage = Path(temporary) / "exports"
        stage.mkdir()
        sources = REPO / "assets/branding"
        subprocess.run([renderer, str(sources / "logo.svg"), "-o", str(stage / "logo.png")], check=True)
        icons = []
        for size in SIZES:
            path = stage / f"icon-{size}.png"
            subprocess.run([renderer, "-w", str(size), "-h", str(size),
                            str(sources / "icon.svg"), "-o", str(path)], check=True)
            if size <= 256:
                icons.append((size, path.read_bytes()))
        # ICO permits PNG-encoded images; no imaging-library dependency is needed.
        offset = 6 + 16 * len(icons)
        directory = bytearray(struct.pack("<HHH", 0, 1, len(icons)))
        payload = bytearray()
        for size, png in icons:
            directory += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(png), offset)
            payload += png
            offset += len(png)
        (stage / "dtvr-recomp.ico").write_bytes(directory + payload)
        if output.exists():
            raise ValueError("Output appeared during export")
        stage.rename(output)
    print(f"Branding exports: {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        export(args.output)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Export failed: {error}\n")


if __name__ == "__main__":
    main()
