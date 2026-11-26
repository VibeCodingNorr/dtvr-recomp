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

See [music control research and Ogg mapping](music-research.md) for the traced
sequencer routines, provisional recording matches and the plan for install-time
sound-set rendering without runtime MIDI. Native start/Escape menu ideas are in
[pause-menu.md](pause-menu.md).

Complete bank-rendered Ogg sets, including cues absent from CD recordings, can
now be prepared with the [soundtrack builder](soundtracks.md). Native playback
and the original CD track numbers are still unverified/unimplemented.

1. Extend the first-pass music analysis: normal background/cue playback uses the
   sequencer; the separate CD helper has no found caller. Five background recordings
   and an intro excerpt have candidate matches. Exact disc track IDs, full cue
   meanings and music titles remain unverified; see the linked research.
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

## Sound set "CD-Aufnahmen + MIDI-Ergänzungen" (round 134)

The user recognised the CD recording as the music remembered from playing the
original (region 01 against our GM rendering of `g0`). `tools/cd_soundtrack.py`
composes a selectable set, source kind `cd-hybrid`, id `cd-hybrid`:

| Cue | Asset | Evidence |
| --- | --- | --- |
| `g0` | `segment-01.ogg` | pitch-class similarity 0.456 (runner-up `g4` 0.228), time scale 1.0 |
| `g1` | `segment-02.ogg` | 0.454 (`g0` 0.165), 1.0 |
| `g3` | `segment-03.ogg` | 0.587 (`g2` 0.349), 1.0 |
| `g4` | `segment-04.ogg` | 0.736 (`g0` 0.238), 1.0 |
| `g5` | `segment-05.ogg` | 0.647 (`g2` 0.382), 1.0 |
| the other 20 | the SoundFont rendering, marked `"source": "soundfont-render"` | no recording |

The scores were reproduced on 2026-11-23 with `tools/inspect_music.py` on the
imported segments of the supported image (same values as docs/music-research.md).
`segment-06` is only the first 109 s of the 434 s intro `i` and does not replace it;
`g2` and the short cues have no recording. A recorded cue ends with its media
(`nominal_end_seconds` = media duration, `nominal_end_source` `cd-recording-end`), a
rendered one 2 s before (the synth tail); the native catalog
(`src/music_catalog.cpp`) checks both and accepts recordings only in `cd-hybrid`
sets. The catalog keeps the CD layout's identity (`cd_audio`) and the supplement's
pack id, settings, SoundFonts and tools; every cue keeps its Ogg hash (recordings
also the source PCM hash). Unchanged: rally.exe plays its music from MIDI files
(the CD routine seg1:2B44 has no caller), original track numbers are unknown, and
the recordings are not stretched or cut.

The installer builds the set after a rendering when the image provides CD audio.
For an existing installation:

```sh
python3 tools/cd_soundtrack.py --music-dir /path/to/data/music \
    --render-pack /path/to/data/music/packs/rendered-gm \
    --output /path/to/data/music/packs/cd-hybrid
```

It is selected on the Audio page (F8) like any set; switching happens at the next
piece. Tests: `tests/test_cd_soundtrack.py` (generated inputs) and the `hybrid` /
`tail` / `recorded` packs of `tests/music_fixture.py`.

## Release-OGG (nicht original), US English retail

User decision (2026-11-26): a release of the US English retail edition that ships
the disc as a 2048-byte data image, a cue sheet and Ogg files is accepted as an
explicitly **non-original** CD music source. The Ogg files are lossy Vorbis
re-encodes (about 320 kb/s) made by that release, not the disc's CD-DA.
`tools/release_ogg.py` accepts them only under all of these conditions:

- the data image has the pinned SHA-256 below (`--iso`), and the folder passed
  with `--cd-audio-dir` holds the pinned cue sheet and six Ogg files;
- the extracted `rally.exe` is the US English retail EXE
  `a8374ca9e39d70b4508efefa782b7135c650792be36600289356a51de819225a` and
  `dat/lan.inf` starts with `E`. German images and executables are refused.

Without `--cd-audio-dir` the pinned data image installs too (edition `en-us-retail`,
`tools/editions.py`); its manifest then records `cd_audio.status: no-cd-da-in-image`
and the game plays MIDI only.

Files are copied unmodified, verified again after copying, and never transcoded.
The label "Release-OGG (nicht original)" appears in the installer log, in
`audio.json` / the manifest (`source_kind: release-ogg`, `original: false`,
`lossy_reencode_by_release: true`) and in the sound-set name
"Release-OGG (nicht original) + MIDI-Ergänzungen" (pack id `release-ogg-hybrid`,
source kind `release-ogg-hybrid`, cue source `release-ogg-recording`) shown on the
Audio page. The native catalog accepts `release-ogg-recording` cues only in such
sets and `cd-recording` cues only in `cd-hybrid` sets.

Provenance (fingerprints of user files, computed read-only 2026-11-26; no media is
in the repository):

