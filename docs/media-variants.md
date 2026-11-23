# Original media variants

The installer supports the original executable with SHA-256
`aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83`.
Directory import uses the same version gate as ISO import. Hashing an input
directory records its contents; it does not establish compatibility by itself.

## User-supplied US English directory, inspected 2026-11-23

The user identifies this copy as the US English edition. Read-only inventory
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

This variant remains unsupported. Do not add its hash to an acceptance list or
rename English assets to German paths to bypass the gate. Before enabling it:

- Compare simulation input tables and relevant executable rules by hash/version;
  do not apply German segment addresses to this EXE without checking them.
- Add explicit resource selection for the English language and art directory 6,
  covering text, town names, cards, menus, subtitles and scene assets. The current
  native entry point still expects `butt/1/map.bmp` and `dat/d/rally_d.dat`.
- Make preflight and installation verification accept the actual runtime layout
  of an explicitly supported profile instead of requiring the German overview.
- Verify one complete human turn and the installer smoke checks against the
  English copy using external installation/artifact directories.

This engine work is separate from the directory importer so it can be coordinated
with the ongoing game-window reconstruction.

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
