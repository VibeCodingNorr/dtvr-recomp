# Native save format

DTVR Recomb stores games in its own versioned text format (`src/save.cpp`),
independent of the original game's save files, as agreed in
`docs/architecture.md`. A save holds the complete simulation state, so loading it
and continuing with the same commands gives the same game as continuing the
original (verified every turn in full computer games on the original data).

## Layout

UTF-8/ASCII text, one record per line, fields separated by single spaces.
Doubles are written as C hexadecimal floating point (`%a`) for an exact round
trip. Records may appear in any order after the header; `end` closes the save.

| Record | Fields |
| --- | --- |
| `DTVR-RECOMB-SAVE` | format version (currently 4; 3 is read without `stages`, 2 also without `demo`, 1 also without `risiko`); always the first line |
| `rules` | rule revision of the reconstruction (`kRulesRevision`); newer ones are refused |
| `data` | 64-bit FNV-1a fingerprint of the board and town records (hex) |
| `rng` | state of the MS C random generator |
| `player` | slot, status (0 inactive, 1 human, 2 computer), flags, field (8 lines) |
| `town` | town, whole-town owner, then per entry owner, burden, landlord, months left |
| `money` | slot, cash, credit debt, monthly income, property value (8 lines) |
| `drago` | holder (−1 none), form, countdown |
| `events` | turns, countdown, hold of the event trigger |
| `places` | positions in the Nessie, earthquake, volcano and rockfall lists |
| `obstacle` | slot, type, duration, cell x, y, field, direction, investor, amount (used slots only) |
| `state` | random events on, pending offer, event player, Verdoppeln, property mode, east open, yellow-field discard pending, loaded season, western property value |
| `facing` | last driving direction of the eight slots |
| `deck` | draw pointer, return pointer, then the slot at every deck position (255 = drawn) |
| `hand` | slot, then eight deck slots (255 = empty) (8 lines) |
| `calendar` | month counter, year, inflation |
| `counter` | allowance of the current move, then the directions already driven |
| `risiko` | level (0 = not running), light, tick counter, steps (format 2) |
| `demo` | 1 when a game without humans was asked to end (DS:16, format 3) |
| `stages` | total number of stages DS:A106 (format 4; older saves use the stages left) |
| `turn` | phase (0 spin, 1 drive, 2 arrived, 3 game over, 4 Risiko), current slot, stage origin, destination, stages left |
| `end` | — |

Not stored: messages, the border barrier links (derived from the map) and other
data recomputed from the original files.

## Loading

`SaveCodec::decode` parses everything into temporary values and checks ranges,
counts, duplicate records, the deck (every slot at most once) and the data
fingerprint before anything is applied; a rejected save leaves the running game
unchanged and reports the reason. `writeSaveFile` writes a temporary file next to
the target and renames it, so an interrupted write never replaces a good save.

## Compatibility

The format version changes when the layout changes; the rule revision records
which reconstruction round produced the state. Older rule revisions are accepted
as long as the layout matches; a save from a newer revision is refused.

Rule revisions with a changed sequence of random draws: 25 (East opening text),
26 (texts of the blocked card window and of the investment offer), 27 (draws at
the stage end), 28 (the year balance's draws), 29 (one more draw per player
in the year balance). A save of an
older revision loads, but continues with the current rules.

## Save slots

The game window's save dialog (round 33) writes slot `n` (0–8) to
`slot<n>.sav` in `$XDG_DATA_HOME/dtvr-recomb/saves` (default
`~/.local/share/dtvr-recomb/saves`), and its description to `saves.ini` in the
same directory (`[Save]`, one `Slot<n>=<description>` line per slot). The
autosave of each turn start (round 35) is `as.sav` in the same directory.
