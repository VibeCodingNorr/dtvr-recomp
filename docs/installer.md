# Original-data installation pipeline

The intended distribution is our native source/toolchain and an installer. Each
user supplies their own original DTVR ISO or original game directory. Original executables, extracted art,
sound and generated outputs remain local and are not repository contents.

## Current implementation

The versioned setup distribution is an AppImage bundling our committed source
and Python/Tk. The current project version is `0.0.1`; see
[release packaging](releases.md) for artifact names, prerequisites and release rules.

Launch `./install.sh` for the native Tk assistant. It selects an available Python
with a usable Tk runtime, shows our own project branding, and delegates to the
same CLI backend used below. Choose **Original-ISO** or **Spielverzeichnis**, then source and destination. Original MIDI music is
enabled by default, using a detected local GM SoundFont. If none is found, select
a bank under **Anpassen**. Skipping preparation is an explicit **Musikvorbereitung überspringen**
choice; the original CD recordings alone do not provide a complete soundtrack.
Classic/Dry styles remain optional. Then click **Installieren und kompilieren**.
The assistant automatically configures CMake, compiles, tests, installs and
verifies the result. No manual compiler invocation is needed.

The approved **Moderne Rallye** design follows the road, tower and die logo:
a larger transparent logo on the navy sidebar, warm cream content, green primary
actions and amber waypoints. A dashed road joins the three phases (Vorbereiten /
Installieren / Losspielen); completed phases have drawn checkmarks that do not
depend on font glyph coverage. Installation type and data source share a compact
row above the numbered source, destination and music cards.

Music options expand only when needed. Installation has its own progress view
with a collapsible, scrollable log; output is retained while collapsed. Errors
and cancellation open the log. The footer log action remains available from
every phase, and returning from the log preserves the completion view. The
success screen shows a finish flag and **Bereit zur Rallye** with **Spiel starten**.
The route represents phases; the backend's seven-stage bar reports progress.

The default window is 1200 × 900, minimum 960 × 760. Cards reflow and the form
scrolls on smaller windows; primary actions and diagnostics use separate footer
rows. Fonts are selected case-insensitively, with a system-font fallback.
Import/update behavior stays in `installer_gui.py`; `installer_layout.py` and
`installer_theme.py` own presentation. Packaging renders the sidebar logo at
264 × 88, including the transparent dark variant.
Logo and icons use the shared supersampling renderer: four times the requested
size, followed by two filtered half-size passes. Packaged PNGs and source GUI
rendering therefore use the same edge smoothing without adding host dependencies.
The road, circular waypoints and completion ticks also use pre-rendered,
antialiased state images rather than Tk canvas strokes. Three packaged PNGs
cover all phases; numbers and labels remain normal Tk text. Source checkouts
without either these PNGs or librsvg retain a minimal canvas fallback.

The project-owned folder chooser supports typed paths, directory navigation,
hidden folders and selecting the current folder. Its optional **Systemdialog**
button opens the standard Tk/platform directory chooser instead. ISO and
SoundFont selection also use our own chooser, with case-insensitive file filters,
typed paths, hidden entries and an optional **Systemdialog** fallback.

Choose **Neu installieren** or **Vorhandene Installation aktualisieren** explicitly.
Update mode shows the detected previous version and the target version; successful
updates show the retained backup and offer **Sicherung öffnen**.

**Pakete prüfen** checks all build/audio dependencies without requiring an ISO.
The separate overview shows available, missing and currently uncheckable items.
For Debian/Ubuntu and Arch-derived systems it offers a copyable command containing
the missing packages; commands are never executed by the installer. Without
pkg-config, library checks are marked uncheckable until it is installed. Unknown
distributions show the component list without guessing a package command.

**Diagnose speichern** exports a bounded local text log. Selected paths and the
home directory are replaced; remaining lines containing absolute or encoded paths
are removed conservatively. Hostname, original assets and environment dumps are
not collected. No log is sent anywhere. Some path-bearing error lines are omitted
to preserve privacy. The normal on-screen log remains available locally.

The GUI uses a background process/event queue; only the main thread updates Tk.
It shows seven stages and a bounded log. Cancel interrupts the dedicated installer
process group; normal interruption cleans staging. Closing an active window
offers to cancel and waits for cleanup. After success, start the native prototype
or open its installation folder.

Prerequisites must already be installed. The GUI identifies missing tools/SDL2/SDL2_ttf,
libvorbisfile and json-c >= 0.15 development files,
and a complete Arial or Liberation Sans font family for game text;
it does not install system packages, download media or ask for root. Without Tk
or a graphical session, use the CLI:

```sh
python3 tools/install.py --iso /path/to/dtvr.iso --output /path/to/new/installation
/path/to/new/installation/play
```

A directory is an alternative to the ISO, including for updates and `--check`:

```sh
python3 tools/install.py --directory /path/to/original-game --output /path/to/new/installation
python3 tools/install.py --directory /path/to/original-game --output /path/to/installation --update --check
```

