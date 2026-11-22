# Local Ogg music from the original disc

Requirement: the native game must use locally installed Ogg tracks at the same
gameplay triggers as original CD music. The disc/ISO is an installation input,
not a runtime audio device. Preserve original track selection, stop/pause/resume,
looping and transitions once those behaviors have been recovered. This does not
mean an unrelated background playlist or a requirement to convert MIDI into Ogg.

## Implemented import

For the known local image, `tools/install.py` automatically calls `cd_audio.py`
to create `data/music/segment-01.ogg` through `segment-06.ogg`, plus `audio.json`.
Install into a new directory, for example:

```sh
python3 tools/install.py --iso /path/to/game.iso --output /path/to/new/installation
```

Requires FFmpeg with the `libvorbis` encoder and `ffprobe`, in addition to the
native build dependencies. Output is Ogg Vorbis, quality 6, 44,100 Hz stereo.
The original image is unchanged. Music files are original game content and must
stay outside Git, just like extracted graphics and speech.

The importer verifies the codec, channel count, sample rate and duration. It
records input sector ranges, source PCM hashes, encoder version, settings and
provisional status in `audio.json` and the installation manifest. The manifest's
file hashes also cover generated audio. A failed conversion prevents publication
of the staged installation. Existing music directories are not overwritten.

## Evidence and unresolved track boundaries

The supported image has SHA-256
`a94cb115097c22b539563dac1954e965a5ee5f812070d70beeb39906a4197fee`.
It is 114,479 raw sectors long; the ISO9660 volume declares 38,003 sectors.
Mode 1 sector headers continue through sector 38,154. Audio-like PCM follows,
with longer all-zero separators between six non-zero regions:

| Segment | Start sector | End sector (exclusive) | Duration (seconds) |
| --- | --- | --- | --- |
| 01 | 38156 | 51570 | 178.853 |
| 02 | 51721 | 66087 | 191.547 |
| 03 | 66238 | 80980 | 196.560 |
| 04 | 81131 | 92876 | 156.600 |
| 05 | 93027 | 106147 | 174.933 |
| 06 | 106298 | 114479 | 109.080 |

The user confirmed there is no companion CUE/TOC file. These are observed
non-zero regions, **not verified original track boundaries or track numbers**.
The current import omits zero separators; do not infer exact original leading or
trailing silence from it. All source bytes remain recoverable from the ISO.
Big-endian signed 16-bit PCM is the current profile: sampled stereo signals show
much lower adjacent-sample differences with this interpretation than little-endian.
Validate against original playback during track mapping; do not assume other
raw CD images use the same byte order.

The fixed profile applies only to the exact image hash. Other images report
`no-known-layout` and do not convert CD audio. The importer does not silently
split arbitrary images on silence or claim that a plain 2048-byte data image
contains CD-DA. Supporting another disc needs its own validated layout.

## Native playback integration: next work

1. Locate original CD playback calls and track IDs, including whether a cue uses
   CD audio, MIDI or sampled effects. Track uses and music titles are unverified.
2. Verify audio identity, boundary timing, PCM byte order and original track IDs
   against original playback or reliable disc metadata. Keep the extraction
   profile versioned so corrected boundaries can regenerate the outputs.
3. Add a versioned mapping from original music cue/track IDs to installed Ogg
   assets. Do not treat provisional segment numbers as original track IDs.
4. Add a native streaming Ogg decoder/audio backend with play, stop, pause, resume,
   seek and repeat behavior as required by the recovered original. Backend choice
   remains open; the current viewer has no music playback implementation.
5. Route recovered music events through this mapping. Honor local music volume
   and mute settings; host/client game-state synchronization must not wait on
   decoding or playback completion.
6. Verify that expected music starts at the original game events, loops and stops
   correctly, and continues to work with the ISO absent. Handle unavailable music
   explicitly and verify that disabling it does not change gameplay.

## Tests and format references

`tests/test_cd_audio.py` uses generated tones, no original audio. It checks profile
selection, bounds, overlap/path rejection, missing dependencies, decoded signal
level for both PCM byte orders, and Ogg duration/codec properties. Actual disc
conversion is an additional integration check, not a fixture distributed in Git.

Format references: [FFmpeg PCM formats](https://ffmpeg.org/ffmpeg-formats.html)
and [libcdio's description of BIN/CUE track layout](https://libcdio.github.io/libcdio.html#CDRWIN-BIN_002fCUE-Format).
