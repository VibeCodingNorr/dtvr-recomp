# Native Ogg music

The game window (`--game`) plays installed complete Ogg Vorbis sound sets through
the same 44,100 Hz stereo SDL device as WAV effects. Runtime MIDI, synthesis,
FFmpeg and the original CD are not required. Users prepare their own music with
the [installer or soundtrack builder](soundtracks.md). No music or samples ship
in the source repository or setup AppImage.

The installer now prepares all original MIDI cues by default with a detected
local GM bank, or asks for one if it is missing. Music is skipped only by explicit
choice (`--no-music` / **Musikvorbereitung überspringen**). This installs the pack where
the game discovers it; archival CD recordings alone are not a playback pack.
The supplied bank determines instrument timbre, so this is not a claim of exact
historical hardware sound. Existing installations need an explicit update with
music preparation to acquire a previously missing pack. Skipping preparation on
an update preserves existing packs and does not modify playback preferences.

## Choosing music

Press **F8** from the game or its title/options menus. The options window also
shows this shortcut. In the audio overlay:

| Key | Action |
| --- | --- |
| Up / Down | Select an installed complete sound set |
| Left / Right | Adjust music volume by 5 percent |
| Space | Enable/disable music (the original music option) |
| Escape / Enter / F8 | Return to the previous screen |

The overlay displays the selected set and any pending change. A new set takes
effect at the next cue boundary; positions are never copied between different
render timelines. A volume of zero preserves progression; disabling music stops
it, and enabling starts a new background selection. Normal menus leave music
playing. Losing application focus pauses the exact current cue and position;
regaining focus resumes it. The overlay is local presentation, not saved rule
state. It blocks local input/AI like other current single-player modal dialogs;
network pause policy is not implemented.

Set ID and volume are stored separately in
`$XDG_CONFIG_HOME/dtvr-recomb/music.ini` (default `~/.config/dtvr-recomb/music.ini`).
The enable switch remains `[Setup] Midi` in `settings.ini` for compatibility with
the existing options UI; it now controls Ogg playback. No settings affect RNG.

## Installed catalogs

The game discovers `data/music/packs/<id>/pack.json` beside `data/program`.
Only complete catalogs with all 25 cue IDs and local, regular Ogg files are
selectable. The native reader bounds JSON size/depth, validates paths, playlist,
timelines, Vorbis headers, stereo/rate and duration. Partial/linked/malformed
packs are excluded with diagnostics in the overlay and console. Missing music
does not prevent playing the game. The six provisional CD recordings alone are
not a complete set and are not silently substituted for cue files.

For full SHA-256 and complete file-decoding verification, run
`python3 tools/build_soundtrack.py --verify /path/to/pack`. Discovery checks
metadata/headers rather than decoding and hashing every complete recording at
startup. A subsequent decoding error is visible and moves to the next background;
six consecutive failures stop playback. The game never executes catalog strings
or attempts to download a missing bank or track.

## Control and original evidence

The [music research](music-research.md) records original executable addresses.
Implemented triggers are startup `i`, game start/load `a1`, destination `z1/z2`,
winner `s`, Dr. Drago `f1/f2/f3`, and the recovered event cue families. Destination
selection retains the existing `random(1)` result rather than drawing again.
The background playlist cycles `g0` to `g5` on successful completion. Stop,
disable, focus pause, errors and replacement are not successful completion.
The separate positive six-player-change trigger uses `EventClock::turns` and a
local six-change background hold; it never advances a game counter.

Further caller tracing identifies the named event cues:

| Cue | Presentation | Original call |
| --- | --- | --- |
| e1 | Storm (category-4 town damage) | seg8:1D03 |
| e2 | Robbers | seg8:20D6 |
| e3 | Nessie | seg8:25DE |
| e4 | Volcano | seg8:3970 |
| e5 | Earthquake | seg8:3E52 |
| e6 | Rain | seg8:4527 |
| e7 | Flood | seg8:4C01 |
| e8 | Snow | seg8:5638 |
| e9 | Rockfall | seg8:5C49 |
| e11 | Eastern borders opening | seg8:6A10 |

These are routed from the corresponding message families, when the native event
presentation is queued. There is no invented drought/e10 trigger. The form-change
branches at seg2:B3F6/B429/B442 map new Drago forms 1/2/0 to f2/f3/f1;
the successful Hau ab transfer at seg2:B53B requests f3. Winner music starts with
the winner presentation (seg7:71DC). Cue positions use native PCM frames, not
unverified original MCI position units.

## Streaming and limits

`music_catalog` handles discovery/preferences, `music_player` handles control,
`music_stream` owns decoding, and `music_menu` owns the local overlay. Decoder/file
work runs on a worker with a half-second PCM ring. The audio callback performs no
file I/O, allocation or waiting: it consumes buffered samples or emits silence.
All sources are summed and clipped in one device; closing the device precedes
destroying the music stream. A missing audio device leaves the rest of the game usable.

Completion uses each catalog's nominal SMF end. The extra two-second synthesis
tail is not added to playlist timing. Short 20 ms fades soften boundaries;
crossfading overlapping reverb tails and sample-exact original transition timing
remain unimplemented. Original cue timing relative to all animation/dialog
boundaries, especially stage announcements and Drago's explicit return-to-background
call at seg2:B578, still needs integration as those presentations are reconstructed.
The player supports explicit background return at a remembered position, but does
not pretend that all original callers or nested interruptions are already mapped.

Build/runtime dependencies are libvorbisfile and json-c >= 0.15, plus SDL2. The
installer checks development files before compiling. Native decoding follows the
[Xiph Vorbisfile API](https://xiph.org/vorbis/doc/vorbisfile/reference.html);
catalog parsing uses [json-c](https://json-c.github.io/json-c/).

## Verification

CTest runs control tests without original data and, when FFmpeg/Python are
available, generates stereo tones outside the checkout for native catalog and
streaming tests. Coverage includes natural completion vs cancellation, nominal
end vs render tails, pause/resume/seek, mute, decoder recovery, pack boundaries,
preferences and rejected incomplete/linked/corrupt/unsafe catalogs. Local Xvfb
checks additionally exercise F8, set selection, volume persistence and rendering
against externally stored real sets. Dummy audio validates decoding/mixing, not
listening quality on physical hardware.