Select either the directory containing `rally.exe` or its parent containing
`program/`; matching is case-insensitive. Only that program tree is imported.
The source remains read-only and must not overlap the destination in either
direction. Symbolic links, special files, control characters, backslashes and
case-colliding names are rejected. Bounds are 20,000 entries, 16 directory levels,
64 MiB per file and 2 GiB total file bytes. Names are normalized to lowercase.

The known SHA-256 of `rally.exe` identifies the supported version, and the same
required runtime assets as for ISO import must exist. Every input file gets a
size and SHA-256 entry. The importer checks the inventory again before and after
copying and verifies each copied file against it. The installation manifest
stores `source_kind: directory`, `source_files` and `source_sha256`, a SHA-256 of
the UTF-8 JSON file table (sorted keys, compact separators, default ASCII escaping).
The fingerprint is independent of the source directory's absolute path, timestamps
and entry order. Absolute source paths are not stored in the manifest.
Input hashes include validation-only Windows files subsequently removed from the
installation; `files` separately hashes the installed, managed runtime files.
These input hashes document the supplied content. They do not authenticate all
assets against a known original-disc file list.
The inspected US English directory is a different, currently unsupported variant.
Future support is an agreed user goal; edition selection, rule/data comparison
and native validation remain pending. See
[media variants and Claude integration handoff](media-variants.md#us-english-integration-handoff).

Directory sources provide no CD audio regions; the manifest records
`cd_audio.status: unavailable-from-directory` with null ISO hash/sector size.
MIDI/SoundFont preparation works as usual. Updates preserve existing music packs.

1. Check that the target is outside the checkout, is new or explicitly selected for update, and required
   build tools/SDL2 are available. Default to the user's XDG data directory.
   Verify the executable directly in the ISO, selected SoundFonts, the Vorbis
   encoder and an estimated free-space reserve before creating installation data.
2. Read the ISO9660 primary directory tree with bounded extents and validated names.
   Extract only `program/` into a temporary directory beside the target.
3. Validate the original `rally.exe` by SHA-256 and check required viewer assets.
   Remove `rally.exe`, the Windows runtime DLLs and their configuration from the
   staged data; the native runtime discovers its data without the Windows EXE.
   For the known mixed-mode image, convert recovered CD audio regions to local
   Ogg Vorbis files using FFmpeg and validate them with ffprobe.
   Render the original MIDI files into a complete 25-cue pack under
   `data/music/packs/`, unless the user explicitly selected `--no-music`.
4. Configure and compile this checkout in Release mode, run CTest, and install the
   native executable into staging. No original Windows program is executed.
5. Run overview, tile, sprite, world and game-window smoke checks against the extracted data,
   with isolated XDG save/settings directories and dummy video/audio drivers.
6. Add the relocatable `play` launcher, write a version-2 manifest hashing all
   managed files including that launcher, and verify hashes/executable permissions.
   Without arguments the launcher opens `--game`; explicit viewer flags remain available.
7. Publish the staged installation by renaming it to the requested destination.
   On ordinary failure, temporary output is cleaned up and the destination is absent.

The final installation has `bin/dtvr-recomp`, `data/program/`, `manifest.json` and
`play`. It needs the host SDL2 runtime; it is not a statically linked portable
package. Build dependencies must already be installed. Paths containing spaces
are supported. No privileged mount, Wine, extraction service or external archive
program is needed. Installation never modifies the source ISO.

CD audio import additionally requires FFmpeg with `libvorbis` and `ffprobe`.
Converted files and provenance go into `data/music/`. See [CD audio](cd-audio.md)
for the provisional segment layout and the native playback integration plan.
The game window plays complete installed Ogg packs through recovered music cues,
without a mounted disc. The six provisional CD recordings alone remain incomplete;
see [native music](native-music.md).

Complete original-MIDI music is the default. Known system GM banks are detected;
pass `--soundfont /path/to/bank.sf2` to choose one explicitly. A missing bank is
reported before installation; no bank is bundled or downloaded. Use `--no-music`
only to explicitly skip MIDI preparation; updates still preserve existing packs.
`--music-style classic|dry`, `--music-pack-id` and `--music-pack-name`
configure it. Repeat `--soundfont` for ordered base/overlay banks. See
[soundtrack preparation](soundtracks.md) for dependencies, timing and verification.

## Supported inputs and limits

The supported mixed-mode image uses raw Mode 1
sectors with 2352-byte physical sectors and 2048-byte payloads. Regular 2048-byte
ISO9660 images are supported too. A primary volume descriptor must be at logical
sector 16. Only primary ASCII names are used, lowercased with ISO version suffixes
removed; Joliet/Rock Ridge names and links are not interpreted.

Interleaved files, multi-extent files, extended attributes, multi-volume media,
Mode 2 sectors and directory cycles are rejected. Extent, tree depth, record count
and extraction size limits protect the bounded reader. This is a DTVR media
importer, not a general-purpose CD filesystem implementation.

Only the original executable hash documented in `AGENTS.md` is accepted. A
different release requires analysis and explicit support, rather than bypassing
the version check. Matching the executable identifies the supported version; it
does not authenticate every asset against a known disc manifest. Per-file hashes
record what was actually installed.

## Validation

For reproducible cancellation/failure scenarios and the AppImage check without a
FUSE mount, see [installer testing](installer-testing.md).

`tests/test_iso9660.py` uses synthetic standard and raw-sector images to check
extraction, case/version normalization, overwrite rejection, invalid paths,
truncation, out-of-volume extents and directory cycles. CTest runs these tests if
Python is available. The installer itself requires Python and also runs the SBS
parser tests, four viewer checks and the game-window smoke check before publishing its output.

The initial real-image run extracted 962 files. Four Windows runtime files are
omitted from the final installation. Compare per-file hashes when changing the importer.

## Future reconstruction stages

The current installer builds the native viewer and installs original data. It
does not yet recompile original instructions. If executable translation or data
conversion is introduced, add explicit, versioned stages after media validation,
and record input hashes, conversion/tool versions and generated outputs in the
manifest. Original executable bytes are only temporary validation/analysis inputs
and are not included in the resulting native installation.

Future packaging can supply a prebuilt game runtime. Save-format migration,
desktop-menu registration and uninstall behavior remain unimplemented.

## Updating a local installation

Close the game first. Select **Vorhandene Installation aktualisieren (mit Sicherung)**
in the GUI and choose the existing installation itself. The checkbox is initially
enabled when a manifest exists at the default destination. The equivalent CLI is:

```sh
python3 tools/install.py --iso /path/to/game.iso --output /path/to/installation --update --check
python3 tools/install.py --iso /path/to/game.iso --output /path/to/installation --update
```

Original data supplied via `--iso` or `--directory` is still required. Update mode accepts verified version-2 installations
with a project version and refuses downgrades. Rebuilding the same version is
allowed during development. Build/test failures leave the old installation in place.
After successful tests, copy local files and existing Ogg packs into staging.
A newly rendered pack replaces an old pack with the same ID. Symlinks, special
files and collisions between user files and new managed files are rejected.

Game saves under the installation's `saves/` directory, including the default XDG
location, are preserved as mutable user files. Settings normally live separately
at `$XDG_CONFIG_HOME/dtvr-recomp/settings.ini` (or `~/.config/dtvr-recomp/settings.ini`)
and are never modified by the installer. Unmanaged local files are copied and
excluded from managed-file hashes; saves are always excluded. The game must remain
closed throughout the update because the installer cannot lock a running game.
Preserving bytes does not migrate incompatible save formats; the game validates them.

Publication uses a per-destination lock and short, cancellation-protected renames.
The old directory moves to a unique sibling `dtvr-recomp.backup-*/installation`,
then staging takes its place. The complete backup is retained and its path printed
in the GUI log/CLI. If the second rename fails, restore the old directory immediately.
SIGKILL or power loss between renames may require manually restoring that backup.
For manual rollback, close the game, move the current installation aside and move
the backup's `installation` directory to the original destination. No backup is
automatically deleted. Updates are local and explicit; there is no network updater.

## Read-only checks

```sh
python3 tools/install.py --dependencies
python3 tools/install.py --iso /path/to/game.iso --output /path/to/new/installation --check
python3 tools/installation_manifest.py /path/to/installation
```

Preflight does not extract, build or create the destination; it reports recognized
media, required tools, selected banks and the disk-space reserve as JSON. The
reserve is a conservative estimate, not an exact final size.

Installed-file verification checks the manifest, safe local paths, required runtime
files, hashes and executable permissions without running the game. It does not
need the original ISO or banks. Legacy version-1 manifests omitted the launcher
from hashes; they require a new installation for this complete verification.

Tests cover synthetic media preflight failures, manifest corruption/path escapes,
GUI command construction, stage events and subprocess cancellation. Local Xvfb
checks exercised actual GUI import/build/start, responsive updates and cancellation
during disc-audio conversion. Killing the installer with SIGKILL or losing power
cannot guarantee cleanup; ordinary errors and cooperative cancellation do.

Package mapping references: [Ubuntu development package](https://packages.ubuntu.com/noble/libsdl2-dev),
[Ubuntu fonts](https://packages.ubuntu.com/noble/fonts-liberation),
[Debian JSON development package](https://packages.debian.org/trixie/libjson-c-dev),
[Arch SDL2 compatibility package](https://archlinux.org/packages/extra/x86_64/sdl2-compat/),
[Arch SDL2_ttf](https://archlinux.org/packages/extra/x86_64/sdl2_ttf/) and
[Arch Liberation fonts](https://archlinux.org/packages/extra/any/ttf-liberation/).
