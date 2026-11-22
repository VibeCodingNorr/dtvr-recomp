# Original ISO installation pipeline

The intended distribution is our native source/toolchain and an installer. Each
user supplies their own original DTVR ISO. Original executables, extracted art,
sound and generated outputs remain local and are not repository contents.

## Current implementation

```sh
python3 tools/install.py --iso /path/to/dtvr.iso --output /path/to/new/installation
/path/to/new/installation/play
```

1. Check that the target is outside the checkout, does not exist, and required
   build tools/SDL2 are available. Default to the user's XDG data directory.
2. Read the ISO9660 primary directory tree with bounded extents and validated names.
   Extract only `program/` into a temporary directory beside the target.
3. Validate the original `rally.exe` by SHA-256 and check required viewer assets.
   Remove `rally.exe`, the Windows runtime DLLs and their configuration from the
   staged data; the native runtime discovers its data without the Windows EXE.
   For the known mixed-mode image, convert recovered CD audio regions to local
   Ogg Vorbis files using FFmpeg and validate them with ffprobe.
4. Configure and compile this checkout in Release mode, run CTest, and install the
   native executable into staging. No original Windows program is executed.
5. Run overview, tile, sprite and world smoke checks against the extracted data.
6. Write a manifest identifying the ISO, original executable and hashes of the
   installed binary and extracted files, then add a relocatable `play` launcher.
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
The current viewer does not yet play music. The intended game uses these local
Ogg assets through the recovered original music cues, without a mounted disc.

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
parser tests and all four viewer smoke checks before publishing its output.

The initial real-image run extracted 962 files. Four Windows runtime files are
omitted from the final installation. Compare per-file hashes when changing the importer.

## Future reconstruction stages

The current installer builds the native viewer and installs original data. It
does not yet recompile original instructions. If executable translation or data
conversion is introduced, add explicit, versioned stages after media validation,
and record input hashes, conversion/tool versions and generated outputs in the
manifest. Original executable bytes are only temporary validation/analysis inputs
and are not included in the resulting native installation.

Future packaging can supply a prebuilt native runtime and a graphical ISO picker,
progress display and dependency guidance. Updating an installation, migrating saves,
desktop integration and uninstall behavior remain unimplemented; the initial CLI
requires a new destination for every install.
