# SPDX-License-Identifier: GPL-3.0-only
"""Use original MIDI music by default with a locally installed GM bank.

No bank is downloaded or bundled. These are host package locations, not paths
to original game content. Full bank/preset validation remains with the renderer.
"""

from pathlib import Path


SYSTEM_GM_BANKS = (
    Path("/usr/share/soundfonts/FluidR3_GM.sf2"),
    Path("/usr/share/sounds/sf2/FluidR3_GM.sf2"),
    Path("/usr/share/sounds/sf3/default-GM.sf3"),
    Path("/usr/share/sounds/sf2/default-GM.sf2"),
    Path("/usr/share/soundfonts/default.sf2"),
)


def default_soundfont(candidates=None):
    """Choose a readable known GM bank using only a bounded header probe."""
    for candidate in SYSTEM_GM_BANKS if candidates is None else candidates:
        try:
            path = Path(candidate).expanduser()
            if not path.is_file():
                continue
            with path.open("rb") as stream:
                header = stream.read(12)
            if len(header) == 12 and header[:4] == b"RIFF" and header[8:] == b"sfbk":
                return path.resolve(strict=True)
        except OSError:
            continue
    return None


def soundfonts_for_install(soundfonts=None, no_music=False):
    """Require an original-MIDI rendering source unless music was declined."""
    if no_music:
        if soundfonts:
            raise ValueError("--no-music cannot be combined with --soundfont")
        return []
    if soundfonts:
        return [Path(path) for path in soundfonts]
    bank = default_soundfont()
    if bank:
        return [bank]
    raise ValueError("Original MIDI music needs a local General MIDI SoundFont. "
                     "Supply --soundfont /path/to/bank.sf2 or explicitly choose --no-music. "
                     "CD recordings alone are not a complete playable soundtrack.")
