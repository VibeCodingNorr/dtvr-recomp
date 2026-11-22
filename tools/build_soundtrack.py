#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build an external Ogg cue pack, or verify an existing pack without MIDI/SoundFonts."""

import argparse
from pathlib import Path
import subprocess

from soundtrack import render_pack, verify_pack


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--midi-dir", type=Path)
    parser.add_argument("--soundfont", type=Path, action="append", default=[])
    parser.add_argument("--output", type=Path)
    parser.add_argument("--pack-id", default="rendered-gm")
    parser.add_argument("--name", default="Rendered SoundFont")
    parser.add_argument("--style", choices=("classic", "dry"), default="classic")
    parser.add_argument("--verify", type=Path)
    args = parser.parse_args()
    if args.verify and (args.midi_dir or args.soundfont or args.output):
        parser.error("Use --verify independently of rendering arguments")
    if not args.verify and (not args.midi_dir or not args.soundfont or not args.output):
        parser.error("Rendering requires --midi-dir, --soundfont and --output")
    try:
        if args.verify:
            pack = verify_pack(args.verify)
            print(f"Verified {pack['id']}: {len(pack['cues'])} local Ogg cues")
        else:
            pack = render_pack(args.midi_dir, args.soundfont, args.output, args.pack_id, args.name, args.style)
            print(f"Prepared {pack['id']}: {len(pack['cues'])} cues in {args.output}")
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        parser.exit(1, f"Soundtrack preparation failed: {error}\n")


if __name__ == "__main__":
    main()
