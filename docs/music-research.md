# Original music control and Ogg cue mapping

Status: static executable analysis and reproducible audio/MIDI comparison. No
runtime playback or installer synthesis is implemented here. Research is isolated
from the engine/refactoring files. Original bytes, note sequences, recordings and
disassembly are not included in this document or repository.

Input executable SHA-256:
`aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83`.
Disc identity and provisional sector ranges are in [cd-audio.md](cd-audio.md).
Addresses below use NE segment numbering; `DS` is automatic data segment 11.

## Main finding: MIDI is the demonstrated music path

The normal music routines open the sequencer device, select files under
`snd/midi`, and request completion notifications to the main window. They do not
select CD tracks. `[Setup] Midi`, default 1, is loaded into `DS:B5D2` at
`seg5:1FB0–1FC2`; MIDI-device availability can disable it at `seg5:2100–2112`.

There is a separate CD routine at `seg1:2B44–2B99`. It opens the CD device, sets
TMSF addressing and requests the interval from track 3 to track 4, without a
completion notification. No references to this routine were found in the NE
entry table, internal fixups/function pointers or direct calls of this executable.
This supports an **apparently unused CD test/helper**, not a demonstrated
alternative soundtrack implementation. Static inspection does not rule out every
possible indirect or dynamically constructed call.

Consequently, do not assign the six extracted regions to CD tracks 2–7 or treat
the isolated track-3 command as the production mapping. CD recordings can supply
Ogg versions of matching cues, but they do not replace all sequencer files.

## Confirmed routines and state

| Entry / evidence | Observed behavior | Native implementation consequence |
| --- | --- | --- |
| `seg1:275E` | Choose `g0`…`g5` in order, copy the selected name into background/current-name buffers, start at zero with completion notification, increment index and wrap after 5. | A six-cue cyclic playlist; no same-track infinite loop is demonstrated. |
| `seg1:2889` | Reopen the saved background name from `DS:3314`, start at saved position `DS:3312`; use the next background track if no name is available. | Explicit return-to-background operation. |
| `seg1:293F` | Query current position into `DS:3330`, then close the sequencer. | Preserve a separate activation/focus suspension position. |
| `seg1:299A` | Reopen the current name from `DS:3322` at `DS:3330`; fall back to the next background track if empty. | Resume the currently interrupted cue, not always a new playlist item. |
| `seg1:2A30` | Save current position into `DS:3312`, stop/close, replace current name with the supplied cue and play from zero with notification. | A named interrupting cue; no explicit same-cue repeat flag. |
| `seg1:2AF3` | Notification flag 1 starts the next playlist item; flags 2, 4 and 8 do nothing. | Natural completion advances music; cancellation or failure must not be treated as completion. |
| `seg4:002E–0042` and dispatcher `seg4:04A5–0522` | Main window routes message `0x3B9` to the notification handler. | Map local decoder completion to a music event, not a gameplay turn. |
| `seg4:0CAE–0D80` | Application activation/deactivation resumes or suspends audio and calls the above save/restore routines. | A local focus/pause policy with remembered media position. |
| `seg1:B342–B384` | Music option toggles playback: enabling starts a new background selection; disabling temporarily enables the flag while saving/closing, then clears it. | Distinguish mute, disable, stop and resume in the native design. |
| `seg5:23E7–23FC` | Shutdown closes the active sequencer when music is enabled. | Release native audio resources on exit. |

The playlist's next-index byte is `DS:3332`, initially zero. Its six name pointers
refer to the technical cue names `g0` through `g5` at `DS:3336–3354`.
Starting a background piece sets `DS:A111` to 6 (`seg1:27AE`). The game decreases
that counter at `seg8:1638–164C` and can request another background selection at
`seg2:083C–085F` when the separate counter `DS:A0FE` is positive and divisible by
6 and `DS:A111` is zero. This additional game-progress trigger must be preserved;
do not implement only end-of-file advancement. Its complete gameplay context is
not yet reconstructed, so this is not labelled a six-second timer or six turns.

