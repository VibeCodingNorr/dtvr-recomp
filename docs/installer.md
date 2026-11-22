# Original ISO installation pipeline

The intended distribution is our native source/toolchain and an installer. Each
user supplies their own original DTVR ISO. Original executables, extracted art,
sound and generated outputs remain local and are not repository contents.

## Current implementation

The versioned setup distribution is an AppImage bundling our committed source
and Python/Tk. The current project version is `0.0.1`; see
[release packaging](releases.md) for artifact names, prerequisites and release rules.

Launch `./install.sh` for the native Tk assistant. It selects an available Python
with a usable Tk runtime, shows our own project branding, and delegates to the
same CLI backend used below. Choose ISO and destination, optionally select a
SoundFont and Classic/Dry style, then click **Installieren und kompilieren**.
The assistant automatically configures CMake, compiles, tests, installs and
verifies the result. No manual compiler invocation is needed.

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
4. Configure and compile this checkout in Release mode, run CTest, and install the
   native executable into staging. No original Windows program is executed.
5. Run overview, tile, sprite, world and game-window smoke checks against the extracted data,
   with isolated XDG save/settings directories and dummy video/audio drivers.
6. Add the relocatable `play` launcher, write a version-2 manifest hashing all
   managed files including that launcher, and verify hashes/executable permissions.
   Without arguments the launcher opens `--game`; explicit viewer flags remain available.
7. Publish the staged installation by renaming it to the requested destination.
   On ordinary failure, temporary output is cleaned up and the destination is absent.

The final installation has `bin/dtvr-recomb`, `data/program/`, `manifest.json` and
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

Pass `--soundfont /path/to/bank.sf2` to additionally render a complete 25-cue Ogg
pack. `--music-style classic|dry`, `--music-pack-id` and `--music-pack-name`
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

The ISO is still required. Update mode accepts verified version-2 installations
with a project version and refuses downgrades. Rebuilding the same version is
allowed during development. Build/test failures leave the old installation in place.
After successful tests, copy local files and existing Ogg packs into staging.
A newly rendered pack replaces an old pack with the same ID. Symlinks, special
files and collisions between user files and new managed files are rejected.

Game saves under the installation's `saves/` directory, including the default XDG
location, are preserved as mutable user files. Settings normally live separately
at `$XDG_CONFIG_HOME/dtvr-recomb/settings.ini` (or `~/.config/dtvr-recomb/settings.ini`)
and are never modified by the installer. Unmanaged local files are copied and
excluded from managed-file hashes; saves are always excluded. The game must remain
closed throughout the update because the installer cannot lock a running game.
Preserving bytes does not migrate incompatible save formats; the game validates them.

Publication uses a per-destination lock and short, cancellation-protected renames.
The old directory moves to a unique sibling `dtvr-recomb.backup-*/installation`,
then staging takes its place. The complete backup is retained and its path printed
in the GUI log/CLI. If the second rename fails, restore the old directory immediately.
SIGKILL or power loss between renames may require manually restoring that backup.
For manual rollback, close the game, move the current installation aside and move
the backup's `installation` directory to the original destination. No backup is
automatically deleted. Updates are local and explicit; there is no network updater.

## Read-only checks

```sh
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
