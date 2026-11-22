# DTVR Recomb

Native Linux reconstruction of **Die total verrückte Rallye** (Blue Byte).
The first milestone is an SDL2 viewer for the original map and graphics blocks.
There is no playable game logic or executable recompilation yet.

## Project status

Current milestone: **native data inspection and installation**. A complete playable
turn is the next gameplay milestone. Status applies to the supported input version;
no overall completion percentage is claimed.

| Component | Status | Implemented / next step | Source or details |
| --- | --- | --- | --- |
| Source-only distribution | ✅ Implemented | User supplies the ISO; external installation, generic example paths and source policy checks. | [Policy](docs/distribution.md) |
| ISO installer and Linux build | ✅ Implemented | Read standard/raw Mode 1 ISO, validate supported input, build/test/install and create a launcher. GUI and updates remain planned. | [Installer](tools/install.py), [pipeline](docs/installer.md) |
| Original executable analysis | ✅ Tools available | NE headers, imports and disassembly tools; generated code stays private and external. | [Tools](tools/ne_disasm.py), [findings](docs/formats.md) |
| Native executable translation | ⬜ Not implemented | The current native viewer is our C++ code; original instruction translation is not implemented. | [Architecture](docs/architecture.md) |
| Sprite data | 🟡 Partial | SBS frames decoded; colour key 0 confirmed from the blitter. Animation sequencing, timing and palette selection still open. | [SBS parser](src/sbs.cpp) |
| World map and seasons | 🟡 Partial | Full tile map, layers, mirroring and seasonal tile selection; viewport and palette cycling still open. | [World map](src/worldmap.cpp) |
| Board and towns | 🟡 Partial | 700 fields, links, field lists and 115 towns decoded, with graph overlay; field actions and town entry meanings still open. | [Board parser](src/board.cpp) |
| Viewer input and display | ✅ Viewer implemented | Mouse zoom/pan, keyboard navigation and inspection views. Full game interface remains to be built. | [SDL viewer](src/main.cpp) |
| CD audio import | 🟡 Partial | Six provisional Ogg segments exported and verified; original track boundaries/IDs remain unverified. | [Audio importer](tools/cd_audio.py), [evidence](docs/cd-audio.md) |
| Native music, speech and effects | ⬜ Not implemented | Map original sound triggers to local assets; stream Ogg music without a disc. | [Playback plan](docs/cd-audio.md#native-playback-integration-next-work) |
| Movement rules | 🟡 Partial | Slot machine roll, field step counting with undo, original random generator, tile properties (`europe.pit`) and routes recovered and tested; move preview in the world view. Direction input and end of a move still open. | [Rules](src/rules.cpp), [evidence](docs/formats.md#movement-round-4) |
| Turn logic and original rules | ⬜ Not implemented | Field effects on arrival, then one complete human turn before broadening mechanics. | [Format research](docs/formats.md) |
| AI, saves and hotseat | ⬜ Planned | Build on the authoritative simulation, explicit actions and versioned game state. | [Architecture](docs/architecture.md) |
| LAN / Internet multiplayer | ⬜ Planned | Host authority, lobby and reconnection; LAN first, Internet later. | [Multiplayer](docs/architecture.md#multiplayer) |
| Modern controls and usability | ⬜ Planned | Rebinding, controllers, contextual help, route planning and event history. | [Backlog](docs/architecture.md#feature-backlog) |
| Optional graphics | ⬜ Planned | Pixel scaling, shaders and restrained parallax; effects remain optional. | [Graphics](docs/architecture.md#optional-graphics) |
| Automated validation | ✅ Implemented | Synthetic parser/installer tests and repository policy workflow; local ISO integration checks. | [Tests](tests), [CI](.github/workflows/source-policy.yml) |

Reconstruction rounds so far (evidence with addresses in [docs/formats.md](docs/formats.md)):

1. SBS sprite container decoded (all 94 files, 1,559 frames).
2. World map composed with the original loader and draw rules; transparency confirmed.
3. Board decoded: field graph from `europe.str`, towns, field lists.
4. Movement: slot machine, step counter, `europe.pit` tile flags, rand(), routes.
5. *In progress:* direction input, end of a move and field effects — the first
   complete human turn.

Build flow: **user ISO → validated importer/converters → external native
installation → local launcher**. The repository supplies the tools and source;
all original content comes from the user's ISO.

✅ = implemented within the stated scope; 🟡 = partial with known gaps;
⬜ = not implemented or planned. Update this table when a milestone is verified.

## Install from your original ISO

The repository provides the native source and installer. You supply your own
original CD image; neither the ISO nor original game data is distributed here.
Requires Python 3, CMake, a C++17 compiler, pkg-config and SDL2 development files.

```sh
python3 tools/install.py --iso /path/to/game.iso
~/.local/share/dtvr-recomb/play
```

The installer reads the ISO without mounting it, extracts the `program` tree,
validates the known original executable, builds and tests the native code, and
creates a local installation with a launcher and hash manifest. It supports
2048-byte ISO9660 images and raw Mode 1 images with 2352-byte sectors.
No root access or Windows installer is needed.

Use `--output /path/to/new/directory` to choose another destination and `--jobs N`
to limit build parallelism. The default is `$XDG_DATA_HOME/dtvr-recomb`, falling
back to `~/.local/share/dtvr-recomb`. Destinations inside the source checkout are
rejected. Existing destinations are never updated or overwritten. Original Windows
executables and runtime DLLs are not part of the resulting native installation.
The ISO can be disconnected after installation. The installed application is
currently the data viewer; extracting the ISO does not yet translate its executable
or produce a playable reconstruction. See [installer details](docs/installer.md).

For the known mixed-mode image, installation also converts six recovered audio
regions to Ogg Vorbis in `data/music/` (requires FFmpeg/libvorbis and ffprobe).
Original track IDs and playback triggers still need reconstruction; the viewer
does not yet play music. The intended native game plays local Ogg assets in place
of original CD playback. See [CD audio and playback plan](docs/cd-audio.md).

## How to play

The current build is an **inspection viewer**, not a playable rally yet. You can
explore the reconstructed world and board graph and preview a move (press **W**
in the world view); full turns, AI opponents, saves,
multiplayer and native music playback are not implemented.

1. Have your own supported original ISO and install the dependencies listed above.
2. Build an installation in a new directory outside this checkout:

   ```sh
   python3 tools/install.py --iso /path/to/game.iso --output /path/to/new/installation
   ```

3. Open the world map with its board overlay:

   ```sh
   /path/to/new/installation/play --world --graph
   ```

The paths above are placeholders: use your ISO and your chosen installation
folder. Start `play` without arguments for the overview. You can disconnect the
ISO after installation; subsequent launches use the installed local data.

| Control | Action |
| --- | --- |
| Mouse wheel | Zoom around the pointer |
| Left mouse drag | Pan |
| R | Fit image to window |
| M | Cycle overview / graphics blocks / sprites / world map |
| Left / Right | Previous / next SBB or SBS file; month in world view |
| V | World view: toggle tile 0x33 (`[Setup] Views`) |
| G | World view: show board fields (coloured by kind), links and towns |
| W | World view: spin the slot machine (1 reel, 1–6) and mark fields a move of exactly that many steps can end on |
| Right click | World view: choose the start field |
| Escape | Exit |

To inspect a different starting view, pass `--tiles` or `--sprites` to `play`.
For a specific seasonal view, use `play --world --month 0` (months are 0–11).
Use `play --help` for the complete command-line options.

## Build and run

Requires a C++17 compiler, CMake, pkg-config and SDL2 development files.
Python 3 is used for the analysis tool. No Python packages are required.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/dtvr-recomb --data /path/to/extracted
```

`--data` accepts either the CD root or its `program` directory. Supply your own
original files; game assets and executables are not included in this repository.
The viewer only reads these files.

Start directly in block view with `--tiles`, in sprite view with `--sprites` or
in world view with `--world [--month 0-11] [--no-views] [--graph] [--roll] [--seed N]`. The world view composes
`map/europe.map` from the season's SBB tiles using the rules recovered from
`rally.exe` (two layers, mirroring, three draw passes, colour key 0); see
docs/formats.md.
The sprite view decodes every frame of an SBS file; index 0 is shown as a
checkerboard (assumed transparency) and colours use the overview palette. Its grid is an inspection layout,
not the reconstructed world map. The overview loads `butt/1/map.bmp`.

## Verification and analysis

```sh
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /path/to/extracted --smoke-test --screenshot /path/to/artifacts/overview.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /path/to/extracted --tiles --smoke-test --screenshot /path/to/artifacts/tiles.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /path/to/extracted --sprites --smoke-test --screenshot /path/to/artifacts/sprites.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /path/to/extracted --world --smoke-test --screenshot /path/to/artifacts/world.bmp
ctest --test-dir build --output-on-failure
python3 tools/inspect_game.py /path/to/extracted > /path/to/artifacts/game-analysis.json
```

The smoke test renders one frame and exits, including without a desktop session.
Screenshots use BMP format. The analysis reports file counts, the executable hash,
NE segments, imported modules and the entry point. It never executes Windows code.

`tools/ne_disasm.py` disassembles the NE code segments with `objdump` (binutils)
and annotates relocations and imported functions. Its output contains game code;
write it to a private directory outside the checkout.

Run `python3 tools/check_repository.py` to check the tracked source distribution.
CI checks for bundled media/binaries and machine-specific data paths. See the
[distribution policy](docs/distribution.md). Tests use synthetic inputs; building
and testing the source does not require original assets.

See [docs/formats.md](docs/formats.md) for observations and next steps.
The agreed [architecture and modernization plan](docs/architecture.md) covers
LAN/Internet multiplayer, save files, usability and optional graphical effects.
These are planned features, not capabilities of the current viewer.

## License

Our original source code, tools and tests are licensed under **GPL-3.0-only**
(GNU General Public License, version 3 only). See [LICENSE](LICENSE) for the full
license text. This project does not grant an option to use later GPL versions.

The original game's ISO, executables, graphics, music, speech and other game data
are **not covered by this project's license** and are not distributed here.
Users must supply their own supported original ISO. Files imported or converted
from it, including Ogg music, remain original game content. Third-party libraries
and tools retain their respective licenses.
