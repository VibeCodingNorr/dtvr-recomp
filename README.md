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
| ISO installer and Linux build | ✅ Implemented | GUI selection, preflight, automatic compilation/tests, Ogg preparation, progress/cancel and game-window launch. Explicit local updates preserve user files and a complete backup. | [GUI](tools/installer_gui.py), [pipeline](docs/installer.md) |
| Versioned setup AppImage | 🟡 Packaging implemented | Bundled Python/Tk and committed installer source; host build/audio dependencies are checked. Public releases and other-platform validation remain pending. | [Packaging and versions](docs/releases.md) |
| Original executable analysis | ✅ Tools available | NE headers, imports and disassembly tools; generated code stays private and external. | [Tools](tools/ne_disasm.py), [findings](docs/formats.md) |
| Native executable translation | ⬜ Not implemented | The current native viewer is our C++ code; original instruction translation is not implemented. | [Architecture](docs/architecture.md) |
| Sprite data | 🟡 Partial | SBS frames decoded; colour key 0 confirmed from the blitter. Animation records and their exact step rule (seg2:6DCB) drive the finish, victory and all event scenes. Vehicle frames placed as the original sprite list (seg2:3EB0). | [SBS parser](src/sbs.cpp), [event scenes](src/event_scenes.cpp) |
| World map and seasons | 🟡 Partial | Full tile map, layers, mirroring and seasonal tile selection; the map origin is centred, clamped and edge-scrolled as in rally.exe, with zoom; the water cycles its palette as the original. | [World map](src/worldmap.cpp), [scrolling](src/map_scroll.cpp) |
| Board and towns | 🟡 Partial | 700 fields, links, field lists and 115 towns decoded, with graph overlay; field actions and town entry meanings still open. | [Board parser](src/board.cpp) |
| Viewer input and display | ✅ Viewer implemented | Mouse zoom/pan, keyboard navigation and inspection views; a debugging aid next to the game window (`--game`). | [SDL viewer](src/main.cpp) |
| Original game window | 🟡 Partial | `--game` (optionally `--resolution 640/800/1024`, F11 fullscreen): title, player setup, options, save / load dialog, menus of the language file, the canvas with map, portrait, info panel and slot machine, all at the original positions with art and text loaded from the installation. The message window shows the language texts with the showmaster, Dr. Drago, card, professor or club pictures and Yes / No questions (obstacle card, quit). Card window with SBBUTTON focus, press and GESPERRT overlay; target choice, Flugzeug booking, Nägel direction, investment offer, Risiko, card shop, town dialog ESTATES, statistics, overview, Umfeld and zoom; stage announcement, year balance, finish and victory sequences; every event plays its original animation at its place. Card films (FLC) play natively. Open: the boat transformation of vehicles at sea, remaining dialogs. | [Game window](src/game_screen.cpp), [flow](src/game_window.cpp), [layout](src/layout.cpp), [evidence](docs/formats.md#game-window-round-21) |
| CD audio import | 🟡 Partial | Six provisional Ogg segments exported and verified; original track boundaries/IDs remain unverified. | [Audio importer](tools/cd_audio.py), [evidence](docs/cd-audio.md) |
| Native music, speech and effects | 🟡 Partial | Ogg streaming shares the stereo effects mixer; startup, game, destination, winner, Drago and recovered event cues. F8 controls set/volume; remaining original transition boundaries and listening validation are open. | [Native music](docs/native-music.md), [effects](src/sound.cpp) |
| Complete Ogg sound sets | ✅ Preparation and selection implemented | Render all 25 cues using supplied banks; classic/dry styles, catalog validation, native discovery and F8 selection. | [Soundtrack builder](docs/soundtracks.md) |
| Movement rules | 🟡 Partial | Slot machine roll, field step counting with undo, original random generator, tile properties (`europe.pit`) and routes recovered and tested; move preview in the world view; arrow-key driving and the original mouse steering cross (MCTRL) and game controllers as the original joystick, with the step and arrival rules. | [Rules](src/rules.cpp), [evidence](docs/formats.md#movement-round-4) |
| Turn logic and original rules | 🟡 Partial | Playable hotseat rally (`--play`): London start, slot roll, field movement with undo, money fields, inflation, player change, months, random destinations, stages and game end — all from recovered rules. Event card deck with yellow-field draws; 37 of 41 card types playable (movement, Fiskus, Konto, Schulden, Zero, Zucker, Schlafen, Blockieren, Weihnachten, Hau ab, Erdbeben, Nägel, Punch, Konferenz, Flugzeug, Verdoppeln, Tschüss, Steuerlos, Risiko and the eight property cards in the town dialog) plus four passive protection cards; Dr. Drago rides along with all six of his actions; properties can be bought and sold in towns (starting cash 30,000, monthly income, whole-town bonus, credit charged at the year end); random events are triggered and Eastern Europe stays behind border barriers until enough property is owned; robbers, road obstacles (cleared with Schneepflug) and property damage (protected by Versicherung / Nessie) work; the professor and football club offer investments that return double or nothing. Computer players drive, buy, play and trade cards with the original strategy tables, Steuerlos hands a turn to them, and a single human gets a computer opponent as in the original. Nebel is unused in the original executable. Card fields work as a card shop (buy at the card value, sell for half; used cards return to the deck). | [Game](src/game.cpp), [Dr. Drago](src/drago.cpp), [events](src/events.cpp), [obstacles](src/obstacles.cpp), [cards](src/cards.cpp), [economy](src/economy.cpp), [evidence](docs/formats.md#risiko-nebel-and-the-player-setup-round-19) |
| AI, saves and hotseat | 🟡 Partial | Hotseat for 1–8 players; computer players with the original strategy tables (driving, buying, cards, card field); versioned native save format with exact round trip and atomic writes, the original save dialog with slots and an autosave at each turn. Driver names come from the language file at run time. Not yet: the computer answer to investment offers (they decline). | [AI](src/ai.cpp), [saves](docs/save-format.md), [save slots](src/save_slots.cpp) |
| LAN / Internet multiplayer | ⬜ Planned | Host authority, lobby and reconnection; LAN first, Internet later. | [Multiplayer](docs/architecture.md#multiplayer) |
| Modern controls and usability | ⬜ Planned | Rebinding, controllers, contextual help, route planning and event history. | [Backlog](docs/architecture.md#feature-backlog) |
| Optional graphics | 🟡 Partial | Window / fullscreen with whole-number pixel scaling and black borders (F11, `[Display]`); shaders, smoothing filters, separately scaled text and parallax are planned. | [Graphics](docs/architecture.md#optional-graphics) |
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
14. Property cards 1–8 in the town dialog, Tschüss; Steuerlos and Risiko traced.
15. Card fields as card shop; used cards return to the deck.
16. Computer players: route planning, end-field scores, town purchases, Steuerlos.
17. Computer card play: card choice and targets, card field, discarding.
18. Native versioned save format (`src/save.cpp`, [format](docs/save-format.md)).
19. Risiko, Nebel (unused in the original) and the single-player opponent.
20. Player slots as in `[Player]` P0–P7, games of computers until stopped,
    driver names from the language file.
21. Game window frame as in the original (`--game`): canvas, map window,
    portrait and player info panel at the original positions for 640/800/1024.
22. Player info panel contents (month, cash, destination, digits, field kind) with
    Arial/Liberation Sans text and the original digit sprites.
23. Slot machine over the map with spinning and stopping reels and the sum
    display at the original positions; animation format decoded.
24. Vehicles of the drivers on the map with their facing and driving animation.
25. Original message window with language texts for the solo opponent, the
    yellow field and the events; East opening random draw corrected.
26. Messages of Dr. Drago, sit-outs, blocked cards, Christmas and investments;
    two missing random draws corrected.
27. Card messages (Fiskus, Konto, Schulden, Zero, Konferenz, n Felder, Erdbeben).
28. Menu window with the original menus ONFIELD, RIGHTBTN and MAIN1 (RT_MENU reader).
29. Card window with the original card pictures and layout.
30. Town dialog ESTATES: property list with owner, price, income and yield; buying
    and selling by mouse.
31. Smooth driving at the original speed with held arrow keys; scrolling rule of
    the town dialog.
32. Computer turns drive field by field on the map (same draws as before).
33. Save dialog as in the original (nine slots, native save files in the user's data
    directory); rules revision 26.
34. Statistik (the statistics variant of the town dialog) and `--load <slot>`.
35. Title menu MAIN with the original title picture: new game, load, continue from
    the autosave of each turn start.
36. Player setup PLAYER: drivers as free / human / computer, names, start.
37. Übersicht (overview map with route and flags) and Umfeld (looking around).
38. Zoom (Zoom entries, Insert / Delete): half the world area at twice the size.
39. Options window with the original switches; settings stored in the user's
    configuration (Views applied to the map).
40. Sound effects from snd/fx through SDL audio (menus, slot machine, money fields,
    sit-outs, blocked cards).
41. Driving sounds, Dr. Drago's laugh, pause keys 1–4 with their voice memos, Credits.
42. Demo from the title: four computer drivers as in the original; Escape or a
    right click return to the title.
43. Stage end as in the original: the missing random draws and the result texts
    (save format 4, rules revision 27).
44. Year balance: its fixed random draws (rules revision 28); timing-dependent draws
    of the original cannot be reproduced.
45. Finish sequence ZIEL at each destination: town photo, banner, band, crowd and the
    driver's car with the original animations and sound.
46. Victory sequence SIEG after the Master-Rallye: podium with the three best drivers
    by cash and property value, crowd, band and speaker.
47. Year balance show BILANZ: the show master pulls the lever for every driver and
    announces the result (positive or negative year) with the original voices.
48. The game window's flow moved to its own module (src/game_window.cpp).
49. Destination announcement ("Zielsuche") with the light searching the map and the
    original voices; the next destination is chosen as in the original (rules revision 30).
50. Sounds of the town and save dialogs.
51. Weihnachten card gift sound; the card shop window decoded for the next round.
52. Card shop of the violet card fields in the game window: buying and selling cards
    with the original card pictures.
53. Target choice SELPLAYER for cards with a target (Fiskus, Schulden, Zucker,
    Blockieren, Schlafen, Tschüss) in the game window.
54. Flugzeug booking window with the original towns and the fog over Brussels.
55. Nägel direction window: the four arrows of the original as click areas.
56. Investment offer window (professor / football club) with Ja / Nein and the amount.
57. Yellow field with a full hand: the card window opens in discard mode after text 403.
58. Risiko window: the light frames and pressed buttons of risiko.sbs, CHANCE / STOP by mouse.
59. Card window masks: only the cards of the current category react (seg3:0701).
60. SBBUTTON decoded (seg1:0E78): disabled cards show the original GESPERRT overlay.
61. SBBUTTON input: focus frame, pressed offset, release-to-play, arrow keys and sounds.
62. Shared SBBUTTON group for the card window, SELPLAYER, OPTION and PLAYER (focus, press, keys, sounds).
63. Window / fullscreen with pixel scaling (F11, Alt+Enter, `--fullscreen`, `[Display]`).
64. Risiko texts 41001-41003 and press sounds as in seg7:69DE ff.
65. Obstacle question: Yes / No message window with the card picture before a clearing card is used.
66. Quit question (text 700, Yes / No) for Rallye beenden and the key Q.
67. Map scrolling as rally.exe: centring on turn, spin, Space and zoom; edge jumps while driving.
68. Event places: the map zooms in on the place of an event while its message is open.
69. Event animation framework and the storm (sturm.sbs) before its message.
70. Exact animation record step (seg2:6DCB); robbers, volcano and rockfall animations.
71. Earthquake animation: two cracks per place from the original tables, waits before and after.
72. Rule correction: the random(3) scene start of rain, snow and flood (seg8:517B), no storm / drought / flood without a town.
73. Rain, snow and flood animations: the cloud flies in from the drawn corner, then plays at the place.
74. Drought (sinking sun) and Nessie (Loch Ness, then the lake of the place): all event animations rebuilt.
75. Nägel fall into the water on a seaway (text 5500, no obstacle; rules revision 32).
76. Dr. Drago's messages show his portrait (qp.bmp); the block action shows text 3351.
77. Message pictures: professor / football club on investment results, the gifted card at Christmas.
78. Mouse steering: the original MCTRL cross opens on a map click and posts the chosen direction.
79. Controllers: any SDL game controller acts as the original joystick 1 (d-pad / stick, A = Return, B = Escape, 80 ms repeat).
80. Shut-in players: the move is shortened to what is reachable or the turn ends (seg2:61B7, texts 6003-6011).
81. Message coverage checked: every in-game language message of rally.exe is rebuilt.
82. Colour cycling: the sea ripples by rotating palette entries 0x90-0x99 every 50 ms; Colorcycle off keeps the summer tiles.
83. Card films: native FLC decoder, the Zucker, Schlafen, Nägel and Punch films with their sounds.
84. Sprite list offset found (seg2:3EB0: -16, -16): vehicles and event animations at their original places.
85. *Next:* the boat transformation of vehicles on sea routes (+0x2D / +0x2E) and remaining presentation details.

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

Select your ISO, an external installation folder and optionally a SoundFont.
Click **Installieren und kompilieren**: import, CMake configuration, compilation,
tests and final verification run automatically. The window shows progress and a
log, remains responsive, and enables **Spiel starten** when finished. The start
button and the installed launcher without arguments open the game window (`--game`).
No manual CMake commands are needed. The starter selects an installed Python
with working Tk support; CLI use does not require Tk.

The repository provides the native source and installer. You supply your own
original CD image; neither the ISO nor original game data is distributed here.
Requires Python 3, CMake, a C++17 compiler, pkg-config, SDL2, SDL2_ttf,
libvorbisfile and json-c >= 0.15 development files.
Install a complete Arial or Liberation Sans font family for text in the game
window. The installer checks all four faces and SDL2_ttf before building.

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
rejected. Existing destinations require explicit update mode. Original Windows
executables and runtime DLLs are not part of the resulting native installation.
The ISO can be disconnected after installation. The launcher opens the native
game interface; this remains an incomplete reconstruction. Original instruction
translation is not implemented. See [installer details and updates](docs/installer.md).

For the known mixed-mode image, installation also converts six recovered audio
regions to Ogg Vorbis in `data/music/` (requires FFmpeg/libvorbis and ffprobe).
Original CD track IDs remain unverified. These six recordings alone do not supply
a complete soundtrack. Build a complete Ogg pack with a supplied SoundFont for
[native playback and F8 selection](docs/native-music.md).

Read-only preflight and later installation verification are available separately:

```sh
python3 tools/install.py --iso /path/to/game.iso --output /path/to/new/installation --check
python3 tools/installation_manifest.py /path/to/installation
```

## How to play

**F8** opens music controls from the game/title/options menus: Up/Down selects a
complete installed Klangset, Left/Right adjusts volume, Space toggles music and
Escape returns. Set changes apply at the next piece; settings persist locally.

**Game controllers** (Xbox and other pads) work like the original joystick while the
option Joystick is on: d-pad or left stick steer and move through menus, A confirms
(Return), B goes back (Escape).

**F11** or **Alt+Enter** switches the game window between window and fullscreen
(`--fullscreen` starts in fullscreen). The original canvas (640 × 480, 800 × 600 or
1024 × 768) is scaled by whole numbers with black borders; the window can be resized.
`[Display]` in `settings.ini` keeps `Fullscreen` and `IntegerScale` (0 stretches to
the largest fitting size with the canvas proportions).

Run the installed `play` launcher without arguments (or use **Spiel starten** in
the installer) for the game window with its title menu. `play --game` also opens
it explicitly. The older `--play` mode below remains useful for debugging.

The current build is an **inspection viewer with a first playable hotseat rally**.
Start it with `play --play --players 2` (humans, `--computers N` adds computer
players, together 1–8; one human alone gets a computer opponent; `--rounds N` stages,
`--seed N` for a reproducible game): press **W** to spin, the **arrow keys** to
drive field by field (the opposite direction takes a step back) and **Enter** to
end the turn after arriving. Before spinning, **1–8** plays the card in that
hand slot (35 of 41 card types are rebuilt; with three or more players **T**
chooses the target; Nägel waits for an arrow key, Flugzeug flies to the current
destination). When the professor or the football club makes an offer, **I**
invests half the cash (rounded to 1,000) and **N** declines. After stopping in a town the terminal lists its
properties; **1–9, 0** buy entry 1–10, **Shift+1–9, 0** sell it and
**Ctrl+1–8** plays a property card (Kaufen, Gold, Platin, Mieten, Nimm,
Abkaufen, Kredit, Verkaufen) whose mode lasts until the turn ends. Status
(including cash) is shown in the window title, the log and your hand in the
terminal. The game-window controls and current reconstruction gaps are documented
in the status table; multiplayer remains planned. Ogg music is available in `--game`.

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
| Ctrl+1–8 | With `--play`, after stopping in a town: play a property card |
| T | With `--play`: choose the card target player (three or more players) |
| I / N | With `--play`: accept / decline a professor or football club investment |
| Escape | Exit (with `--game`: open the menu `RIGHTBTN`; quit through Hauptmenü → Rallye beenden) |

To inspect a different starting view, pass `--tiles` or `--sprites` to `play`.
For a specific seasonal view, use `play --world --month 0` (months are 0–11).
Use `play --help` for the complete command-line options.

## Build and run

Requires a C++17 compiler, CMake, pkg-config and SDL2 development files
(optional: SDL2_ttf with Arial or Liberation Sans for text in the game window).
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
