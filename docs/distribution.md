# Source-only distribution

The repository distributes our native source, tools, synthetic tests and technical
format documentation. A user supplies a supported original ISO or game directory to the installer.
No developer's installed copy, mounted CD, local symlink or asset download is
required to build and test this source.

Original media, graphics, sounds, text/resource files, executables, extracted code,
disassembly, generated music and screenshots containing game content must remain
outside the source checkout. Do not add them to commits, releases or CI artifacts.
Technical filenames, signatures, input hashes, format fields and implementation
observations are allowed: they enable the importer to recognize user-owned media.
All paths in public examples are generic placeholders.

Project-authored SVG branding in `assets/branding/` is allowed as editable text
source. It contains our own shapes and lettering, with no original game graphics
or external images. Raster/ICO exports are generated outside the checkout with
`tools/export_branding.py` and are not committed.

## Installation boundary

The installer receives an explicit `--iso` or `--directory` path and creates an external user-local
installation. By default it uses `$XDG_DATA_HOME/dtvr-recomb`, or
`~/.local/share/dtvr-recomb` when that environment setting is unavailable or relative.
An explicit `--output` also has to be outside the checkout, including after
resolving symlinks. Original source files are never modified in place. Directory
imports identify the supported executable and record SHA-256 hashes for every
imported input file; see [installer validation](installer.md). Directory sources
and installation destinations must not overlap. No source directory is discovered
automatically or embedded as a machine-specific dependency.

The native binary and its imported assets live together in the installation.
The launcher resolves its own location, so it has no dependency on the checkout,
an extraction directory, a disc drive or the original ISO. Temporary Windows
executables/DLLs used during validation are removed before publication. The
resulting binary still needs its normal system SDL2 runtime.

## Checks

`python3 tools/check_repository.py` checks tracked paths, UTF-8 source content,
binary/media extensions, symlinks and machine-specific data paths. The source
policy workflow runs this check and the synthetic Python tests without original
media. Synthetic parser tests also run through CTest. These automated checks are
guardrails; review new text fixtures to ensure they contain synthetic data rather
than copied original text or disassembly.

Before publishing, inspect `git diff --cached --stat` and the staged file list.
Integration tests against a user-owned ISO run locally with external destinations;
their resulting files are not uploaded. Never rewrite shared history merely to
refresh examples; make a normal corrective source/documentation commit.
