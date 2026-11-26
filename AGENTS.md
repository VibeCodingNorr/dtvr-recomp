# Working on DTVR Recomp

## Objective and current state

Reconstruct Die total verrückte Rallye as a native Linux game using the user's
original installation. Repository: https://github.com/VibeCodingNorr/dtvr-recomp.
Communicate with the user in German. Code and technical documentation may use English.

## Parallel work with Claude

- Choose tasks that do not block Claude's active engine work. Prefer isolated
  installer/packaging modules, documentation or external validation.
- Inspect ongoing changes before choosing scope. Do not edit Claude's active
  modules or shared integration files as part of parallel feature work. Prepare
  any required integration separately for handoff instead.

The C++17 / SDL2 executable is a manual reconstruction of rally.exe, not a
translation of its instructions. Its main mode is the game window (`--game`, the
installed launcher's default): Blue Byte logo, intro, title menu, player setup and
stage count, then the rally with the recovered rules and the original windows,
menus, dialogs, messages, scenes, films, sounds and music, all art and text loaded
from the user's installation at run time. The SDL-independent simulation
(src/game*.cpp with src/cards, economy, drago, events, obstacles, rules and the
computer players in src/ai*.cpp) also drives the terminal hotseat mode `--play`
(src/play_view.cpp). Games are saved in the native versioned format (src/save.cpp,
docs/save-format.md) through the original save dialog, one save directory per
edition; loading repeats rally.exe's loader (src/game_load.cpp). The German and
the US English retail editions are installable and playable. Recomp additions:
the Escape menu (docs/pause-menu.md), game controllers with an on-screen keyboard
and remappable pad buttons, window / fullscreen scaling, widescreen, text
smoothing and animation tempo. The viewer modes (`--tiles`, `--sprites`,
`--world`) remain debugging aids. Open: remaining presentation and timing details
of single windows compared with rally.exe, the gaps listed in the README status
table and docs/formats.md, multiplayer (after the port) and any translation of
original instructions.

## Source-only distribution and user input

- License our original source, tools and tests as `GPL-3.0-only`. Preserve the
  SPDX identifier in source headers and the unmodified GNU GPL v3 text in `LICENSE`.
  The project license does not cover original game data or third-party dependencies.

- Our own SVG branding in `assets/branding/` is permitted and uses GPL-3.0-only.
  Keep it independent of original art. Export PNG/ICO with `tools/export_branding.py`
  into an external directory; never relax the original-asset checks for these exports.

- Users supply an original ISO explicitly through `--iso` or a complete original
  game directory through `--directory`. Validate an EXE hash of a supported edition
  (`tools/editions.py`, mirrored in `src/edition.cpp`) and record
  directory input files with SHA-256; never accept an incomplete installed copy
  solely because its EXE matches. All examples use
  generic placeholders; never commit machine-specific paths or rely on a local
  installation, mounted disc, download URL or symlink to original game content.
- Distribution model: ship our installer/toolchain; users supply their original data.
  Use `python3 tools/install.py --iso /path/to/dtvr.iso` to build a local installation.
  It builds and installs the native game (`play` opens `--game`): a manual
  reconstruction, not a translated executable.
- Treat original files as read-only. Accept paths through `--data`; do not hardcode
  this machine's path into the runtime.
- Keep original executables, assets, extracted game content, screenshots containing
  game art and generated analysis output out of the source checkout. Installation
  defaults to the user's XDG data directory and rejects targets inside the checkout.
  Keep developer media and disassembly in an external private directory too.
  The repository contains our source and technical format observations only.
  Run `python3 tools/check_repository.py` before committing; CI enforces this policy.
- Keep the README's project-status table current after verified milestones. Separate
  implemented, partial and planned systems, with concrete gaps and links to modules.
  Do not present decoded data or a native viewer as a playable recompilation.

## Build and verify

- `VERSION` is the single project version (currently `0.0.1`). Installer UI,
  installation manifest, CMake executable and AppImage filename use it.
- Increase the numeric MAJOR.MINOR.PATCH version for each release, not each commit.
  Release tags must be `v<VERSION>` and newer than all previous release versions.
  See `docs/releases.md`; never bundle user media or SoundFonts in setup artifacts.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/dtvr-recomp --data /path/to/extracted
SDL_VIDEODRIVER=dummy ./build/dtvr-recomp --data /path/to/extracted --smoke-test --screenshot /path/to/artifacts/overview.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomp --data /path/to/extracted/program --tiles --smoke-test --screenshot /path/to/artifacts/tiles.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomp --data /path/to/extracted --sprites --smoke-test --screenshot /path/to/artifacts/sprites.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomp --data /path/to/extracted --world --smoke-test --screenshot /path/to/artifacts/world.bmp
ctest --test-dir build --output-on-failure
python3 tools/ne_disasm.py /path/to/extracted/program/rally.exe /path/to/private-analysis --use32 9 --dll-dir /path/to/extracted/program --dll-dir /path/to/extracted/wing --dll-dir /path/to/optional-system-dlls
python3 tools/inspect_game.py /path/to/extracted > /path/to/artifacts/game-analysis.json
```

Requires CMake, pkg-config, a C++17 compiler, SDL2 >= 2.0.10, libvorbisfile and
json-c >= 0.15 development files and Python 3. Optional: SDL2_ttf with Arial or Liberation Sans for text in the game
window (`--game`); without it the window is built without text. The analysis script uses only the Python standard library.
Inspect externally stored screenshots when changing rendering. Some image viewers require conversion
from BMP to PNG; Pillow is available on the current machine, but is not a build dependency.
The dummy driver verifies rendering and file loading, not interactive desktop input.

## Project layout

- `src/main.cpp`: native SDL viewer, command-line handling, image loading.
- `src/sbs.{hpp,cpp}`: bounds-checked SBSF sprite container parser (no SDL).
- `src/worldmap.{hpp,cpp}`: europe.map cells, season table, SBB tile split and
  map composition following rally.exe (no SDL).
- `src/game.{hpp,cpp}`: SDL-independent hotseat game built from the recovered rules;
  `src/game_load.cpp` what rally.exe's loader recomputes after reading a save.
- `src/ai.{hpp,cpp}`: computer strategy tables, route planning and town plan (no SDL);
  `src/ai_cards.cpp` card choice, targets, card worth and card field; `src/game_ai.cpp`
  runs computer turns.
- `src/game_risiko.cpp`: the Risiko card as a timed game phase.
- `src/save.{hpp,cpp}`: native versioned save format and atomic file writes, with the
  edition record of format 8 (no SDL).
- `src/edition.{hpp,cpp}` (no SDL): edition profiles by the SHA-256 of rally.exe or the
  installation manifest, required files and known demo builds.
- `src/ne_strings.{hpp,cpp}`: RT_STRING and RT_MENU reader for the language file and Windows-1252 conversion.
- `src/layout.{hpp,cpp}`: original window geometry; `src/panel.{hpp,cpp}`: player info panel
  contents (both no SDL).
- `src/data_paths.{hpp,cpp}` (no SDL): case-insensitive paths and the `dat/lan.inf` language letter; `src/sha256.{hpp,cpp}` (no SDL): the SHA-256 behind the edition check;
  `src/sdl_assets.{hpp,cpp}`: BMP/world loading; `src/text.{hpp,cpp}`:
  GDI-like text through optional SDL2_ttf; `src/game_screen.{hpp,cpp}`: the game window (`--game`)
  and `src/game_window.{hpp,cpp}` its flow (class GameWindow: set-up, title, menu commands, loop) with
  `src/game_window_frame.cpp` (per frame), `src/game_window_input.cpp` (input order) and
  `src/game_window_dialogs.cpp` (game dialogs);
  `src/slot_view.{hpp,cpp}`: the slot machine over the map with `src/slot_records.{hpp,cpp}` (no SDL) its animation records; `src/message_view.{hpp,cpp}`: the
  message window (texts from `Game::takeNotices`); `src/menu_view.{hpp,cpp}`: the menu window
  (menu resources of the language file); `src/card_view.{hpp,cpp}`: the card window and `src/card_window_choice.{hpp,cpp}` (no SDL) its play path;
  `src/estates.{hpp,cpp}` (no SDL) and `src/estates_view.{hpp,cpp}`: the town dialog;
  `src/save_slots.{hpp,cpp}` (no SDL) and `src/save_view.{hpp,cpp}`: the save dialog, slots and
  per-edition save directories; `src/scripted_input.{hpp,cpp}`: `--press` key scripts (smoke tests,
  with `Hold:`, `Release:` and `Wait`);
  `src/stat_sheet.{hpp,cpp}` (no SDL) and `src/stat_view.{hpp,cpp}`: the monthly statement STAT,
  `src/game_statement.cpp` its pause in the game and `src/game_window_statement.cpp` in the window;
  `src/setup_view.{hpp,cpp}`: the player setup (geometry in layout.cpp);
  `src/stages_view.{hpp,cpp}`: the stage count window TEMP with the slider;
  `src/overview_view.{hpp,cpp}`: the overview map; `src/recomp_menu.{hpp,cpp}` (no SDL) and
  `src/recomp_menu_view.{hpp,cpp}`: the recomp Escape menu (docs/pause-menu.md);
  `src/display_control.{hpp,cpp}`: window, fullscreen, scaling keys and screenshots;
  `src/osk.{hpp,cpp}` (no SDL) and `src/osk_view.{hpp,cpp}`: SBINPUT name rules and the recomp
  on-screen keyboard for names, `src/osk_pad.{hpp,cpp}` (no SDL) its pad feel (repeat, stick hysteresis); `src/stage_count.{hpp,cpp}` (no SDL): the TEMP count and its key steps; `src/settings.{hpp,cpp}` (no SDL) and
  `src/options_view.{hpp,cpp}`: the [Setup] options and their window; `src/sound.{hpp,cpp}`:
  sound effects (cues from `Game::takeSounds`) and `src/audio_mix.{hpp,cpp}` (no SDL) the mixing rule of one sample;
  `src/presentation_clock.{hpp,cpp}` (no SDL): the animation tempo clock; `src/busy_cursor.hpp`: the system wait cursor while loading; `src/finish_view.{hpp,cpp}`: the finish sequence;
  `src/victory_view.{hpp,cpp}`: the victory; `src/finale_scenes.{hpp,cpp}` (no SDL): their records and passes; `src/record_canvas.{hpp,cpp}`: their DIB, window image and SBS slots over `src/record_paint.{hpp,cpp}` (no SDL); `src/balance_view.{hpp,cpp}`: the year balance with `src/balance_players.{hpp,cpp}` (no SDL) its players' part and `src/balance_closing.{hpp,cpp}` (no SDL) its closing part and `src/balance_board.{hpp,cpp}` (no SDL) / `src/balance_board_view.cpp` its board;
  `src/stage_view.{hpp,cpp}`: the destination announcement; `src/card_shop_view.{hpp,cpp}`: the card shop;
  `src/select_player_view.{hpp,cpp}`: the target choice; `src/flight_view.{hpp,cpp}`: the Flugzeug booking;
  `src/nails_view.{hpp,cpp}`: the Nägel direction; `src/invest_view.{hpp,cpp}`: the investment offer;
  `src/risiko_view.{hpp,cpp}`: the Risiko window; `src/window_border.{hpp,cpp}`: the wooden non-client border; `src/sb_button.{hpp,cpp}`: the SBBUTTON
  button group (focus, press, disabled, keys) of the button windows; `src/map_scroll.{hpp,cpp}`
  (no SDL): centring, clamping and edge scrolling of the map origin; `src/event_scenes.{hpp,cpp}`
  (no SDL): event animation table and timing; `src/mouse_control_view.{hpp,cpp}`: the MCTRL
  mouse steering cross; `src/controller.{hpp,cpp}`: game controllers as the original joystick with `src/pad_input.{hpp,cpp}` (no SDL calls) its key logic and the recomp pad layer, `src/pad_map.{hpp,cpp}` (no SDL) the pad button remapping ([Gamepad]) and `src/game_window_pad.cpp` its context in the game window; `src/flc.{hpp,cpp}` (no SDL): FLC film decoder;
  `src/film_view.{hpp,cpp}`: the FLIC window; `src/sight_view.{hpp,cpp}`: the Tourism sights window; `src/voice_sync.{hpp,cpp}` (no SDL): sync.dat subtitle timing; `src/subtitle_view.{hpp,cpp}`: the SUBTITLE window; `src/intro_script.{hpp,cpp}` (no SDL): the start intro state machine; `src/ambient.{hpp,cpp}` (no SDL): the ambient map animations; `src/intro_view.{hpp,cpp}`: the INTRO window; `src/logo_view.{hpp,cpp}`: the Blue Byte logo (PICT) while loading; `src/vehicle_air.{hpp,cpp}` (no SDL): the plane transformation on sea routes; `src/vehicle_anim.{hpp,cpp}` (no SDL): the vehicles' frame animation; `src/drago_vehicle.{hpp,cpp}` (no SDL): Dr. Drago's own vehicle following his holder;
  `src/scene.{hpp,cpp}` and `src/scene_record.hpp`: their animation records.
- `src/game_cards.cpp`: playing hand cards and the investment offer; `src/play_view.{hpp,cpp}`:
  SDL keys, title and map marks of the play mode.
- `src/rules.{hpp,cpp}`: MS C rand, slot machine, tile properties (europe.pit),
  step counter, field distances and routes as in rally.exe (no SDL).
- `src/board.{hpp,cpp}`: europe.inf, europe.str field graph and lists, town records,
  OEM code page 850 conversion (no SDL).
- `tests/*_test.cpp`: synthetic and malformed-input tests (ctest).
- `tools/ne.py`, `tools/ne_disasm.py`: NE parser and annotated objdump disassembly.
- `tools/ne_strings.py`: RT_STRING dump of NE resources (e.g. rally_d.dat); output stays private.
- `src/cards.{hpp,cpp}`: card table, deck, shuffle, hands, movement cards (no SDL).
- `src/economy.{hpp,cpp}`: town state, purchases, whole-town bonus, monthly settlement (no SDL).
- `src/drago.{hpp,cpp}`: Dr. Drago state, forms, action tables, meetings (no SDL);
  `src/game_drago.cpp` applies his actions and the Weihnachten / Hau ab cards in the game.
- `src/events.{hpp,cpp}`: event countdown and tables, East opening rule, barrier links,
  robbery victim, event places and damage towns (no SDL); `src/game_events.cpp` runs
  the trigger and effects in the game.
- `src/obstacles.{hpp,cpp}`: road obstacle table, expiry, placement and link lookup (no SDL).
- `tools/inspect_game.py`: read-only installation inventory and NE header analysis.
- `tools/install.py`: validated ISO / directory import, native build/test/install and launcher;
  `tools/installer_gui.py` the Tk assistant with `tools/installer_*.py` (preflight checks,
  dependencies, directory import, process, progress, layout and theme) and
  `tools/installation_manifest.py` the manifest and its verification.
- `tools/project_version.py`, `tools/build_appimage.py`, `tools/check_appimage.py`,
  `tools/export_branding.py` / `tools/branding_render.py`: version, setup AppImage and
  branding exports (docs/releases.md).
- `tools/soundtrack.py`, `tools/build_soundtrack.py`, `tools/cd_soundtrack.py`,
  `tools/soundfont_defaults.py`, `tools/inspect_music.py`: Ogg sound-set preparation and
  music research (docs/soundtracks.md).
- `tools/editions.py`: the installer's edition profiles and known demos (mirrors
  `src/edition.cpp`); `tests/test_editions.py` checks both.
- `tools/installer_update.py`: explicit verified updates, preserved local files/Ogg
  packs and retained rollback installation. Never hash mutable saves as managed
  assets or modify XDG settings; run smoke checks with isolated XDG directories.
- `src/music_{catalog,stream,player,menu}.{hpp,cpp}`: complete local Ogg catalogs,
  bounded worker decoding, presentation-only cue control and sound-set selection on the recomp menu's Audio page (`music_menu`).
  One 44.1 kHz stereo device mixes music and effects. No file I/O or waiting in
  the audio callback; stop/error must never count as natural completion.
- `docs/native-music.md`: supported triggers, timing limitations, preferences and
  generated-tone tests. Preserve game RNG when exposing musical choices.
- `tools/check_repository.py`: tracked source/media/path distribution checks.
- `docs/distribution.md`: source-only boundaries and external installation policy.
- `tools/cd_audio.py`: hash-specific CD audio region import to local Ogg Vorbis.
- `tests/test_cd_audio.py`: generated-tone conversion and audio layout validation.
- `tools/release_ogg.py`: hash-pinned release Ogg tracks of the US English retail
  edition, copied 1:1 and marked "Release-OGG (nicht original)"; `tests/test_release_ogg.py`.
- `docs/cd-audio.md`: provisional boundaries, provenance and native playback requirements.
- `tools/iso9660.py`: bounded primary-tree reader for standard/raw Mode 1 ISO images.
- `tests/test_iso9660.py`: synthetic ISO fixtures and malformed-input checks.
- `docs/installer.md`: distribution contract, supported media and installer stages.
- `docs/formats.md`: confirmed observations, unknowns and next milestones.
- `docs/architecture.md`: agreed simulation, multiplayer, usability and graphics direction.
- `README.md`: setup, controls, limitations and verification commands.

## Reconstruction discipline

### Code structure and engineering rules

- A file containing code must not exceed **1000 lines**. Split growing files by
  coherent responsibility before reaching the limit, with small modules and clear
  interfaces. Do not split arbitrarily by line number or hide code in include fragments.
- Exceed the limit only when a technical constraint makes a meaningful split
  impossible. Document the concrete reason and the affected file; convenience or
  postponing refactoring is not an exception. The repository check must record any
  necessary exception with its justification.
- Keep format parsing, simulation, presentation, audio and installer work separate.
  Prefer focused functions, descriptive names, explicit ownership and narrow APIs.
- Use RAII for C++ resources, validate binary bounds and command preconditions, and
  handle failures explicitly. Avoid hidden mutable globals and duplicated rules.
- Add focused tests for behavior and edge cases; run the checks appropriate to the
  change. Keep implementation and technical documentation consistent.

### Evidence and original behavior

- Keep confirmed facts separate from hypotheses. Record evidence as file offsets,
  field sizes, executable addresses or reproducible commands where available.
- Do not invent game mechanics or infer record layouts solely from file sizes.
- Original graphics displayed by the viewer are not evidence that the original
  renderer, world map or gameplay has been reconstructed.
- Identify binaries by hash when comparing versions. The German retail `rally.exe` has
  SHA-256 `aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83`, the US
  English retail one `a8374ca9e39d70b4508efefa782b7135c650792be36600289356a51de819225a`
  (docs/media-variants.md); addresses in the docs are German (D) unless marked R.
- The executable is 16-bit Windows NE, using segmented addressing and WinG/GDI.
  Account for this when selecting disassembly or translation tools. Segment 9
  switches itself to 32-bit code; disassemble it with `--use32 9`.
- Bounds-check new binary parsers and test malformed input as well as original
  files. Preserve indexed palettes and investigate transparency explicitly.
- Add focused modules as more formats are decoded; keep format parsing separate
  from gameplay and rendering.

## Next work

Music handoff for Claude (2026-11-23): the user recognizes the recovered CD
recording as the remembered soundtrack, after comparison with our GM rendering.
Prioritize a complete selectable CD-recording set with honest MIDI supplements
for missing cues; keep the existing MIDI set as an alternative. SB16 was the
reported hardware, but an FM reconstruction is not the immediate requested fix.
Evidence, catalog blockers and acceptance boundaries are in
`docs/music-research.md` under "User listening reference and handoff to Claude".
This set is planned, not implemented; preserve the distinction between listening
recognition and proven original CD control flow.

Follow `docs/architecture.md` when introducing gameplay: use an SDL-independent
simulation, explicit validated commands, authoritative random outcomes and a
versioned serializable state. Local, AI and remote play must share this boundary.
LAN then Internet multiplayer are agreed goals, not current capabilities; by user
decision (2026-11-26) that work starts only after the port is complete
(`docs/architecture.md`, "Multiplayer"). Keep cosmetic effects local and optional;
preserve the alignment of roads and fields.

Both retail editions are supported and installable: the German "Die total
verrückte Rallye" and the US English "Dr. Drago's Madcap Chase" (playable since
round E2). Blue Byte's official English demo (`c8b7dab6…`, Demo Version, 27-MAY-95)
is recognised and deliberately refused as unsupported. Evidence and fingerprints
are in `docs/media-variants.md`. Keep edition/resource selection explicit by the
EXE hash; do not bypass the hash gate or rename English resources to German paths.
Saves record their edition (format 8) and live in a directory per edition
(`docs/save-format.md`); `map/europe.map` is read afresh for every new or loaded
game.

Game controllers are fully supported (every window and dialog operable by pad,
on-screen keyboard for names, every pad button remappable on the recomp menu's
Steuerung page); keyboard keys stay fixed (user decision).

Next: finish the game window as in the original (agreed with the user: it must
look like rally.exe; `--game`, src/game_screen.cpp): compare each window, message
and its input and timing with rally.exe and close the remaining deviations, with
all art and text loaded from the installation. Keep `kSaveFormat` /
`kRulesRevision` in src/save.hpp current when the state changes. The viewer is a
debugging aid only; put effort into the simulation and the game window. The port
remains a manual reconstruction; a static translator is not in use.
