# DTVR Recomb

![DTVR Recomb: an original road-and-die mark](assets/branding/logo.svg)

Native Linux reconstruction of **Die total verrückte Rallye** (Blue Byte).
Current project version: **0.0.1** ([release and AppImage policy](docs/releases.md)).
The first milestone is an SDL2 viewer for the original map and graphics blocks.
There is no playable game logic or executable recompilation yet.

## Project status

Current milestone: **native data inspection and installation**. A complete playable
turn is the next gameplay milestone. Status applies to the supported input version;
no overall completion percentage is claimed.

| Component | Status | Implemented / next step | Source or details |
| --- | --- | --- | --- |
| Source-only distribution | ✅ Implemented | User supplies the ISO; external installation, generic example paths and source policy checks. | [Policy](docs/distribution.md) |
| ISO installer and Linux build | ✅ Implemented | Graphical ISO/destination selection, preflight, automatic compilation/tests, Ogg preparation, progress/cancel and start button. Updates remain planned. | [GUI](tools/installer_gui.py), [pipeline](docs/installer.md) |
| Versioned setup AppImage | 🟡 Packaging implemented | Bundled Python/Tk and committed installer source; host build/audio dependencies are checked. Public releases and other-platform validation remain pending. | [Packaging and versions](docs/releases.md) |
| Original executable analysis | ✅ Tools available | NE headers, imports and disassembly tools; generated code stays private and external. | [Tools](tools/ne_disasm.py), [findings](docs/formats.md) |
| Native executable translation | ⬜ Not implemented | The current native viewer is our C++ code; original instruction translation is not implemented. | [Architecture](docs/architecture.md) |
| Sprite data | 🟡 Partial | SBS frames decoded; colour key 0 confirmed from the blitter. Animation sequencing, timing and palette selection still open. | [SBS parser](src/sbs.cpp) |
| World map and seasons | 🟡 Partial | Full tile map, layers, mirroring and seasonal tile selection; viewport and palette cycling still open. | [World map](src/worldmap.cpp) |
| Board and towns | 🟡 Partial | 700 fields, links, field lists and 115 towns decoded, with graph overlay; field actions and town entry meanings still open. | [Board parser](src/board.cpp) |
| Viewer input and display | ✅ Viewer implemented | Mouse zoom/pan, keyboard navigation and inspection views. Full game interface remains to be built. | [SDL viewer](src/main.cpp) |
| CD audio import | 🟡 Partial | Six provisional Ogg segments exported and verified; original track boundaries/IDs remain unverified. | [Audio importer](tools/cd_audio.py), [evidence](docs/cd-audio.md) |
| Native music, speech and effects | ⬜ Not implemented | Map original sound triggers to local assets; stream Ogg music without a disc. | [Playback plan](docs/cd-audio.md#native-playback-integration-next-work) |
| Complete Ogg sound sets | ✅ Offline preparation implemented | Render all 25 cues using supplied banks; classic/dry styles, catalog and verification. Runtime selection remains planned. | [Soundtrack builder](docs/soundtracks.md) |
| Movement rules | 🟡 Partial | Slot machine roll, field step counting with undo, original random generator, tile properties (`europe.pit`) and routes recovered and tested; move preview in the world view. Direction input and end of a move still open. | [Rules](src/rules.cpp), [evidence](docs/formats.md#movement-round-4) |
| Turn logic and original rules | 🟡 Partial | Playable hotseat rally (`--play`): London start, slot roll, field movement with undo, money fields, inflation, player change, months, random destinations, stages and game end — all from recovered rules. Event card deck with yellow-field draws; 26 of 41 card types playable (movement, Fiskus, Konto, Schulden, Zero, Zucker, Schlafen, Blockieren, Weihnachten, Hau ab, Erdbeben, Nägel, Punch, Konferenz, Flugzeug, Verdoppeln) plus four passive protection cards; Dr. Drago rides along with all six of his actions; properties can be bought and sold in towns (starting cash 30,000, monthly income, whole-town bonus, credit charged at the year end); random events are triggered and Eastern Europe stays behind border barriers until enough property is owned; robbers, road obstacles (cleared with Schneepflug) and property damage (protected by Versicherung / Nessie) work; the professor and football club offer investments that return double or nothing. The eight property cards, Steuerlos, Tschüss, Risiko, card fields and computer players are still open. | [Game](src/game.cpp), [Dr. Drago](src/drago.cpp), [events](src/events.cpp), [obstacles](src/obstacles.cpp), [cards](src/cards.cpp), [economy](src/economy.cpp), [evidence](docs/formats.md#professor-football-club-and-further-cards-round-13) |
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
5. Turn: arrow-key input, arrival only after the full allowance, field kinds
   (blue money, red cost, yellow event card, towns, card field), inflation.
6. Players, months, destinations and stages; first playable hotseat rally (`--play`).
7. Event cards: 41 types, 199-card deck with original shuffle, hands, yellow field,
   movement cards (2–5 Walzen, 1 oder 2, 1–6 Felder).
8. Properties: town state, purchases (normal, discount, rent, take, buy-out,
   credit), whole-town bonus and monthly income; starting cash 30,000.
9. Selling, credit at the year end, card targets, money and status cards
   (Fiskus, Konto, Schulden, Zero, Zucker, Schlafen, Blockieren).
10. Dr. Drago: placement at each stage, form changes, passing on meetings, six
    actions; cards Weihnachten and Hau ab.
11. Events: trigger and countdown, four progress-dependent event tables, Eastern
    Europe behind 15 border barriers until the players own enough property;
    robbers.
12. Event effects: rain, snow and rockfall block roads for 3–4 rounds (Schneepflug
    clears them); storm, drought, flood, Nessie, earthquake and volcano destroy
    uninsured properties in a town (Versicherung, Nessie protect).
13. Professor and football club investments; cards Erdbeben, Nägel, Punch,
    Konferenz, Flugzeug and Verdoppeln; play view split from the viewer.
14. *Next:* property cards, Steuerlos, Tschüss, Risiko, card fields, then
    computer players.

Build flow: **user ISO → validated importer/converters → external native
installation → local launcher**. The repository supplies the tools and source;
all original content comes from the user's ISO.

✅ = implemented within the stated scope; 🟡 = partial with known gaps;
⬜ = not implemented or planned. Update this table when a milestone is verified.

## Install from your original ISO

Start the native graphical assistant:

The planned release artifact is `DTVR-Recomb-Setup-0.0.1-x86_64.AppImage`.
It bundles Python/Tk and our source; users still supply their ISO. The installer
automatically compiles the game using checked host build dependencies. No release
download is published yet. For a source checkout, run:

```sh
./install.sh
```

Select your ISO, a new external installation folder and optionally a SoundFont.
Click **Installieren und kompilieren**: import, CMake configuration, compilation,
tests and final verification run automatically. The window shows progress and a
log, remains responsive, and enables **Spielprototyp starten** when finished.
No manual CMake commands are needed. The starter selects an installed Python
with working Tk support; CLI use does not require Tk.

The repository provides the native source and installer. You supply your own
original CD image; neither the ISO nor original game data is distributed here.
Requires Python 3, CMake, a C++17 compiler, pkg-config and SDL2 development files.

The assistant checks prerequisites before importing data and reports missing
packages/tools; it does not modify system packages or ask for root access.
FFmpeg/ffprobe are needed for known CD audio; SoundFont rendering also needs
FluidSynth. The equivalent CLI installation is:

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

Read-only preflight and later installation verification are available separately:

```sh
python3 tools/install.py --iso /path/to/game.iso --output /path/to/new/installation --check
python3 tools/installation_manifest.py /path/to/installation
```

## How to play

The current build is an **inspection viewer with a first playable hotseat rally**.
Start it with `play --play --players 2` (1–8 players, `--rounds N` stages,
`--seed N` for a reproducible game): press **W** to spin, the **arrow keys** to
drive field by field (the opposite direction takes a step back) and **Enter** to
end the turn after arriving. Before spinning, **1–8** plays the card in that
hand slot (26 of 41 card types are rebuilt; with three or more players **T**
chooses the target; Nägel waits for an arrow key, Flugzeug flies to the current
destination). When the professor or the football club makes an offer, **I**
invests half the cash (rounded to 1,000) and **N** declines. After stopping in a town the terminal lists its
properties; **1–9, 0** buy entry 1–10, **Shift+1–9, 0** sell it. Status
(including cash) is shown in the window title, the log and your hand in the
terminal. The other card effects, card fields, computer players, saves,
multiplayer and native music playback are not implemented yet.

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
| Right click | World view: choose the start field and print its field action |
| W / arrows / Enter | With `--play`: spin, drive one field, end the turn |
| 1–8 | With `--play`: play the hand card in that slot instead of spinning |
| 1–9, 0 | With `--play`, after stopping in a town: buy property 1–10 (with Shift: sell) |
| T | With `--play`: choose the card target player (three or more players) |
| I / N | With `--play`: accept / decline a professor or football club investment |
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
write it to a private directory outside the checkout. `tools/ne_strings.py`
dumps the string resources of `dat/d/rally_d.dat` (original text: keep private too).

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

The [project-authored SVG logo and icon](assets/branding/) use the same license.
They contain no original game artwork. PNG and ICO exports for packaging can be
generated outside the checkout with `tools/export_branding.py`.

The original game's ISO, executables, graphics, music, speech and other game data
are **not covered by this project's license** and are not distributed here.
Users must supply their own supported original ISO. Files imported or converted
from it, including Ogg music, remain original game content. Third-party libraries
and tools retain their respective licenses.
