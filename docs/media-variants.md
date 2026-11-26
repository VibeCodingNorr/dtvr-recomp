# Original media variants

The installer supports the original executable with SHA-256
`aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83`.
Directory import uses the same version gate as ISO import. Hashing an input
directory records its contents; it does not establish compatibility by itself.

## Edition profiles

The runtime keeps one explicit profile per supported executable
([src/edition.hpp](../src/edition.hpp), tests in `tests/edition_test.cpp`). Addresses
below are `D` = German, `R` = US English retail.

| Profile id | EXE SHA-256 | lan.inf | Canvases | Runtime state |
| --- | --- | --- | --- | --- |
| `de-retail` | `aa3dde36…6b83` (438272 B) | `D` | 640, 800, 1024 | playable (reference of the rebuild) |
| `en-us-retail` | `a8374ca9e39d70b4508efefa782b7135c650792be36600289356a51de819225a` (438272 B) | `E` | 640, 800, 1024 | playable (round E2) |

**Detection.** Only the executable's SHA-256 identifies an edition, never the
presence of files. The runtime takes it from `rally.exe` in the program directory
(any case, e.g. an extracted disc), else from the top-level `executable_sha256` of
`<install>/manifest.json` when the program directory is `<install>/data/program`
(the installer removes the EXE from installed data). An optional manifest field
`edition` must name the same profile. Unknown hashes, contradicting sources and
directories without either source are refused with a German message. The profile
then requires its files (`requiredFiles`, looked up without regard to case) and a
`dat/lan.inf` letter of its language.

**Playable (round E2).** The retail audit (2026-11-26, private notes) found the German
rules and tables with English resources. One complete human turn was then validated
with the retail program data (scripted `--game --press` run, 640 canvas, seed 5, two
humans): title → player setup → stage count → destination announcement, and with
`--no-title` the turn menu (English menu texts), the slot machine, driving with a held
arrow (plane on the sea route), the field (`Lose`, payment and the storm event text in
English) and the player change to the second driver. The same script on the German
data printed an identical simulation log (start, destination, hand, spin, payment,
damage town, next player), as expected from identical rules. Screenshots stay in the
private analysis directory. `--game` and `--play` therefore accept `en-us-retail`; the
developer option `--dev-unfinished-edition` remains for profiles not yet released
(none at present). The installer accepts both editions (docs/installer.md, "Editions").

**Verified US retail differences used by the runtime** (disassembled with
`tools/ne_disasm.py --use32 9`; private output):

- Canvas choice R seg5:1239–135B equals D seg5:1227–1349 with identical immediates
  and DS operands (only far call/jump targets differ); the disc ships `BUTT/1`, `BUTT/6`
  and `BUTT/8` with `MAP.BMP`. `BUTT/6/MAP.BMP` and `BUTT/1/MAP.BMP` are byte-identical
  to the German files.
- Setup tables: driver buttons R DS:1BD6 (read at R seg1:058F, 064D, 07F6, AE92, B056)
  equal D DS:1BD4. Name fields R DS:1CC6 (R seg1:06E6) equal D DS:1CC4 except the
  640 fields of slots 6 / 5 / 4: y 236 / 155 / 73 instead of 235 / 154 / 74
  (word-by-word comparison of both data segments).
- Empty save slot: D shows `leer` (DS:80D7, default of GetPrivateProfileString);
  R seg8:B7EE–B7FC calls LoadString(DS:B5C4, 11, buffer, 20), so string 11 clipped to
  19 bytes ("empty" in the retail language file).
- Credits (command 1026): D seg4:0720 passes `Credits.bmp` (DS:009A) to seg6:0A4D;
  R seg4:0701–070F formats `%c\credits.bmp` (DS:009A) with the letter DS:A0F0, giving
  `butt/<n>/e/credits.bmp`.

- Voice / subtitle: D seg5:219C / 21A5 compare the letter DS:A0F0 with `E` / `e` and
  then force voices off and subtitles on; the retail seg5 has no such comparison, so
  voices and subtitles stay as configured. The rebuild does not apply that German rule
  either, so nothing changes in the code.
