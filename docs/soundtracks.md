# Preparing complete Ogg sound sets

Status: offline pack building, verification and optional installer integration
are implemented. The game window now supports [native Ogg playback and F8
soundtrack selection](native-music.md). Original media and SoundFonts are supplied by the user.

## Contents and styles

Every pack contains all **25** original MIDI cue identities: `a1`, `e1`–`e11`,
`f1`–`f3`, `g0`–`g5`, `i`, `s`, `z1`, `z2`. This includes `g2`, the full intro,
winner music and short cues absent from the six recovered CD recordings.

| Option | Rendering | Limitation |
| --- | --- | --- |
| `classic` | Supplied bank with fixed reverb/chorus settings. | Not authenticated Roland or other historical hardware. |
| `dry` | Same bank with reverb and chorus disabled. | A mix variant, not a different hardware emulation. |
| Another supplied bank | Another complete pack rendered with a compatible SF2/SF3 bank. | Instrument mapping needs validation. |
| Base plus overlay | Repeat `--soundfont`, base first. | A small GS extension bank is not a complete GM bank by itself. |

The runtime contract is Ogg-only. MIDI and synthesis run during preparation; a
pack contains neither MIDI files nor SoundFont samples. No banks are downloaded
automatically. Names/hashes identify inputs without becoming runtime dependencies.

## Build independently

Requires Python 3, FluidSynth, FFmpeg with `libvorbis`, ffprobe and a compatible
user-supplied bank. Use your own extracted MIDI directory and an external output:

```sh
python3 tools/build_soundtrack.py \
    --midi-dir /path/to/extracted/program/snd/midi \
    --soundfont /path/to/bank.sf2 \
    --output /path/to/soundtrack-packs/my-gm-classic \
    --pack-id my-gm-classic --name "My GM bank — Classic" --style classic
```

Repeat with a new output/ID and `--style dry`, or another bank, for alternatives.
Existing packs are never overwritten; destinations inside the checkout, including
through symlinks, are rejected. Failed renders clean up staging. Identical MIDI
sources render once per pack and are copied under separate cue IDs; all eight
identical short-cue files remain distinct catalog entries.

## Build from the ISO

```sh
python3 tools/install.py --iso /path/to/game.iso \
    --output /path/to/new/installation \
    --soundfont /path/to/bank.sf2 \
    --music-pack-id my-gm-classic --music-pack-name "My GM bank — Classic" \
    --music-style classic
```

The installer imports the original files, prepares supported CD regions, renders
the selected pack, then builds/tests the native installation. The pack is placed
at `data/music/packs/my-gm-classic/` and listed/hashed in the installation manifest.
Without `--soundfont`, the installer retains CD extraction only and does not
claim a complete soundtrack. Additional independently built packs can live under
the external installation's `data/music/packs/`; the game discovers complete packs on startup.

## Catalog and timing

`pack.json` records schema/ID/name/style, ordered bank hashes, synth/encoder
versions, settings, original MIDI hashes and output hashes. All cues map to safe
relative `cues/<id>.ogg` paths. `background_order` is `g0` through `g5`. Provisional
CD matches are not silently substituted and original CD track IDs are not invented.

Ogg output is 44,100 Hz stereo Vorbis, quality 6, encoded once from floating-point
WAV with synth gain 0.2. Rendering uses sample-driven timing, one synth CPU worker,
fixed polyphony/effects settings and an empty synth config instead of inherited
user settings. Byte-identical output across different synth/codec versions is not
promised; versions and inputs are recorded.

- `nominal_end_seconds` is original SMF tempo-map/end-of-track time, not stopwatch
  verification of Windows MCI playback.
- `media_duration_seconds` adds exactly two seconds for note release/reverb.
  Rendering is padded/bounded to that length; leading/internal MIDI silence stays.

Native playback triggers completion at the nominal end, without adding two
seconds to every background change. Overlapping tails and all original animation
boundaries still need integration. The catalog marks original hardware and
playback timing as unverified. `loop` is false; background cycling is not an
infinite loop of the same piece.

The original `e11` produces a missing-note-off warning with the tested synth.
FluidSynth releases those notes at EOF; the catalog records
`missing_note_off_at_eof`. Instrument errors or missing presets fail the build.
Each rendered Ogg is probed for codec/channels/rate/duration and fully decoded
before the pack is published.

## Verify without MIDI or banks

```sh
python3 tools/build_soundtrack.py --verify /path/to/soundtrack-packs/my-gm-classic
```

Verification checks the 25 required IDs, local paths, hashes, timing and full
decoding. It uses ffprobe/FFmpeg but does not load FluidSynth, banks or MIDI.
These are preparation/diagnostic tools; the game uses libvorbisfile for native decoding.

Synthetic `tests/test_soundtrack.py` checks completeness, reuse, missing cues,
unsafe IDs/paths, invalid timelines, tampering and failed-render cleanup. Set
`DTVR_TEST_SOUNDFONT` to an explicit bank to also render generated test MIDI.

Two local complete sets were prepared using the supported original files and the
available FluidR3 GM bank: `fluidr3-gm-classic` and `fluidr3-gm-dry`. Their files
remain external, not repository/release contents or authentic hardware claims.

Next: listening checks, remaining original transition boundaries and a broader
[ESC settings interface](pause-menu.md). F8 selection and streaming are implemented.
A CD-hybrid set remains experimental
because source boundaries/timing are unverified; its short recorded intro must
not blindly replace the full intro. See [music research](music-research.md) and
[FluidSynth's file renderer](https://www.fluidsynth.org/api/FileRenderer.html).