Saved original positions are 16-bit integers obtained through a decimal parse.
No explicit sequencer time-format command was found in these routines. Do not
assume those integers are milliseconds or seek Ogg by passing them through
unchanged. Native cue positions need a defined timebase and, if original position
compatibility is needed, conversion through the original MIDI tempo map.

## Named cue call sites

This is a code-backed cue inventory, not a complete semantic event dictionary.
Case differences reflect original call strings; lookups on Linux must account
for the lowercase imported filenames.

| Cue | Call sites | Context / certainty |
| --- | --- | --- |
| `I` | `seg5:0162` | Startup, before optional intro-window creation at `seg5:0188`. |
| `A1` | `seg4:0E8A`; `seg2:0788`, `1449`, `147C` | Startup-to-game transition and other game-progress paths; exact semantics of every path remain open. |
| `Z1`, `Z2` | `seg2:11C5`, `12ED`, `12FC` | Destination/stage-related paths; the latter two branches choose between the two cues. |
| `S` | `seg7:71DC` in the helper called from `seg2:12B4` | Winner/end-of-game presentation path; subsequent routine builds the podium. |
| `F1`, `F2`, `F3` | `seg2:B442`, `B3F6`, `B429`, `B53B` | Three branch-selected cues. Naming their game events needs further caller tracing. |
| `e1`, `e2`, `e3` | `seg8:1D03`, `20D6`, `25DE` | Event routines. |
| `e4`, `e5`, `e6` | `seg8:3970`, `3E52`, `4527` | Event routines. |
| `e7`, `e8`, `e9`, `e11` | `seg8:4C01`, `5638`, `5C49`, `6A10` | Event routines. |

No direct call with `e10` was found by tracing the named-cue routine's internal
fixups. The file exists; do not conclude it is used simply because it is present.
Several short event MIDI files are byte-identical; cue identity still matters
for control flow and future version support.

## Recording-to-cue comparison

`tools/inspect_music.py` parses SMF timing and, optionally, compares pitch-class
energy over time against decoded Ogg segments. The method uses 11,025 Hz mono,
4096-sample Hann windows, 2756-sample hops, 12 pitch classes and non-percussion
MIDI notes with a simple five-harmonic model. It searches time scales 0.65–1.35
and offsets −3 to +5 seconds. These are **similarity rankings, not probabilities**,
hardware identification, exact alignment markers or verification of disc TOC.
The model ignores sustain-controller state, pitch bends and device effects. The
research tool compares at most the first 600 seconds of an audio file; all six
recordings here fit within that limit.

Local comparison against all 25 MIDI files produced:

| Imported recording | Best MIDI match | Similarity | Runner-up | MIDI end time | Ogg duration |
| --- | --- | --- | --- | --- | --- |
| `segment-01.ogg` | `g0` | 0.456 | `g4`: 0.228 | 179.996 s | 178.853 s |
| `segment-02.ogg` | `g1` | 0.454 | `g0`: 0.165 | 192.055 s | 191.547 s |
| `segment-03.ogg` | `g3` | 0.587 | `g2`: 0.349 | 197.235 s | 196.560 s |
| `segment-04.ogg` | `g4` | 0.736 | `g0`: 0.238 | 157.030 s | 156.600 s |
| `segment-05.ogg` | `g5` | 0.647 | `g2`: 0.382 | 176.723 s | 174.933 s |
| `segment-06.ogg` | `i` (opening excerpt) | 0.495 | `g1`: 0.117 | 433.933 s total | 109.080 s |

All best matches use time scale 1.0. The five background matches have closely
matching musical lengths as well as matching pitch-class progressions. The
intro match covers only the recording's roughly 109 seconds; the complete MIDI
is much longer. There is no matched CD recording for `g2` (136.800 s), nor for
the short effects/destination/winner cues. Lead-in and tail timing differs, so
none of these scores justifies using the recordings as sample-exact replacements.

The matching pitch progressions also support the currently chosen big-endian PCM
interpretation for this image. Hardware, original track numbers and exact sector
boundaries remain unverified. Keep installed segment naming provisional until
the playback catalog explicitly maps cue identities to usable assets.