| Input | Cue track | Installed as | SHA-256 |
| --- | --- | --- | --- |
| `track01.iso` | 01 (MODE1/2048) | (data import) | `26260860553361b356a8884b290c659ab6cedf2e29645fbcf2f8f0c511551fc7` |
| `track01.cue` | sheet | (not installed) | `11084c6c22bfa4a61cf184443753353c5049a0bd2f90743f9f3ac05c204484d9` |
| `track02.ogg` | 02 | `segment-01.ogg` | `89b3ec1c1c297fba257c68b3edd0ffe1417b8a5a399d8975c2709b3646827917` |
| `track03.ogg` | 03 | `segment-02.ogg` | `a129fe4e8d0a27895f04f29a48239583a8114915ccc36d10ce31668b14be2f8e` |
| `track04.ogg` | 04 | `segment-03.ogg` | `7111a2b40236f2236e5a3913004e59d24ef3839629e611c089593b86600ce819` |
| `track05.ogg` | 05 | `segment-04.ogg` | `66f41e296c78a8f019368f4bc44b9518a8be0a150f50d484bbce2742115d5d49` |
| `track06.ogg` | 06 | `segment-05.ogg` | `924fb31f515753583647d16a73421a5376bb976314a4553fe5a47d93f6cb9ab9` |
| `track07.ogg` | 07 | `segment-06.ogg` | `73c3794edc8c952491e5432fcee1bc22177d8f9e9c401dc5577154d892472781` |

The cue sheet types the Ogg files as `MP3` and gives track 02 `PREGAP 00:02:00`
(a virtual pregap, not contained in the file). It is the release's sheet, not a
factory TOC, so `original_track_numbers_known` stays false.

Mapping: track k+1 is the German observed region k (segment-0k above). A private
read-only comparison (envelope cross-correlation of German region k and release
track k+1, 2 kHz mono, 0.1 s RMS) gave r = 0.82, 0.89, 0.89, 0.94, 0.92, 0.91, all at
lag 0. The release tracks therefore get the same segment names and the same cue
mapping as the German set (`g0`, `g1`, `g3`, `g4`, `g5`; segment-06 is again only
an intro excerpt).

Measured lengths and silence (decoded with FFmpeg to s16le, silence = every sample
of both channels |x| <= 16; lengths are the Vorbis `duration_ts`):

| Segment | Samples | Media (s) | Leading silence (s) | Trailing silence (s) | Music end (s) | German region (s) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 01 | 7975044 | 180.840 | 0.039 | 1.964 | 178.876 | 178.853 |
| 02 | 8534820 | 193.533 | 0.039 | 1.965 | 191.568 | 191.547 |
| 03 | 8755908 | 198.547 | 0.039 | 1.968 | 196.579 | 196.560 |
| 04 | 6993672 | 158.587 | 0.039 | 1.961 | 156.626 | 156.600 |
| 05 | 7802172 | 176.920 | 0.039 | 1.962 | 174.958 | 174.933 |
| 06 | 4986828 | 113.080 | 0.039 | 4.177 | 108.903 | 109.080 |

Each media length is a whole number of CD sectors (588 samples): 13563, 14515,
14891, 11894, 13269 and 8481 sectors, 149 sectors (1.987 s) longer than the German
region for 01–05 and 300 sectors for 06. The trailing silence is the zero separator
that the German import omits; at threshold 0 it is 1.935–1.950 s (06: 3.980 s), the
rest is the decay of lossy coding noise. Segment 06 ends 0.18 s before the German
region's end at threshold 16, consistent with a fade into noise; not investigated
further.

Gap handling, as in the German set: the German regions contain no separators and
a recorded cue ends with its media. The release files keep their separator (no
cutting), so each cue's `nominal_end_seconds` is the measured music end and
`trailing_silence_seconds` the remainder; the catalog requires
`media_duration_seconds - nominal_end_seconds = trailing_silence_seconds` (±0.05 s,
at most 6 s). The 0.039 s leading silence is not skipped (the catalog has no start
offset); it is within the lossy-coding difference to the German regions.

## Tests and format references

`tests/test_release_ogg.py` generates tone Ogg files with FFmpeg (skipped without
it) and checks the pinned profile, the edition gate (German EXE, letter `D`),
cue parsing, hash and cue-sheet mismatches, missing/linked files, byte-identical
copies, the `audio.json` marking, the hybrid catalog mapping (track → segment →
cue, music end) and the preflight refusals. `tests/music_fixture.py` packs
`release`, `release-silence` and `release-kind` check the native catalog.


`tests/test_cd_audio.py` uses generated tones, no original audio. It checks profile
selection, bounds, overlap/path rejection, missing dependencies, decoded signal
level for both PCM byte orders, and Ogg duration/codec properties. Actual disc
conversion is an additional integration check, not a fixture distributed in Git.

Format references: [FFmpeg PCM formats](https://ffmpeg.org/ffmpeg-formats.html)
and [libcdio's description of BIN/CUE track layout](https://libcdio.github.io/libcdio.html#CDRWIN-BIN_002fCUE-Format).
