# DTVR Recomb

Native Linux reconstruction of **Die total verrückte Rallye** (Blue Byte).
The first milestone is an SDL2 viewer for the original map and graphics blocks.
There is no playable game logic or executable recompilation yet.

## Install from your original ISO

The repository provides the native source and installer. You supply your own
original CD image; neither the ISO nor original game data is distributed here.
Requires Python 3, CMake, a C++17 compiler, pkg-config and SDL2 development files.

```sh
python3 tools/install.py --iso /media/steam1/Games/Dtvr/dtvr.iso
./local/dtvr/play
```

The installer reads the ISO without mounting it, extracts the `program` tree,
validates the known original executable, builds and tests the native code, and
creates a local installation with a launcher and hash manifest. It supports
2048-byte ISO9660 images and raw Mode 1 images with 2352-byte sectors, including
the local `dtvr.iso`. No root access or Windows installer is needed.

Use `--output /path/to/new/directory` to choose another destination and `--jobs N`
to limit build parallelism. Existing destinations are never updated or overwritten.
The ISO can be disconnected after installation. The installed application is
currently the data viewer; extracting the ISO does not yet translate its executable
or produce a playable reconstruction. See [installer details](docs/installer.md).

For the known mixed-mode image, installation also converts six recovered audio
regions to Ogg Vorbis in `data/music/` (requires FFmpeg/libvorbis and ffprobe).
Original track IDs and playback triggers still need reconstruction; the viewer
does not yet play music. The intended native game plays local Ogg assets in place
of original CD playback. See [CD audio and playback plan](docs/cd-audio.md).

## Build and run

Requires a C++17 compiler, CMake, pkg-config and SDL2 development files.
Python 3 is used for the analysis tool. No Python packages are required.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/dtvr-recomb --data /media/steam1/Games/Dtvr/ISOINHALT
```

`--data` accepts either the CD root or its `program` directory. Supply your own
original files; game assets and executables are not included in this repository.
The viewer only reads these files.

| Control | Action |
| --- | --- |
| Mouse wheel | Zoom around the pointer |
| Left mouse drag | Pan |
| R | Fit image to window |
| M | Cycle overview / graphics blocks / sprites / world map |
| Left / Right | Previous / next SBB or SBS file; month in world view |
| V | World view: toggle tile 0x33 (`[Setup] Views`) |
| Escape | Exit |

Start directly in block view with `--tiles`, in sprite view with `--sprites` or
in world view with `--world [--month 0-11] [--no-views]`. The world view composes
`map/europe.map` from the season's SBB tiles using the rules recovered from
`rally.exe` (two layers, mirroring, three draw passes, colour key 0); see
docs/formats.md.
The sprite view decodes every frame of an SBS file; index 0 is shown as a
checkerboard (assumed transparency) and colours use the overview palette. Its grid is an inspection layout,
not the reconstructed world map. The overview loads `butt/1/map.bmp`.

## Verification and analysis

```sh
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /media/steam1/Games/Dtvr/ISOINHALT --smoke-test --screenshot build/overview.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /media/steam1/Games/Dtvr/ISOINHALT --tiles --smoke-test --screenshot build/tiles.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /media/steam1/Games/Dtvr/ISOINHALT --sprites --smoke-test --screenshot build/sprites.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /media/steam1/Games/Dtvr/ISOINHALT --world --smoke-test --screenshot build/world.bmp
ctest --test-dir build --output-on-failure
python3 tools/inspect_game.py /media/steam1/Games/Dtvr/ISOINHALT > build/game-analysis.json
```

The smoke test renders one frame and exits, including without a desktop session.
Screenshots use BMP format. The analysis reports file counts, the executable hash,
NE segments, imported modules and the entry point. It never executes Windows code.

`tools/ne_disasm.py` disassembles the NE code segments with `objdump` (binutils)
and annotates relocations and imported functions. Its output contains game code;
write it to the ignored `local/` directory.

See [docs/formats.md](docs/formats.md) for observations and next steps.
The agreed [architecture and modernization plan](docs/architecture.md) covers
LAN/Internet multiplayer, save files, usability and optional graphical effects.
These are planned features, not capabilities of the current viewer.