- `dat/e/sync.dat` has 139 records of 0xD0 bytes (German 140); `nicosia.wav` has none.
  `parseVoiceSync` accepts any multiple of 0xD0 and a voice without a record shows no
  subtitle (no timing); how rally.exe behaves then is not checked.

- Fresh `europe.map` (user decision 2026-11-26, both editions): R seg5:182F reloads
  `map/europe.map` at a new or loaded game (called from R seg5:186A with argument 1 and
  from the loader R seg8:AF67 before the East re-opening seg8:69F0); D loads it once per
  run (D seg5:1559). The rebuild reloads it for every new or loaded game in both
  profiles, Blue Byte's later fix and a deliberate deviation for the German profile
  (docs/save-format.md, "Map state between games").
- Language case comparisons, rechecked in the retail disassembly: R seg1:766F / 77C4
  compare DS:A0F0 with `d` only (Statistik header: `E` takes 0x7A / 0x92 like the
  German `D`), R seg7:7EC1 draws `s_bann_d.bmp` only for `d` / `D` (no banner with
  `E`). Both were already rebuilt (`estates_view.cpp`, `victory_view.cpp`).
- Window title: D seg5:1BCF loads string 10 of the language library (DS:B5C4) as the
  CreateWindow title (seg5:1C06): `D.T.V.R.` in `rally_d.dat`, `Dr. Drago's Madcap
  Chase` in `rally_e.dat`. The game window takes it from the language file (it showed a
  fixed German name and then the debug title before).
- Saves record the edition (format 8, docs/save-format.md, "Editions"); a save of the
  other edition is refused.
- Driver names: strings 120–127 are identical in both language files (Helmut Pohl …);
  the card pictures, menus, messages, town names and destination photos / voices come
  from the English files. Only the recomp's own additions (e.g. the F9 option line in
  the player setup, the recomp menu) stay German by project policy.

Observed only: `map/europe.str` is byte-identical to the German
file, `map/europe.map` differs (SHA-256 `5afb8c52…` vs German `91779387…`);
`dat/e/rally_e.dat` has SHA-256 `d17a203f…`.

**Known, unsupported: the official English demo (Demo Version, 27-MAY-95).**
`RALLY.EXE` `c8b7dab6574047c6e3a3d4e600dd3f2ed23e90bd8a10de8a1bb891688d0624d9`
(fixed route, two-year end, no save/load, 640 only) is Blue Byte's official English
demo of "Dr. Drago's Madcap Chase": its `softinfo.wri` states "Release Type: Demo
Version", "Program/Release Date 27-MAY-95", "Language ENGLISH", Blue Byte Software
GmbH, and its `readme.wri` says "DEMO VERSION". Independent source (audit 2026-11-26):
the PC Review Cover CD #14 (August 1995), volume ID `PCR_0895`, raw Mode 1 BIN with
SHA-256 `c5236fc0cd88a6bcbcb5db98435dac6b296f570c6023fa00d33f066a3b10ba13`, holds it
below `rally/program/`; its 529 program files are identical to the copy inventoried
below. By user decision (2026-11-26) the demo stays unsupported and has no profile.
The runtime (`knownDemoForExecutableHash` in src/edition.cpp) and the installer
(`KNOWN_DEMOS` in tools/editions.py, also for a demo below another directory of a
cover disc) recognise its hash and refuse it with "Demoversion von Dr. Drago's
Madcap Chase (offizielle englische Demo, 27-MAY-95) – nicht unterstützt" instead of
an unknown-version error.

## User-supplied US English directory (English demo), inspected 2026-11-23

The user identified this copy as the US English edition; it was later found to be the
official English demo (see above) and is not supported. Read-only inventory
found 529 regular files totaling 24,122,697 bytes in the program directory.
Files are directly in that directory, with uppercase on-disk names; paths below
are normalized to lowercase. No original files or generated inventory are stored
in the checkout. This inspection did not run the Windows executable.

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `rally.exe` | 435712 | `c8b7dab6574047c6e3a3d4e600dd3f2ed23e90bd8a10de8a1bb891688d0624d9` |
| `dat/e/rally_e.dat` | 61952 | `e668bf3f4b7f06062a2c3b73cb3410539ce00b7ac50c9351a70eab4c750128ad` |
| `butt/6/map.bmp` | 187702 | `b4b2a45353e99f6ccce04c1d898c5ddfa8ff3ad6283c2ce2c784bf21c88ee8a4` |
| `map/europe.map` | 262144 | `fecad49a92483dae4f3afbf41054313db7df938fc130e2f641c37db4f3d3183a` |
| `map/europe.str` | 14476 | `8d8b7f36708dc7efe3db5a3a24cd67f29a5ca78e94a71dc29453b49a16295bc3` |
| `map/europe_e.tow` | 44880 | `68008ded4fd39feb560a80edd0ae37a445f741866ae32af3aa584b24229dc45f` |

