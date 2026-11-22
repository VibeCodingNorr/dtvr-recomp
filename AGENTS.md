# Working on DTVR Recomb

## Objective and current state

Reconstruct Die total verrückte Rallye as a native Linux game using the user's
original installation. Repository: https://github.com/VibeCodingNorr/dtvr-recomb.
Communicate with the user in German. Code and technical documentation may use English.

The current C++17 / SDL2 executable is a data viewer. It loads the original Europe
overview and presents SBB bitmap strips as a graphics-block grid. Zoom, pan, view
switching and SBB file navigation are implemented. SBS frames are decoded. No game rules, AI, save support,
SBS animation playback or executable translation have been implemented.

## Local original data

- Original ISO: `/media/steam1/Games/Dtvr/dtvr.iso` (raw Mode 1, 2352-byte sectors).
- Distribution model: ship our installer/toolchain; users supply their original ISO.
  Use `python3 tools/install.py --iso /path/to/dtvr.iso` to build a local installation.
  This currently produces the native viewer, not a playable translated executable.
- CD root: `/media/steam1/Games/Dtvr/ISOINHALT`
- Program directory: `/media/steam1/Games/Dtvr/ISOINHALT/program`
- Treat original files as read-only. Accept paths through `--data`; do not hardcode
  this machine's path into the runtime.
- Keep original executables, assets, extracted game content, screenshots containing
  game art and generated analysis output out of Git. Use ignored `build/` or `local/`
  for local artifacts. The repository contains our source and format observations.

## Build and verify

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/dtvr-recomb --data /media/steam1/Games/Dtvr/ISOINHALT
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /media/steam1/Games/Dtvr/ISOINHALT --smoke-test --screenshot build/overview.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /media/steam1/Games/Dtvr/ISOINHALT/program --tiles --smoke-test --screenshot build/tiles.bmp
SDL_VIDEODRIVER=dummy ./build/dtvr-recomb --data /media/steam1/Games/Dtvr/ISOINHALT --sprites --smoke-test --screenshot build/sprites.bmp
ctest --test-dir build --output-on-failure
python3 tools/inspect_game.py /media/steam1/Games/Dtvr/ISOINHALT > build/game-analysis.json
```

Requires CMake, pkg-config, a C++17 compiler, SDL2 >= 2.0.10 development files and
Python 3. The analysis script uses only the Python standard library.
Inspect screenshots when changing rendering. Some image viewers require conversion
from BMP to PNG; Pillow is available on the current machine, but is not a build dependency.
The dummy driver verifies rendering and file loading, not interactive desktop input.

## Project layout

- `src/main.cpp`: native SDL viewer, command-line handling, image loading.
- `src/sbs.{hpp,cpp}`: bounds-checked SBSF sprite container parser (no SDL).
- `tests/sbs_test.cpp`: synthetic and malformed-input parser tests (ctest).
- `tools/inspect_game.py`: read-only installation inventory and NE header analysis.
- `tools/install.py`: validated ISO import, native build/test/install and launcher.
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
  Account for this when selecting disassembly or translation tools.
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

Confirm SBS transparency and palette selection from code, derive full map tile indexing from the
original loader, then recover the city/road graph. Reconstruct and validate one
complete human turn before broadening gameplay. Choosing a static translator versus
manual reconstruction requires further executable analysis.