### Reproduction

Only timing inspection needs Python's standard library. Optional audio comparison
requires NumPy and FFmpeg as research tools; neither is a native playback dependency.
Provide your own extracted MIDI directory and locally installed Ogg directory:

```sh
python3 tools/inspect_music.py --midi-dir /path/to/extracted/program/snd/midi
OPENBLAS_NUM_THREADS=1 python3 tools/inspect_music.py \
    --midi-dir /path/to/extracted/program/snd/midi \
    --audio-dir /path/to/installation/data/music > /path/to/private-analysis/music-report.json
```

The report contains hashes, durations and similarity summaries, not note sequences.
For executable evidence, run the existing NE disassembler into an external private
directory and inspect the address ranges above. Synthetic parser checks are in
`tests/test_music_inspection.py` and run via unittest discovery without game data.

## Multiple historical sound styles, without runtime MIDI

The proposed runtime consumes **Ogg cue assets only**. At installation/import time,
the user can render original MIDI sources with a selected synthesizer and supplied
SoundFont. [FluidSynth's file renderer](https://www.fluidsynth.org/api/FileRenderer.html)
supports offline MIDI-to-audio rendering; its
[SoundFont requirements](https://www.fluidsynth.org/wiki/GettingStarted/) are SF2/SF3.
The offline renderer and installer option are now implemented; see
[soundtrack preparation](soundtracks.md). Native playback/menu selection remain pending.

| Soundtrack option | Proposed construction | Limitation |
| --- | --- | --- |
| CD recordings + rendered gaps | Use the five matched background recordings; render `g2`, full intro and other cues from original MIDI. | A mixed timbre is possible; boundaries and cue alignment need review. |
| General MIDI render | Render every cue using one explicitly supplied GM SoundFont and fixed settings. | Consistent timbre, not proof of a particular historical device. |
| Sound Canvas / GS-style render | A separately supplied, suitable renderer/bank, with the same complete cue set. | Track labels mention SCC-1, but no exact playback device or authentic hardware rendering has been established. |
| Other supplied sound sets | Re-render the same original cues with another compatible bank. | Display as that bank's sound; do not claim hardware authenticity without verification. |

Do not equate Roland MT-32 with Sound Canvas/GM. Roland documents a distinct
[MT-32 sound arrangement on Sound Canvas](https://cdn.roland.com/assets/media/pdf/SC-55_OM.pdf).
Compatibility must be established before offering an MT-32 preset. Source notes
and intended programs must be preserved; swapping instrument banks blindly may
change the arrangement.

Each rendered pack should have a complete cue catalog, source MIDI hashes,
SoundFont/renderer identity and hashes, rendering settings, tempo-map identity,
defined cue lengths/tails and any verified seek/loop markers. Encode only once
from the rendered PCM. Keep all input banks, music and generated files external;
no instrument samples or hardware ROMs are bundled by this project.

The pause/settings menu may select only installed complete packs. Use one cue ID
and control-flow contract across packs. Switch packs at a cue boundary, or use a
verified timeline mapping when resuming mid-piece; do not reuse raw time offsets
across a shorter CD recording and a full MIDI rendering. Decode/seek/volume stay
local to each client and must not advance gameplay or the network turn state.

## Remaining work before playback integration

- Validate candidate matches by listening and compare intro timing with original
  presentation; recover exact end/tail boundaries if reliable disc metadata appears.
- Trace all event meanings and nested interruption behavior. The two saved
  positions in the original are not a demonstrated arbitrary-depth audio stack.
- Recover the sequencer position units and original timing around short cues.
- Implement and verify offline rendering as a separate installer stage, then a
  complete installed cue catalog and native streaming playback.
- Test natural end vs cancellation, background progression, temporary cues,
  focus suspension and menu settings with generated audio fixtures first.

API interpretation references: Microsoft's
[MCI completion notification](https://learn.microsoft.com/en-us/windows/win32/multimedia/mm-mcinotify)
and [sequencer time-format command](https://learn.microsoft.com/en-us/windows/win32/multimedia/set).
