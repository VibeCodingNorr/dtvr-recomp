# Native save format

DTVR Recomp stores games in its own versioned text format (`src/save.cpp`),
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
| `DTVR-RECOMP-SAVE` | format version (currently 8; 7 is read without `edition`, 6 also without `flow`, 5 also without `worth`, 4 also without `extensions`, 3 also without `stages`, 2 also without `demo`, 1 also without `risiko`); always the first line |
| `edition` | edition id of the original data (`de-retail`, `en-us-retail`; `src/edition.hpp`), written as the second line (format 8; older saves are German) |
| `rules` | rule revision of the reconstruction (`kRulesRevision`); newer ones are refused |
| `data` | 64-bit FNV-1a fingerprint of the board and town records (hex) |
| `rng` | state of the MS C random generator; loading a slot or the autosave keeps the running generator (rally.exe saves no seed, round 130), exact copies (new game, a roll taken back) restore it |
| `player` | slot, status (0 inactive, 1 human, 2 computer), flags, field (8 lines) |
| `town` | town, whole-town owner, then per entry owner, burden, landlord, months left |
| `money` | slot, cash, credit debt, monthly income, property value (8 lines) |
| `worth` | slot, the year's rent (player +0x66), count, then that many worth entries of years 0, 1, … (+0x6A; trailing zeros left out) (8 lines, format 6; older saves: 0 and none) |
| `flow` | slot, the month's cash flow (player +0x4A, double), the year's flow (+0x62, 32-bit) (8 lines, format 7; older saves: 0) |
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
| `extensions` | 1 when computers also play the cards rally.exe never picks (recomp extension, format 5; older saves: 0) |
| `turn` | phase (0 spin, 1 drive, 2 arrived, 3 game over, 4 Risiko), current slot, stage origin, destination, stages left |
| `end` | — |

Not stored: messages, the border barrier links (derived from the map) and other
data recomputed from the original files.

## Loading

`SaveCodec::decode` parses everything into temporary values and checks ranges,
counts, duplicate records, the deck (every slot at most once) and the data
fingerprint before anything is applied; a rejected save leaves the running game
unchanged and reports the reason. Loading a slot or the autosave then runs
`Game::resumeLoaded` (rules revision 49), which repeats what rally.exe's loader
recomputes (docs/formats.md, "Loading a saved game"): the start facing of every
slot in play (seg2:5067), Dr. Drago placed anew at his holder with a fresh
countdown random(3) + 4 (seg2:9C97), the western property value when the East is
open, the month's season, and for a save in the spin phase the turn start
seg2:0407 of the current player. The stored facings, countdown and season are
therefore overwritten on such a load; exact copies (new game, a roll taken back)
keep them. `writeSaveFile` writes a temporary file next to
the target and renames it, so an interrupted write never replaces a good save.

## Editions

A save belongs to the edition of the data it was written with (format 8, round E2):
the town names, texts and `europe.map` differ between the German and the US English
retail edition, although the rules are the same. Loading compares the `edition`
record with the running edition before the data fingerprint and refuses another
edition's save with a German message ("Dieser Spielstand gehört zu einer anderen
Ausgabe …"); the running game stays unchanged. Saves of formats 1–7 have no
`edition` record and load only with German data. A new game takes over the edition
of the running one (`PlayView::newGame`).

Since round E3 (user decision 2026-11-26) each edition keeps its saves in its own
directory (below), so no edition overwrites the other one's slots or autosave.
Foreign saves can still turn up there (copied by hand, or left over from the common
directory of rounds E2 and before). `SaveCodec::editionOf` reads a save's edition
from its beginning without decoding it (formats 1–7 count as German), and the game
window treats another edition's save as follows (recomp additions, the original
layout is kept):

- "Spiel fortsetzen" / "Continue Game" in the title menu MAIN is grayed as when no
  autosave exists. rally.exe grays the item at seg8:0DE7: it reads the profile entry
  `Save` / `AS` (DS:1864 / DS:1861) through seg5:22C0 and, for 0, calls seg6:156D,
  which sends message 0x7E9 for the menu and calls `EnableMenuItem(menu, 1,
  MF_BYPOSITION | MF_GRAYED = 0x401)` at seg6:159F (seg6:15BC enables it otherwise).
  The rebuild greys the same item (menu flag 0x01, colour 0x43) when `as.sav` is
  absent or belongs to another edition (`SaveSlots::hasOwnAuto`).
- In the save dialog a foreign slot's description gets the other edition's mark
  (`[US] ` or `[DE] `, `EditionProfile::tag`) and is drawn in the grayed colour 0x43
  unless it is the chosen slot (white as every chosen slot).
- Loading such a slot (or continuing such an autosave) is refused by the codec as
  before and opens the message window with a recomp text in the running edition's
  language (`foreignSaveMessage`: German for the German edition, English for the US
  edition), besides the German line on stderr. Saving into a slot that holds a
  foreign save overwrites it, as any save overwrites its slot.

## Map state between games

User decision (2026-11-26, both editions): every new and every loaded game starts
from a freshly read `map/europe.map`. The US retail rally.exe does this (R seg5:182F
from seg5:186A(…, 1) and in the loader R seg8:AF67, before the East re-opening);
the German rally.exe reads the file once per program run (D seg5:1559), so tile
changes of a previous game (opened East barriers, obstacle tiles) would carry over
into the next one. The rebuild follows the retail behaviour for the German profile
too, as Blue Byte's own later fix and a documented deviation from the German
executable. The simulation never writes into its map (barrier links follow
`eastOpen`, obstacles their table, both part of the save), and a new game is handed
over by the save codec, so no map state carries over there; the game window reads
`europe.map` again for its world bitmap and the ambient animations at every new or
loaded game (`GameScreen::reloadMap`). The rules revision is unchanged.

## Compatibility

The format version changes when the layout changes; the rule revision records
which reconstruction round produced the state. Older rule revisions are accepted
as long as the layout matches; a save from a newer revision is refused.

Rule revisions with a changed sequence of random draws: 25 (East opening text),
26 (texts of the blocked card window and of the investment offer), 27 (draws at
the stage end), 28 (the year balance's draws), 29 (one more draw per player
in the year balance), 30 (order and exclusion of the next destination), 49 (the
loader's draw for Dr. Drago, seg8:B06A, and the turn start after loading). A save of an
older revision loads, but continues with the current rules.

## Save slots

The game window's save dialog (round 33) writes slot `n` (0–8) to
`slot<n>.sav` in the edition's save directory, and its description to `saves.ini` in
the same directory (`[Save]`, one `Slot<n>=<description>` line per slot). The
autosave of each turn start (round 35) is `as.sav` in the same directory.

Save directories (round E3, `saveDirectory` in src/save_slots.cpp), below
`$XDG_DATA_HOME/dtvr-recomp` (default `~/.local/share/dtvr-recomp`):

| Edition | Directory |
| --- | --- |
| `de-retail` | `saves` (unchanged: the common directory of rounds E2 and before) |
| `en-us-retail` | `saves-en-us-retail` |

Migration: the German edition keeps the former common directory, so existing German
saves need no action. When the US edition starts and `saves-en-us-retail` does not
exist yet, `adoptEditionSaves` copies the US slots (with their descriptions) and a US
autosave from `saves` into it, once; nothing is moved or deleted. The originals stay
in `saves`, where the German edition shows them as foreign slots (see Editions); the
user may delete them by hand. Once the directory exists it is never filled again.
Without US saves in `saves` no directory is created before the first US save.
Saves are user data: the installer neither hashes nor manages them.
