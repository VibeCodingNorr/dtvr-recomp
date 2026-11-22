# DTVR Recomb

Native Linux reconstruction of **Die total verrückte Rallye** (Blue Byte).
The first milestone is an SDL2 viewer for the original map and graphics blocks.
There is no playable game logic or executable recompilation yet.

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
| M | Cycle overview / graphics blocks / sprites |
| Left / Right | Previous / next SBB or SBS file |
| Escape | Exit |

Start directly in block view with `--tiles` or in sprite view with `--sprites`.
The sprite view decodes every frame of an SBS file; index 0 is shown as a
checkerboard (assumed transparency) and colours use the overview palette. Its grid is an inspection layout,
not the reconstructed world map. The overview loads `butt/1/map.bmp`.

## Verification and analysis

```sh
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /media/steam1/Games/Dtvr/ISOINHALT --smoke-test --screenshot build/overview.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /media/steam1/Games/Dtvr/ISOINHALT --tiles --smoke-test --screenshot build/tiles.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /media/steam1/Games/Dtvr/ISOINHALT --sprites --smoke-test --screenshot build/sprites.bmp
ctest --test-dir build --output-on-failure
python3 tools/inspect_game.py /media/steam1/Games/Dtvr/ISOINHALT > build/game-analysis.json
```

The smoke test renders one frame and exits, including without a desktop session.
Screenshots use BMP format. The analysis reports file counts, the executable hash,
NE segments, imported modules and the entry point. It never executes Windows code.

See [docs/formats.md](docs/formats.md) for observations and next steps.
The agreed [architecture and modernization plan](docs/architecture.md) covers
LAN/Internet multiplayer, save files, usability and optional graphical effects.
These are planned features, not capabilities of the current viewer.