`inspect_game.inspect_ne` recognizes the EXE as Windows NE, with eleven segments
and entry point segment 10, offset 658 (`0x0292`). Segment 9 has file offset
128512 and size 362; segment 11 is data, at file offset 144384, size 46422.
These header observations do not establish equivalence with the supported EXE's
instructions or segment-relative addresses.

Present resources include `dat/e/sync.dat`, `dat/lan.inf`, `butt/6/e/`,
`butt/e/`, `map/0/part0000.sbb`, `sbs/d.sbs` and all 25 MIDI cue filenames
used by soundtrack preparation. `butt/1/map.bmp` and German language resources
are absent. No disc image or CD audio layout has been established for this copy.

## US English integration handoff

The user explicitly agreed on 2026-11-23 that the US English edition should be
supported in the future and requested this handoff to Claude. This is a planned
compatibility goal, not a request to interrupt the current German game-window
reconstruction. Keep the importer work separate and schedule engine integration
with the existing owner. See also [architecture](architecture.md#editions-and-languages)
and the standing project instructions in `AGENTS.md`.

The English demo above stays unsupported; the retail edition is supported by the
runtime and the installer since round E2 (see [Edition profiles](#edition-profiles)).
Do not rename English assets to German paths. The handoff checklist, now done:

- Compare simulation input tables and relevant executable rules by hash/version;
  do not apply German segment addresses to this EXE without checking them.
- Resource selection: the language letter comes from `dat/lan.inf` (see below) and
  the entry point checks the profile's required files instead of `butt/1/map.bmp`
  (done, round E1). Text, town names, cards, menus, subtitles and scene assets follow
  the letter; their English contents were checked in round E2.
- Make preflight and installation verification accept the actual runtime layout
  of an explicitly supported profile instead of requiring the German overview
  (done, round E2: `tools/editions.py`).
- Verify one complete human turn and the installer smoke checks against the
  English copy using external installation/artifact directories (done, round E2).

This engine work is separate from the directory importer so it can be coordinated
with the ongoing game-window reconstruction.

**Language letter (supported German EXE, checked 2026-11-23 after a user hint).**
seg5:0239 formats `%s\lan.inf` (DS:0118) with the `dat` directory DS:7E, reads it
(seg6:027F) and keeps its first byte as the language letter DS:A0F0; seg5:02B1
then loads `Rally_%c.dat` (DS:0123) with that letter as the language library
(seg5:1B6B formats `%s\%c\%s` (DS:02ED) with DS:7E and calls LoadLibrary; handle
DS:B5C4, below 32 = failure). The German copy holds `D` + CRLF (bytes `44 0D 0A`)
and the directory `dat/d/`; the user reports `E` for English with `dat/e/`.
seg5:028C is the only write of DS:A0F0. Its 46 reads build `dat\%c\sync.dat`
(seg1:31D4), `%s\%d\%c\menu.bmp` (seg1:AD9B), `%s\%d\%c\%dcard_%.2d.bmp` (seg2:10D6,
24AE, 2583, seg3:0474, 11EE), `%s\%c\bcards.sbs` (seg3:449F), `%s\europe_%c.tow`
with the map directory (seg5:15A6), `%s\%c\views\%s` (seg6:4E6A), the intro,
destination, Sieg and Bilanz voices plus `zeitung.bmp`, `banner.bmp`,
`ziel\%s.bmp` (seg7, seg8:8C66 ff.), `%s\%d\%c\t.bmp` (seg8:04A7) and the frame
name `dis_%c` (seg1:1908, DS:32CB). Four places compare the byte with case:
seg1:766F / 77C4 (Statistik header: the text x advances by 0x84 / 0x6A for `d`
only, else 0x7A / 0x92, so the German `D` takes the second pair), seg7:7EC1
(`s_bann_d.bmp` only for `d` or `D`) and seg5:219C (`E` / `e` forces subtitles and
turns the voice off).

Failure: when lan.inf cannot be opened, seg6:027F returns 0, seg5:0225 calls the
ERROR dialog seg6:0639 (template DS:164B) with the path and returns 0, and
seg5:0058 ends the start. seg6:027F allocates size + 1 bytes with GlobalAlloc
flags 0x42 (moveable, zero-initialised), so an empty file yields the letter 0, the
library name becomes `Rally_` and its load fails in the same way.

Rebuild (done): `src/data_paths.cpp` (no SDL, `tests/data_paths_test.cpp`) reads
the letter once; `src/main.cpp` reads it together with the board and passes it to
the play view, the towns file `map/europe_<c>.tow`, `runGameWindow` and through
`GameScreen` to every view that builds a language path. Paths use the lower-case
letter with the case-insensitive lookup; the stored byte is kept for the case
comparisons (Statistik spacing, Sieg banner). A missing, empty or unreadable file
stops the program with an error, as rally.exe does. Rebuild decision: a first
byte that is not an ASCII letter is rejected at once, whereas rally.exe would use
it and only fail if no `dat\<byte>\Rally_<byte>.dat` loads. Not rebuilt: the
`E` / `e` subtitle rule of seg5:219C (only relevant with an English library). This
does not lift the version gate; the English EXE stays unsupported.

## German installed-directory example, inspected 2026-11-23

A second user-supplied example contains a DOSBox/Windows environment rather than
the game files at its root. The game is under `disk/SPIELE/DTVR/`, and an original
disc image is present separately as `disk/dtvr.iso`. These are relative paths
within the supplied example, not installer search rules. No wrapper scripts or
Windows executables were run.

The game directory contains 434 files totaling 11,676,220 bytes. Its 438272-byte
`rally.exe` matches the already supported SHA-256 above. It has German resources
under `dat/d/`, but no `butt/` directory. This installed copy alone is insufficient
for import; the importer reports the missing overview rather than accepting the
executable hash as proof that all required assets are present.

The accompanying image has SHA-256
`a94cb115097c22b539563dac1954e965a5ee5f812070d70beeb39906a4197fee`
and 2352-byte sectors. The ISO reader finds 962 program files and the required
overview, map tile and sprite inputs. Of those files, 531 are absent from the
installed directory (the `butt/` tree, `wavemix.dll` and `wavemix.ini`). All 431
shared files match the ISO by size and SHA-256. The remaining three files in the
installed directory are not used to establish disc compatibility.

Read-only ISO preflight succeeded against a fresh external destination, with CD
audio profile support recognized and no installation directory created. This
check skipped MIDI preparation dependencies; it did not build or publish an
installation. The appropriate input for this example is its ISO, explicitly
selected via `--iso`, rather than the installed game folder or the whole
DOSBox/Windows environment.

## US English retail release with Ogg CD music (audio only)

A US English retail release (data image `track01.iso`, SHA-256
`26260860553361b356a8884b290c659ab6cedf2e29645fbcf2f8f0c511551fc7`, 2048-byte
sectors, `rally.exe` SHA-256
`a8374ca9e39d70b4508efefa782b7135c650792be36600289356a51de819225a`, `dat/lan.inf`
`E`) ships its CD music as a cue sheet and six lossy Ogg Vorbis files re-encoded by
that release. The image holds no CD-DA, so `cd_audio.py` reports `no-known-layout`.
By user decision these Ogg files are an accepted, explicitly **non-original** music
source for this edition only: `tools/install.py --iso … --cd-audio-dir …` checks the
pinned hashes and copies them unmodified, labelled "Release-OGG (nicht original)".
The six tracks (cue tracks 02–07) carry the same six recordings as the German
regions 01–06. Hashes, mapping and measured silence: [CD audio](cd-audio.md#release-ogg-nicht-original-us-english-retail).

This does not lift the version gate: the installer still rejects this executable
until the English edition is supported, and German images refuse `--cd-audio-dir`.
