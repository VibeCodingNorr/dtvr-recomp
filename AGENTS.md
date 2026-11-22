# Working on DTVR Recomb

## Objective and current state

Reconstruct Die total verrückte Rallye as a native Linux game using the user's
original installation. Repository: https://github.com/VibeCodingNorr/dtvr-recomb.
Communicate with the user in German. Code and technical documentation may use English.

The current C++17 / SDL2 executable is a data viewer. It loads the original Europe
overview and presents SBB bitmap strips as a graphics-block grid. Zoom, pan, view
switching and SBB file navigation are implemented. SBS frames are decoded and
the full world map is composed from europe.map with rules recovered from
rally.exe. The board (700 fields, links, 115 towns) is decoded and shown as an
overlay. Movement counting, the slot machine and the original random generator
are reconstructed as tested rules; a move preview exists in the world view. No
complete turn, field effects, AI, save support,
SBS animation playback or executable translation have been implemented.

## Source-only distribution and user input

- License our original source, tools and tests as `GPL-3.0-only`. Preserve the
  SPDX identifier in source headers and the unmodified GNU GPL v3 text in `LICENSE`.
  The project license does not cover original game data or third-party dependencies.

- Users supply an original ISO explicitly through `--iso`. All examples use
  generic placeholders; never commit machine-specific paths or rely on a local
  installation, mounted disc, download URL or symlink to original game content.
- Distribution model: ship our installer/toolchain; users supply their original ISO.
  Use `python3 tools/install.py --iso /path/to/dtvr.iso` to build a local installation.
  This currently produces the native viewer, not a playable translated executable.
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

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/dtvr-recomb --data /path/to/extracted
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /path/to/extracted --smoke-test --screenshot /path/to/artifacts/overview.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /path/to/extracted/program --tiles --smoke-test --screenshot /path/to/artifacts/tiles.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /path/to/extracted --sprites --smoke-test --screenshot /path/to/artifacts/sprites.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /path/to/extracted --world --smoke-test --screenshot /path/to/artifacts/world.bmp
ctest --test-dir build --output-on-failure
python3 tools/ne_disasm.py /path/to/extracted/program/rally.exe /path/to/private-analysis --use32 9 --dll-dir /path/to/extracted/program --dll-dir /path/to/extracted/wing --dll-dir /path/to/optional-system-dlls
python3 tools/inspect_game.py /path/to/extracted > /path/to/artifacts/game-analysis.json
```

Requires CMake, pkg-config, a C++17 compiler, SDL2 >= 2.0.10 development files and
Python 3. The analysis script uses only the Python standard library.
Inspect externally stored screenshots when changing rendering. Some image viewers require conversion
from BMP to PNG; Pillow is available on the current machine, but is not a build dependency.
The dummy driver verifies rendering and file loading, not interactive desktop input.

## Project layout

- `src/main.cpp`: native SDL viewer, command-line handling, image loading.
- `src/sbs.{hpp,cpp}`: bounds-checked SBSF sprite container parser (no SDL).
- `src/worldmap.{hpp,cpp}`: europe.map cells, season table, SBB tile split and
  map composition following rally.exe (no SDL).
- `src/rules.{hpp,cpp}`: MS C rand, slot machine, tile properties (europe.pit),
  step counter, field distances and routes as in rally.exe (no SDL).
- `src/board.{hpp,cpp}`: europe.inf, europe.str field graph and lists, town records,
  OEM code page 850 conversion (no SDL).
- `tests/*_test.cpp`: synthetic and malformed-input tests (ctest).
- `tools/ne.py`, `tools/ne_disasm.py`: NE parser and annotated objdump disassembly.
- `tools/inspect_game.py`: read-only installation inventory and NE header analysis.
- `tools/install.py`: validated ISO import, native build/test/install and launcher.
- `tools/check_repository.py`: tracked source/media/path distribution checks.
- `docs/distribution.md`: source-only boundaries and external installation policy.
- `tools/cd_audio.py`: hash-specific CD audio region import to local Ogg Vorbis.
- `tests/test_cd_audio.py`: generated-tone conversion and audio layout validation.
- `docs/cd-audio.md`: provisional boundaries, provenance and native playback requirements.
- `tools/iso9660.py`: bounded primary-tree reader for standard/raw Mode 1 ISO images.
- `tests/test_iso9660.py`: synthetic ISO fixtures and malformed-input checks.
- `docs/installer.md`: distribution contract, supported media and installer stages.
- `docs/formats.md`: confirmed observations, unknowns and next milestones.
- `docs/architecture.md`: agreed simulation, multiplayer, usability and graphics direction.
- `README.md`: setup, controls, limitations and verification commands.

## Reconstruction discipline

- Keep confirmed facts separate from hypotheses. Record evidence as file offsets,
  field sizes, executable addresses or reproducible commands where available.
- Do not invent game mechanics or infer record layouts solely from file sizes.
- Original graphics displayed by the viewer are not evidence that the original
  renderer, world map or gameplay has been reconstructed.
- Identify binaries by hash when comparing versions. This local `rally.exe` has
  SHA-256 `aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83`.
- The executable is 16-bit Windows NE, using segmented addressing and WinG/GDI.
  Account for this when selecting disassembly or translation tools. Segment 9
  switches itself to 32-bit code; disassemble it with `--use32 9`.
- Bounds-check new binary parsers and test malformed input as well as original
  files. Preserve indexed palettes and investigate transparency explicitly.
- Add focused modules as more formats are decoded; keep format parsing separate
  from gameplay and rendering when extending beyond this first viewer.

## Next work

Follow `docs/architecture.md` when introducing gameplay: use an SDL-independent
simulation, explicit validated commands, authoritative random outcomes and a
versioned serializable state. Local, AI and remote play must share this boundary.
LAN then Internet multiplayer are agreed goals, not current capabilities. Keep
cosmetic effects local and optional; preserve the alignment of roads and fields.

Trace direction input, the end of a move and what each field kind does on
arrival, then palette selection and the map viewport.
Reconstruct and validate one complete human turn before broadening gameplay. Choosing a static translator versus
manual reconstruction requires further executable analysis.
