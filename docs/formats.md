# Original data: observed structure

These observations describe the supported German CD version identified by its
executable hash below. They do not describe every release.
Use `tools/inspect_game.py` to identify a different installation.

## Confirmed

- `program/rally.exe`: MZ/NE Windows executable, 438,272 bytes. Ten code segments
  total 291,828 bytes; one data segment is 46,662 bytes.
- Imports: WIN87EM, KERNEL, GDI, USER, KEYBOARD, TOOLHELP, WING, MMSYSTEM,
  WAVEMIX and SBTIMER. There is no source code in the inspected game directory.
- `program`: 962 files, 51,526,448 bytes, including 497 BMP, 193 WAV,
  94 SBS and 72 SBB files.
- `butt/1/map.bmp`: uncompressed indexed 8-bit BMP, 688 by 688 pixels. It
  contains the Europe overview with paths already drawn into the picture.
- `map/{0,1,2,3}/part0000.sbb` through `part0017.sbb`: BMP signature;
  the examined `map/0/part0000.sbb` is 16 by 3,072 pixels, 8-bit indexed.
  Dividing it into 16 by 16 blocks yields 192 graphics blocks. The viewer
  checks these dimensions for each loaded file.
- SBS sprite containers (94 files: 34 in `sbs/`, 59 in `butt/sbs/`, 1 in `butt/d/`).
  All 94 files and 1,559 frames parse without remainder under this layout
  (checked by `src/sbs.cpp` and a standalone script; little-endian):

  | Offset | Size | Field |
  | --- | --- | --- |
  | 0x00 | 4 | `SBSF` |
  | 0x04 | 2 | 0x0100 in all files (meaning unknown) |
  | 0x06 | 2 | entry count |
  | 0x08 | 10 | always `0, 1, 8, 0, 0` as uint16 (meaning unknown) |
  | 0x12 | 2 | directory offset, 0x14 in all files |

  Directory entry, 14 bytes: `char name[10]` (NUL-padded, not always terminated
  at 10 characters), `uint32 frame offset`. The first frame directly follows
  the directory. Frame: `uint16 width`, `uint16 height`, 4 bytes that are zero
  in every file, then `height` rows of `(width + 3) & ~3` bytes, stored
  bottom-up like BMP. Every frame ends exactly where the next frame (or the
  file) begins; no offset is shared by two entries. Bottom-up order is confirmed
  visually (volcano eruption in `sbs/vulkan.sbs`, card captions in
  `butt/d/bcards.sbs`).

## Executable analysis (rally.exe, SHA-256 aa3dde36…6b83)

Addresses are `segN:offset` in NE segment numbering; DS offsets refer to the
automatic data segment 11. Reproduce with:

```sh
python3 tools/ne_disasm.py PROGRAM/rally.exe /path/to/private-analysis --use32 9 \
    --dll-dir PROGRAM --dll-dir CDROOT/wing --dll-dir /path/to/optional-system-dlls
```

Import ordinals are named from the export tables of the game's DLLs and Wine's
16-bit builtins. Relocation source type 2 patches only the selector; the
offset in the instruction is the real target.

### Confirmed from code

- **Data loader** seg5:14C0: builds paths with `wsprintf` from DS strings
  (`%s\europe.map` DS:0254, `.pit` 0262, `.car` 0270, `europe_%c.tow` 027E,
  `.str` 028F, `%d_assort.sbs` 029D, `slot0.sbs` 02AE, `europe.inf` 02BB) and
  loads each whole file via seg6:027F. `europe.map` → far pointer DS:A058.
  `europe.inf` words 0 and 1 (700 and 115 in this copy) go to DS:9CFA and DS:9C80
  and size later allocations (700 × 2 bytes; 115 × 0x6F bytes). Meaning open.
- **europe.map** (262,144 bytes): `getMapCell(x, y, layer)` at seg6:26B6 returns
  0 when x or y > 255, otherwise the uint16 at `((layer·256 + y)·256 + x)·2`.
  So the file is 2 layers × 256 rows × 256 columns. `setMapCell` seg6:273B keeps
  the top 4 bits and replaces the low 12.
- **Cell value**: bits 0–11 index the tile pointer table (max 3275 in layer 0,
  3203 in layer 1; 3456 tiles loaded). Bit 14 (0x4000): tile is mirrored
  horizontally — seg6:2D36 copies every 16-byte row reversed into DS:9DC4.
  Bit 15 selects the draw pass (below). Bit 12 (0x1000, 176 cells in layer 1):
  meaning unknown, not used by the draw routine.
- **Tile draw** seg6:2C0A(mapX, mapY, destX, destY, flags): layer = flags & 1;
  skip tile 0x1F (47,769 cells of layer 1 = empty); skip tile 0x33 when
  DS:B5D8 = 0, which is `[Setup] Views` read with default 1 (seg5:2004);
  skip if bit 15 of cell ≠ bit 15 of flags; skip null tile pointers; blit 16 × 16
  with colour key 0.
- **Draw passes** per visible cell (seg6:28D5, seg6:2AC1): flags 0x0000 (layer 0),
  0x0001 (layer 1), 0x8001 (layer 1, bit 15 set). seg6:28A3 runs only the 0x8001
  pass. Layer-0 cells with bit 15 set (11 cells) are therefore never drawn by
  these paths.
- **Season tiles** seg5:186A(month, reload): directory = table[month % 12] with
  table `3,3,0,0,0,1,1,1,2,2,2,3` (stack bytes seg5:187E–18A7). If DS:B5DC = 0
  the directory is fixed to 1. Loads `%s\%d\part%.4d.sbb` (DS:02DA) from 0
  upward until loading fails (18 files here), then builds the pointer table at
  DS:9AD2 with 192 entries (0x300 bytes) per file: first entry =
  `bits + 0xBF00`, each next one 0x100 lower. For the bottom-up 16 × 3072 DIB
  this makes tile k = image rows 16k…16k+15 counted from the top; global index
  = file × 192 + k. The four directories differ from `map/0` in 118 (`map/1`),
  159 (`map/2`) and 760 (`map/3`) tiles.
- **Transparent blit**: seg5:4FC6 clips and computes DIB addresses (stride
  from width × bpp rounded to 32 bits, bottom-up), then calls seg9:0024. Segment
  9 switches its own descriptor to 32-bit code via DPMI `int 31h` AX=0Bh/0Ch
  (sets bit 0x40 of the access byte); disassemble it as i386. seg9:0024 copies
  rows and skips every source byte equal to the key byte; seg9:00F1 is an opaque
  `rep movs` copy. Six of seven blitter call sites pass key 0 (tiles at
  seg6:2CC8, SBS frames at seg6:1E34, others at seg6:09E0, seg6:224F,
  seg2:1AF5, seg5:5ABB); seg1:1BE3 passes 0xFF for a DIB source.
- **SBS handling**: seg6:3E38 loads a whole SBS file into a free slot of a
  30 × 18-byte table at DS:9224 (far pointer + 14-byte name). seg6:406D looks a
  frame up by name across all loaded files: count at file +0x06, directory at
  file +0x12, 14-byte entries, name compare, then returns file + uint32 at entry
  +10. No `SBSF` signature check exists. The frame drawer at seg6:1E06 takes
  width/height from frame +0/+2 and pixels from frame +8, and blits with key 0.
  **Index 0 is transparent for SBS frames.**

### Board: fields, towns (round 3)

- **europe.inf** (4 bytes): uint16 field count (700) → DS:9CFA, uint16 town
  count (115) → DS:9C80 (seg5:1714–1724). Both are loop bounds, e.g. seg6:2327
  (fields) and seg6:2411 (towns).
- **europe.str** → DS:52. Field table of `count` × 16 bytes (index × 16,
  `shl si,4`). The original debug dump at seg6:2331 prints the members with the
  format string DS:16DE — `Number`, `oben`, `rechts`, `unten`, `links`,
  `whatisit`, `offset`, `flagset` — which gives the layout:

  | Offset | Size | Field |
  | --- | --- | --- |
  | 0 | 1 | x (map cell); compared with DS:955E at seg6:233B |
  | 1 | 1 | y (map cell); compared with DS:9560 at seg6:2352 |
  | 2, 4, 6, 8 | 2 each | neighbour field index up, right, down, left; −1 = none |
  | 0x0A | 2 | `whatisit` (kind); 6 → town lookup (seg6:23EA), 5 → "Cardfield found" (seg6:2487) |
  | 0x0C | 2 | `offset`: file offset of a field list, 0 if none |
  | 0x0E | 2 | `flagset` |

  Several callers convert x/y to pixels with `shl ax,4` (e.g. seg2:00F6), i.e.
  map cells of 16 pixels.
- Checked on this copy: all 1,602 links are symmetric and axis-aligned
  (up = y−1, right = x+1, down = y+1, left = x−1); the graph of 700 fields is
  connected; no two fields share a cell. Degrees: 477 × 2, 187 × 3, 19 × 1, 17 × 4.
- **Field lists** follow the table (byte 11,200 to EOF): `char name[14]` (OEM),
  `uint16 n`, `uint16 values[n]`. 119 lists end exactly at EOF; 131 field
  offsets hit list starts, 4 (kind 5) equal the file size (empty). Only kinds 4,
  5 and 6 have offsets. Names include places off the map (`New York`,
  `Reykjavik`); the meaning of the values (0–2700) is open.
- **Towns** `europe_%c.tow` (DS:027E, letter from DS:A0F0) → DS:4E. Records of
  0x176 bytes (seg6:241B): uint16 x (+0), uint16 y (+2), uint16 at +4 (0 in all
  records), name at +6 (looked up with `lstrcmp` at seg5:5E51), then ten
  0x22-byte entries from +0x22: name (24 bytes), uint32 at +0x18 (heavily used,
  e.g. 100,000 for London's first entry), three uint16 at +0x1C/+0x1E/+0x20.
  The file holds 120 records; the game uses the first 115 (DS:9C80). Text is in
  OEM code page 850 (`0x94` = ö; the game imports `KEYBOARD.OEMTOANSI`).
- Every one of the 115 towns lies on a field: 46 of kind 6, 69 of kind 4. Every
  kind-6 field carries a town.

### Board: data correlations (not yet confirmed in code)

- Each field lies on a layer-1 map cell whose tile is `15 + kind` (tiles 16–21:
  yellow = kind 3, blue = 1, red = 2, striped = 6, white = 4, violet = 5); this
  holds for all 700 fields. These are the coloured markers on the roads.
- `flagset` bit 14 (0x4000) is set on exactly the 176 fields whose layer-1 map
  cell has bit 12 (0x1000). Other `flagset` bits (0–4) and the game meaning of
  the marker colours are open.
- `europe.ind`, `.edt`, `.eit`, `.oit`, `.c88`, `.cvc` and `.001` are not named
  anywhere in `rally.exe` (no format string or literal); they may be editor
  files. `europe.car` (→ DS:4A) is loaded but not yet analysed.

### Movement (round 4)

Confirmed from code unless marked otherwise.

- **europe.pit** (16,384 bytes → DS:46) is a tile property table: seg2:3260(x, y)
  returns `pit[0x1000 + tile(layer 0)] | pit[0x1000 + tile(layer 1)]` (uint16).
  Entries 0–0xFFF are zero in this copy. Bit usage in the code: 0x01 and 0x10
  at the vehicle's cell (seg2:1DE2, seg2:1E00), 0x02 in the step direction
  (seg2:1E53), 0x02|0x04 together switch a vehicle mode (seg2:1EB5 → seg2:2AF6
  argument 0 or 1). Data correlation: 0x01 is set on exactly the 700 field
  cells, 0x02 on roads (6,014 cells), 0x04 on sea routes (always together with
  0x02), 0x08 on water including rivers and lakes (29,939), 0x10 on the 40
  cells with tile 0x33 (info points shown with `[Setup] Views`), 0x20 on 15
  road cells (meaning open). Every graph link starts on a road cell and every
  cell between linked fields has 0x02.
- **Players**: records of 0x522 bytes at DS:56, current player index DS:94E2.
  +0x16/+0x18 = pixel position (cell = value >> 4), +0x3C/+0x3E = field index
  set from the cell via seg2:4C7D (linear search over the field table, −1 if
  none).
- **Driving** (seg2:1CB0): per tick, cell flags are probed at the vehicle and at
  the four neighbours (offset tables in that order: centre, up, right, down,
  left). Directions: 1 up, 2 right, 3 down, 4 left; velocity tables DS:19EA (dx
  = 0, 0, 2, 0, −2) and DS:19F4 (dy = 0, −2, 0, 2, 0), shifted left by DS:34
  (initial 1). Direction decisions are only taken on cells with flag 0x01 (or
  0x10 with Views on); the requested direction DS:A77A needs flag 0x02 in the
  neighbouring cell.
- **Step counter** seg2:3620(dir, commit): DS:A778 = allowance (0 → no move,
  −1 → unlimited), DS:9D40 = steps taken, direction history at DS:A790. The
  first step is allowed; a step opposite to the last recorded one (table DS:19E4
  = 0, 3, 4, 1, 2) is allowed and, when committed, removes it; otherwise the
  step is refused once steps = allowance. Steps are therefore counted in
  fields, and turning back undoes steps.
- **Allowance from the slot machine**: seg2:6426(reels, maximum, flag) sets
  DS:948A = reels and DS:A6D6 = maximum; seg2:776A sets each reel to
  `random(maximum) + 1`; seg2:7EC2 sums the reels into DS:A778 via seg2:7F14,
  which also resets the step count. The message WM_USER+6 (0x406, seg4:0907)
  uses 1 reel, maximum 5, i.e. 1–6. Command IDs 0x2710–0x2714 use 1–5 reels with
  maximum 5 and 0x2715 one reel with maximum 1 (1–2); matching them to the cards
  "2–5 Walzen" and "1 oder 2" is an interpretation.
- **Random numbers**: seg10:1DDE is the Microsoft C `rand()`
  (`seed = seed · 0x343FD + 0x269EC3`, result `(seed >> 16) & 0x7FFF`), seg10:1DC6
  `srand()` (16-bit seed, high word cleared), seeded once at seg1:02A9 with the
  result of seg10:19AA(0) (probably `time`). seg6:1343 is `random(n)` =
  `n ≤ 0 ? 0 : rand() % (n + 1)`.
  *Round 130:* seg1:02A9 lies in seg1:028A, called once by the program's
  initialisation seg5:0000 (seg5:0029, after the cursor); nothing else calls
  seg10:1DC6, and the seed DS:838E / 8390 is only used by seg10:1DC6 / 1DDE. The save
  routine seg8:A870 writes the players (DS:56), heap tables (DS:5A, 5E, 9572) and
  the blocks DS:9CFC, A0FE, A790 — not the seed. A loaded game therefore continues
  the session's generator: loading the same game twice gives different rolls.
  Rebuilt: the program seeds with the time when `--seed` is not given, and
  `SaveSlots` loads with `SaveCodec::Random::Session` (the saved `rng` record stays
  for exact copies). *Corrects* the former fixed seed 1 and the restored generator,
  which made every load of a save roll the same.
  *Round 143:* the program's initialisation seeds at seg5:0029 and only later
  starts the intro (seg5:0174 reads ShowIntro, seg5:0188 calls seg7:0000), so the
  intro's wipes draw from the time-seeded generator; a new game (PLAYER / TEMP)
  and the demo (seg4:0DA9) do not seed again but draw on. Rebuilt: one session
  generator, the running Game's (`Game::sessionRandom`); the intro draws from it
  and `PlayView::newGame` builds the new game from its state. *Corrects* the intro's
  seed 1 and the new time seed of every new game.
- **Field distances** seg2:43A8(unused, start): breadth-first search over the
  field links up to 80 levels into a word array at DS:22; a field with
  `flagset & DS:A10F` ≠ 0 is not expanded (seg4:0E0B sets the mask to 0x4000).
  **Route** seg2:9996(from, to) clears the mask, computes distances from `to`
  and from `from` repeatedly takes the first neighbour (up, right, down, left)
  with a smaller distance, recording each field via seg2:4D3F into a 1,000-entry
  ring buffer at DS:2C. seg1:A4E4 draws such a route with GDI `MoveTo`/`LineTo`.
- **DS:22 as state (round R3b).** seg2:43A8(from, to) returns 0 without touching
  the array when either argument is 0xFFFF (seg2:43BC–43CF); otherwise it fills
  DS:9CFA words with 0xFFFF, sets `to` to 0 and searches from `to` only (`from`
  is not used further) level 1..0x50 (seg2:4449 `ja`): for each frontier field
  (DS:9AE0 list, count DS:B62E) whose record word +0xE & DS:A10F is 0
  (seg2:44A4–44C2), each link +2 + 2k ≠ 0xFFFF whose word is still 0xFFFF gets
  the level and joins the next list (DS:ABFC, count DS:A124); an empty next list
  ends the search (seg2:44FD). It is the only writer of the array: every other
  `les si, DS:22` (seg1:A506 / A569, seg2:14FB / 151A / 4846 / 4932 / 99E3 / 9A5A,
  seg5:6ADC / 7845–7C08 / 7FAA / 9BBF, seg8:5FB5 / 5FCE / 602A) is followed by a
  read or compare; seg5:25D1 only frees it. Callers and their `to`: seg2:0076
  (new game, A10D = A10B), seg2:154A (stage change, after Dr. Drago), seg4:0EA4
  (new game after seg7:372F(2)), seg8:AF82 (load), seg8:612A (event code) and seg8:6C66 (the
  East opening, after DS:A10F = 0 at 6C46) with (A10B, A10D); seg1:A4E4 (player
  +0x3E, A10D), seg1:A5BD (A10B, A10D), seg8:5FA8 (player +0x3C, A10D); and the
  route seg2:9996(from, to) with DS:A10F = 0 (seg2:99B0), which restores the mask
  and recomputes (A10B, A10D) on both exits (seg2:9B6D–9B7B, 9B89–9B97). DS:A10F
  changes only at seg4:0E0B, seg8:6C46 and inside seg2:9996. So during play the
  array holds the distances from DS:A10D with the current mask; the exception is
  the stage change, where seg7:372F writes the new A10D (seg7:3D7D / 3F46 / 45E7 /
  4867 / 4C1F) before the placement at seg2:1495 and the recompute at seg2:154A.
  Between seg2:1486 and 154A run seg1:2A30, seg7:372F, seg2:0368, 1A41 and 9C97
  (9C97 calls seg2:9BAE, 4CEB, B477, 9CF9 and reaches no seg2:43A8 caller).
  **Timing-dependent exception:** seg7:372F's loops call the map tick seg2:1C37
  (seg7:44A0 in modes 1 / 2, after A10D was stored at 3D7D / 3F46; seg7:4B27 in
  mode 3; both only when DS:94C4 and DS:94E6 ≠ 0). 1C37 → seg2:9F61 (Dr. Drago's
  vehicle, nothing when DS:9CFC = 0xFF; while off-screen, seg2:9ECB = 0, it
  repeats seg2:A0A5) → A0A5 calls seg2:98D0 when the vehicle's route ring is
  empty (DS:2A = DS:28, seg2:A11B; similar at A176 / A271 / A2C1 / A64E / A7D7 /
  A959 / A9EB). 98D0 computes a route seg2:9996(DS:94D8, holder +0x3C) at seg2:990B
  only for a pending request (DS:9566 = 0 and DS:94EC ≠ 0xFF), which only seg2:9804
  sets (98BF / 98C2): Dr. Drago's form change at a turn start (seg2:B400 / B433 /
  B44C), after the leaving route seg2:98A2. 9996 then rebuilds DS:22 from the
  current A10B / A10D (9B7B / 9B97). So when the arriving player's turn began with
  a form change and the vehicle's leaving route had not ended before seg7:372F
  stored the new A10D, the placement at seg2:1495 sees the distances from the
  **new** destination with the mask. Whether that happens depends on the ticks
  (vehicle speed, view, the time the player took), as with the ambient and
  destination-light draws; not modelled — the rebuild always uses the reached
  destination. The route itself draws no random number.
  **Assumption:** the overview (seg1:A240, from the RALLY command handler seg4:08DC /
  099F) is not opened while the ZIEL window (seg7:372F holds the focus, seg7:37C1)
  has stored the new A10D; if it were, seg1:A4E4 / A5BD would rebuild the array
  from the new destination before seg2:1495. The early exit of seg2:9996's restore
  (A10B or A10D = 0xFFFF) does not occur with fields. Rebuilt as
  `distanceTable` (src/rules.cpp) and `Game::distanceTable`, used by the computer
  (seg5:7386 / 7E4F / 9B0B), the PLINFO distance (seg5:6AB2) and, with the reached
  destination, the stage placement.

### Horn while driving (roadmap R2)

Confirmed from code (rally.exe SHA-256 aa3dde36…6b83, `tools/segdis.py 2 1CB0 2410`,
`2 2676 271C`, `2 03C0 04AD`, `6 1343 137F`).

- **Horn check** seg2:2676(mover): returns at once while DS:9C74 ≠ 0. Otherwise
  slots 0–7 in order, the mover skipped and slots with status +0 = 0 skipped (no
  test of human or computer; Dr. Drago has no slot): the first one whose field
  +0x3C equals DS:9592 (the field at the vehicle's cell, −1 off fields) draws
  `random(1)` (seg6:1343 → rand() % 2) and on 0 plays sound 10 + mover / 2
  (Hupe01, Hupe02, Hupe03, Hupe05) on channel 2 (seg1:2C9B). Only that first
  driver is checked; a second one on the field draws nothing.
- **Call sites** in the drive seg2:1CB0, which runs in every idle pass seg2:1C37
  and acts when the vehicle is aligned to a cell (seg2:2C0F) on a field cell
  (bit 0x01) or a view point (bit 0x10 with Views): seg2:1F81 when a direction is
  accepted while the vehicle was driving (DS:9AD0 ≠ 0, or at a view point), after
  Plopp01 (seg2:1F75); seg2:209F when it stops on a field (DS:9AD0 ≠ 0, DS:9592 ≠ −1),
  after Plopp01 (seg2:208D) — unless steps taken DS:9D40 = allowance DS:A778 and
  DS:9C74 = 0: then Carb01 (seg2:207C) and no check. The departure from a stop
  (DS:9AD0 = 0, seg2:1F25 – 1F6C) has no check. So every field the vehicle reaches
  runs exactly one check, except the last field of the move when it stops there.
- **DS:9C74**: set to 1 at the turn start (seg2:048F, with DS:9AD0 = 0xFF) and
  cleared at seg2:215E by the first pass that rests on the start field (stop path
  seg2:2021, also taken when the direction is refused because DS:A778 = 0 during
  the slot window, seg2:6446 / 661C). The first counted step therefore always
  finds it 0. *Not modelled:* a direction held through the player change that is
  accepted in that very first pass.
- **Obstacles**: the check of seg2:2404 runs on the road after the step was
  counted (seg2:1EA3 at the departure, seg2:21D0 ff. in front of a 0x20 cell); a
  refusal stops the vehicle there (seg2:2246) and only the reversing direction
  drives on (seg2:2189 → 22E1 undoes the step), so the vehicle reaches its field
  again and the stop / pass there runs the horn check. A step that seg2:3620(d, 0)
  refuses never leaves the field (seg2:1E61 → 2015) and meets no barrier, obstacle
  or card question.
- **Rebuilt** in `Game::step` / `Game::honk` (`src/game.cpp`): each step is one
  arrival; Plopp01 and the check unless the step reaches the last field (Carb01).
  A step over a closed barrier or an obstacle first plays Carb01 (seg2:21E5, round
  R3a; once, also when a human is asked for a clearing card).
  The counter is probed before barriers and obstacles; a barrier or obstacle
  without a clearing card, or a declined card question (`answerObstacle(false)`),
  checks the player's own field at once (the return; no rule draws while the
  vehicle stands on the road — the timing-bound ambient draws are kept apart,
  `src/ambient.hpp`). Computers (`computerAdvance`) drive through the same
  step. Tested in `tests/honk_test.cpp`. *Known difference:* a human who reverses
  exactly when reaching the last field passes it in rally.exe (Plopp01 and a check)
  where the field-by-field model stops (Carb01); the horn of an obstacle return
  sounds at the refusal instead of on arrival. View points (DS:9592 = −1) never
  match because every rebuilt driver stands on a field.

### Turn and field actions (round 5)

- **Direction input** (main window procedure, seg4:0001): key-down of VK_UP,
  VK_RIGHT, VK_DOWN, VK_LEFT sets DS:A77A to 1, 2, 3, 4 (seg4:01F2–020A); a
  second message releases them to 0 (jump table seg4:00FC). The vehicle
  therefore drives while an arrow key is held. Other keys: Space centres the
  view on the player (seg2:3216 → seg6:3064, then redraw seg2:05A8), Enter →
  seg2:0BC3, Backspace/Escape → a dialog via seg5:5214, `1`–`4` → seg6:0DA0,
  Insert/Delete → seg6:2E54(3).
- **Cells with pit bit 0x20**: entering one calls seg2:2404(x, y); a zero result
  stops the vehicle (seg2:21D0). Meaning: border barriers and obstacle tiles
  (seg2:3305 writes 0x20 as the obstacle tile's property). Only a road cell runs
  the test (a field cell takes the field branch at seg2:1E00), and before it
  Carb01 plays on channel 1 (seg2:21E5–21EB) whether or not the vehicle passes.
  All 15 barrier cells lie at least two cells from both fields of their link
  (data check, R3a), so the test always runs.
- **Arrival** (seg2:0D38): the field action runs when the player is on a field
  (DS:9592 ≠ −1), steps taken = allowance or allowance = −1, the vehicle is
  aligned to the cell grid (seg2:2C0F: `(x | y) & 0xF == 0`) and DS:9EDE = 0. A
  move therefore only triggers a field when the full allowance is used.
  *Checked in round 136:* the destination DS:A10D is read in seg2 only by the town
  case of the field action (seg2:11A6 / 11BA / 11CD), the finish (1304 / 130B), 1542
  and the route (9B73 / 9B8F) — never by the drive. Passing over the destination
  with steps left triggers nothing; only a move that ends on it reaches the goal.
- **Return accepts the field** (round 137): seg2:0BC3, which contains this test,
  has one caller, the main window's Return (seg4:01EA; the steering cross M1
  posts Return too). A human's vehicle therefore stops on its last field (Carb01)
  and the field's action — money, card, town, finish — runs only when Return is
  pressed; until then an arrow can still turn back. The rebuild did this at once;
  now `Game::setArrivalConfirm` (game window) leaves it to `confirmArrival`.
  Computers and the terminal mode arrive at once (an unchanged two-computer log).
  Return while the vehicle is still between cells does nothing (seg2:2C0F).
- **Turn end after the field** (round 113): every field handler returns to
  seg2:1618, which calls the forced sale check seg1:4E39 and nextPlayer seg2:06FC
  directly. The handler's message, card dialog, town dialog or card shop runs
  modally, so a human's turn ends when it is closed, without another Enter. The
  game window now calls `Game::endTurn` itself once nothing modal is left (the
  terminal mode `--play` keeps Enter).
- **Dispatch** on `whatisit − 1`, jump table seg2:160C:

  | Kind | Marker | Handler | Action |
  | --- | --- | --- | --- |
  | 1 | blue | seg2:0D85 | money gain, see below; text 4001 + random(14) |
  | 2 | red | seg2:0F15 | money loss, see below; text 4101 + random(13) |
  | 3 | yellow | seg2:10A5 | next event card: seg3:032D cycles through the deck array DS:9572 (index DS:94FA, size DS:9C8C); picture `%s\%d\%c\%dcard_%.2d.bmp` (DS:1A2E) |
  | 4, 6 | white, striped | seg2:11A6 | town: if the field is the destination DS:A10D, reward 300,000 (DS:85EE) × inflation, or 100,000 (DS:85F6) when DS:A104 ≠ 0, then WM_COMMAND 0x424; otherwise the town dialog seg1:4BDD |
  | 5 | violet | seg2:15AE | seg3:4437(0) card shop (round 15) |

- **Money fields**: monthly ranges (month = DS:A100 mod 12) built on the stack
  at seg2:0BD7–0C4A. Blue uses factor 0.5 (DS:85DE), red 0.75 (DS:85E6), both
  truncated by `_ftol` (seg10:368A). Amount = minimum + random((max − min) / 100)
  × 100, multiplied by the inflation factor DS:A11B; seg2:59E6 adds it to the
  player's doubles at +0x42 and +0x4A, seg2:5A47 subtracts it.
  On arrival the field sound plays (seg2:0EF1 / 1081), then the message window
  shows the text with the player and the unsigned amount (seg2:0F05 / 1095 →
  seg6:51EA), before Dr. Drago acts (seg2:0F0D / 109D). Rebuilt in round 93 from
  the audit `docs/original-money-field-audit.md` (test
  `tests/money_field_notice_test.cpp`); passing such a field shows nothing.
  *Round 104:* the arrival handler runs once the vehicle has driven onto the field,
  so the game window keeps a field's messages (like its sounds) until the drawn
  car has arrived.

  | Month | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
  | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
  | blue raw min | 1800 | 3600 | 7200 | 9000 | 12600 | 14400 | 18000 | 16200 | 14400 | 10800 | 7200 | 5400 |
  | blue raw max | 2400 | 4800 | 9600 | 12000 | 16800 | 19200 | 24000 | 21600 | 19200 | 14400 | 9600 | 7200 |
  | red raw min | 19600 | 15200 | 12600 | 10500 | 8800 | 7300 | 3300 | 5300 | 9200 | 12400 | 14300 | 16500 |
  | red raw max | 27000 | 24300 | 18900 | 16200 | 13500 | 10800 | 5400 | 8100 | 13500 | 18900 | 21600 | 24300 |

- **Inflation**: a new game copies 1.0 (DS:8586) to DS:A11B and sets the route
  mask DS:A10F = 0x4000 (seg4:0DFD–0E0B). At seg2:0773, when month mod 12 = 11
  and the player returned by seg2:1A41 is the current one, the year counter
  DS:A102 is incremented and DS:A11B *= 1.01 + 0.01 · random(5) (+1 % to +6 %).

### Players, months and stages (round 6)

- **Player order**: 8 slots (seg2:19DD `(p + 1) mod 8`, seg2:1A0F `(p − 1) mod 8`);
  slot status byte +0x00, 0 = inactive. seg2:0368 counts active slots,
  seg2:1A41 returns the first active slot. seg2:5C5A is true when no slot has
  status 1 (then seg2:5CAD sets DS:16 = 1 and the game ends via WM_COMMAND
  0x424).
- **nextPlayer** seg2:06FC: clear bit 3 of +0x40 for the current player, take the
  next active slot (DS:94E2). If it is the first active slot, a new month
  starts: when DS:A100 mod 12 = 11 the year ends first (seg1 year-end calls,
  DS:A102 + 1, inflation step), then DS:A100 + 1 (seg2:07EB). seg2:089D then
  checks +0x40: bit 1 or bit 2 → this player sits out once (bit cleared, text
  1000 + random(2) or 1010 + random(2)) and the loop repeats; bit 0 → seg2:C00C
  (only when the player equals DS:9CFC; not modelled). The bits are set in seg3
  (seg3:19C4, 1A17, 1AE9, 1B3C, 2B88), probably by cards.
- **Start**: seg2:0000 takes the first kind-6 field whose town name equals DS:1A00
  `LONDON` (`_fstricmp`, seg10:22CA folds A–Z) as origin DS:A10B and destination
  DS:A10D.
- **Destination** seg5:1E2C(origin, current): candidate list DS:1A (60 slots)
  of all kind-6 fields in field order (seg5:1D70; 46 here). It repeats
  `idx = random(count)` and rejects an empty slot (idx = count), the origin, the
  current destination, a field with `flagset & DS:A10F` (0x4000 after a new
  game) and any idx for which an active player's +0x3C equals idx. That last
  test compares a field number with a list index; the rebuild mirrors it.
- **Stages** (see also "Stage end", round 43): DS:A104 = remaining stages, read with key `Rounds` (DS:184D,
  seg8:0A6A; new game default 99 at seg4:0E23), DS:A106 = total. Reaching the
  destination with A104 ≠ 0 pays 100,000 × inflation, decrements A104 (at 0:
  "last stage" message seg7:372F(3)), sets the origin to the reached field and
  draws a new destination; then seg2:9C97 gives **Dr. Drago** to the active
  player with the largest value in the distance array DS:22 (first active on a
  tie or with one player). *Round R3b:* seg2:14B1–1535 starts from 0 with the
  first active (seg2:1A41) and takes a slot only when DS:22[+0x3C] is larger,
  unsigned (`jbe` at seg2:1506): ties keep the earlier slot, a field the search
  did not reach (0xFFFF: beyond 80 levels or behind the mask) wins, and all 0
  gives the first active. DS:22 still holds the distances from the destination
  just reached with the current mask (see "DS:22 as state", also for the
  timing-dependent exception after a form change of Dr. Drago); it is rebuilt from
  the new destination at seg2:154A. No random draw before seg2:9C97. Rebuilt as
  `stageDragoPlayer` (src/rules.cpp) from `Game::placeDragoForStage(reached)`;
  *corrects* the earlier rebuild (distances from the new destination, no mask,
  unreached fields skipped). *Correction (round 10): this was first read as the
  player who starts the stage; seg2:9C97 places Dr. Drago (DS:9CFC).* With A104 = 0 the reward is 300,000 × inflation and WM_COMMAND
  0x424 ends the game (seg4:075B tears down the game windows).
- **Last stage / Master-Rallye (round R3a).** When A104 becomes 0 (seg2:1441) the
  game plays music A1 (DS:1A52) and calls seg7:372F(3). That announcement still
  draws a destination at seg7:3755 (seg5:1E2C with the old A10B and the reached
  A10D), but mode 3 (seg7:46EF ff.) never stores it: once the first voice
  `Master1` is done (or skipped) it searches the list DS:1A in order for the town
  name of seg2:271C equal to `BRÜSSEL`, `brüssel` or `brussels` (DS:4E11 / 4E19 /
  4E21, lstrcmpi, seg7:47AB–4867) and at the end again for `BRÜSSEL` / `brüssel`
  (DS:4E4B / 4E53, code page 850, seg7:4B89–4C22), writing the field to A10D. The
  German towns match `brüssel` (europe_d.tow `Br\x81ssel`, name at file offset
  0x67C0, field 239). Mode 3 has no rule draw of its own (seg1 has no rand call);
  its loop calls seg2:1C37 (seg7:4B15–4B27, when DS:94C4 and DS:94E6 ≠ 0), whose
  ambient animation seg8:83A0 draws seg6:1343 at seg8:8465 — a timing-dependent
  draw like the destination light's, not modelled.
  Then A10B = the reached field (seg2:145B) and **seg2:5CDE**: k = random(12 − n)
  (n = players in play, seg2:5D51–5D66); the players in play, in slot order, get
  the fields at cells k, k + 1, … of the table x = 05 14 37 44 51 70 94 A0 A0 BF D3
  C7, y = 05 E2 F5 FD FD FD FA F5 EA DC D7 C0 ([bp−0xE] / [bp−0x1A]): every field
  whose record +0 / +1 equals the cell (the loop does not stop at a match), +0x16 /
  +0x18 = cell · 16, +0x3C = +0x3E = field, seg2:5067 (start facing) and +0x1E =
  animation word 0 − slot (4 − slot, a per-slot frame phase); a cell without a
  field leaves the player but uses the cell. Original data: one field per cell,
  fields 0, 31, 109, 132, 178, 252, 376, 410, 408, 518, 573, 543 (31 and 376 are
  sea fields, cell bit 8). Then seg2:5FCA: seg2:5FF7 recomputes +0x5A of all eight
  slots, seg1:8EE1 sorts the players in play by +0x42 + +0x5A (adjacent swaps only
  when the later value is greater, so ties keep slot order; one player: seg2:1A41)
  and seg2:9C97 gives Dr. Drago to DS:957A, the first (countdown random(3) + 4,
  seg2:B48D). The distance rule seg2:1495–1535 is skipped (seg2:1475 → 153D).
  Rebuilt in `Game::arrive` / `Game::startMasterRallye` (`src/game.cpp`,
  `src/game_drago.cpp`) with `lastStageDestination`, `masterStartFields` and
  `startFacing` (`src/rules.cpp`); tested in `tests/master_rallye_test.cpp`. The
  +0x1E frame phase is not modelled (the map's frame animation restarts per vehicle).
- The opening capital was not found; money is only changed through seg2:59E6
  (add) and seg2:5A47 (subtract).
- **Rule consequence**: as reversing only undoes steps, a player in a dead end
  (19 fields of degree 1) cannot use a large allowance; no escape path has been
  identified yet.

### Rebuilt hotseat game

`src/game.cpp` assembles these rules into a hotseat game: London start, slot
roll, field-to-field movement with undo, arrival actions for money fields and
destinations, player change with month, year and inflation, stages and game
end. Simplification: movement is field to field instead of pixel driving.
Not yet rebuilt: event cards (yellow), card fields (violet), town dialog and
property economy, sit-out flags set by cards, opening capital, computer
players. A scripted run on the original data (3 players, 2 stages) finished
after 82 turns at month 27 with inflation 1.092.

### Event cards (round 7)

- **Definitions** at DS:2EB2: 41 records of 14 bytes, terminated by 0xFF: byte
  category (1, 2 or 4; meaning open), byte copies, double value (meaning open),
  far pointer to the card handler (data-segment relocations: segment 3, card 26
  in segment 7). 199 copies in total.
- **Names**: the code builds `1card_%.2d` from type + 1 (DS:3282) and looks the
  frame up in `bcards.sbs`; the frame pictures and the help texts 11001 ff. of
  `rally_d.dat` give the names (type 0 Weihnachten … 16 Schlafen … 22–25 2–5
  Walzen, 28 1 oder 2, 29–34 1–6 Felder … 40 Verdoppeln; full table in
  `src/cards.cpp`). Text IDs drift by one from 11039 on, so names are matched
  through the pictures.
- **Deck** (seg3:0000): one 7-byte slot per copy at DS:9EDA (type in byte 0) in
  type order; the order array DS:9572 starts as 0…n−1 (seg3:01AD) and is shuffled
  by seg3:0206 with n · 10 swaps of `rand() % n` pairs (note: `rand`, not
  `random`). seg3:032D(take): with take ≠ 0 it returns the slot at the pointer
  DS:94FA, marks it 0xFF and advances cyclically; with take = 0 it only peeks.
  Nothing writes slots back, so played cards do not return.
- **Hands**: 8 slots at player +0x24 (0xFF empty). A new game deals
  `[Setup] defaultCards` cards to each active player (seg3:0286; read at
  seg2:02C8 with default 1, 0 → 3, more than 8 → 8).
- **Yellow field** (seg2:10A5): peeks the top card; if the slot is taken nothing
  happens, otherwise the card dialog (seg3:0887, text 403 when the hand is full:
  "Du mußt erst eine Karte ablegen …", text 4201 "… bekommst Du natürlich auch
  eine Karte!") hands it to the player; computer players choose via seg5:7CD2 →
  seg3:4161.
- **Movement cards** (handlers verified): 2–5 Walzen post WM_COMMAND
  0x2711–0x2714 (2–5 reels, maximum 5), 1 oder 2 posts 0x2715 (1 reel, maximum 1),
  1–6 Felder call seg2:7F14(n). This confirms the round-4 reading of the command IDs.
- **Texts**: `tools/ne_strings.py` dumps the RT_STRING resources of
  `dat/d/rally_d.dat` (631 strings), e.g. 1000/1001 sit-out texts (engine
  damage, sugar in the tank), 1010 Schlafen, 4001 ff. blue, 4101 ff. red, 4201 ff.
  yellow field.

Rebuilt: deck, shuffle, draw, hands, starting cards, yellow field and playing
movement cards before the spin (src/cards.cpp, src/game.cpp). Assumed, not yet
traced: when a card may be played, and the order of random calls at game
start (shuffle, deal, destination).

Open: the effects of the other 30 cards (money, property, blocking, sit-out
flags), card categories and values, the card field (kind 5) dialog, discarding,
computer players.

### Properties and money (round 8)

- **Starting cash**: seg2:033A copies 30,000 (DS:85BE) into every player's
  cash double at +0x42 during a new game (seg2:0178).
- **Town entries** (europe_<lang>.tow, 0x22 bytes each; town record = 11 such
  blocks, addressed as `(town · 11 + entry) · 0x22`): +0x00 name (non-empty =
  entry exists, seg1:7D2D), +0x18 uint32 value, +0x1C uint16 income factor
  (`fimul`, seg1:8726), +0x1E uint16 category (indexes a table at DS:3F80;
  value 3 is tested separately at seg1:43C3; meaning open), +0x20 uint16 open.
  The game never writes these records.
- **Town state** DS:5A (allocated `towns · 0x6F`): byte +0 owner of the whole
  town (seg1:7CF6 sets it when one player owns every existing entry, else 0xFF),
  then 10 × 11 bytes per entry: +0 owner (0xFF free), +1..+8 double burden,
  +9 landlord while rented, +10 rent months left.
- **Purchase** seg1:7DC5(player, town, entry, factor*): with a factor pointer the
  price is value × factor. Otherwise DS:3FC6 selects the mode, set to card type +
  1 by the handlers of cards 1–8 (seg3:1361–1495, then seg5:71B3(0xB)):

  | Mode | Card | Rule in seg1:7DC5 |
  | --- | --- | --- |
  | none (0xFF) | — | free entry; cash (+0x42) ≥ value; pay value |
  | 2 | Verkaufen | not handled here: normal purchase (round 14); selling × 1.5 |
  | 3, 4, 5 | Kaufen, Gold, Platin | free entry; price value × 0.75 / 0.5 / 0.1 (DS:86A6/AE/B6) |
  | 6 | Mieten | another player's entry: landlord = owner, months = 12, owner = player |
  | 7 | Nimm | another player's entry: owner = player |
  | 8 | Abkaufen | another player's entry; cash ≥ value; owner receives value, buyer pays. A free entry goes on to the normal purchase at its value (seg1:806C → 8176, round 147); the mode stays |
  | 9 | Kredit | free entry; no cash check; value added to player +0x51A |

  Prices are not multiplied by inflation. After each purchase the whole-town
  owner is recomputed.
- **Monthly settlement** seg1:85F9, called once per month advance from
  nextPlayer (seg2:0795, seg2:07DC): for each active player (outer loop) and
  town (inner loop): reset +0x52 (income) and +0x5A (property value) first;
  factor = 2.0 (DS:86C6) if the player owns the whole town, else 1.0; for every
  entry the player owns: income = value × 0.001 (DS:86BE) × income factor ×
  factor; the property value goes to the landlord if rented, else to the owner;
  a non-zero burden is reduced by the income first (a surplus is paid out and
  the burden cleared), otherwise the income is paid and added to +0x52; a rented
  entry counts down its months and returns to the landlord at 0.
  *Correction (round 149):* the reset of +0x52 / +0x5A happens once per player at the
  start of his own pass (seg1:8644 / 865B), not per town; only entries 0..7 are
  settled (seg1:86B5); afterwards +0x66, +0x62 and the year's worth +0x6A are updated
  (see "Year balance", the board). The use of +0x4A: the cash flow of the month
  (seg2:59E6 / 5A47 add to it); seg1:8CBA clears +0x52 and +0x4A of all slots after
  every month (seg2:07E6). *Round R1:* +0x4A is modelled (`PlayerMoney::monthFlow`) and
  shown by the statement STAT, see "Monthly statement".

Rebuilt in `src/economy.cpp` and wired into the game: starting cash, purchases
in all modes above (the game exposes the normal purchase after stopping in a
town), whole-town bonus, monthly settlement, rent expiry.

Open: how a burden arises, the entry category and
+0x20 word, whether visitors pay anything to owners, and the rest of the town
dialog.

### Selling, credit and card effects (round 9)

- **Selling** seg1:832A(player, town, entry, factor*): only an own entry that is
  not rented (landlord byte 0xFF). The entry becomes free and the player gets
  the value, × 1.5 (DS:869E) while DS:3FC6 = 2 (card Verkaufen); with a factor
  pointer the value × factor.
- **Year end** (seg2:0784–07E6, when month mod 12 = 11): seg1:2A30 message,
  seg1:8AA7 subtracts every positive credit debt (+0x51A) from cash, seg1:85F9
  monthly settlement, year + 1, inflation, seg8:88C6 year-end window (consumes at
  least one `rand()` % 7 at seg8:88DF; not modelled, so random sequences diverge
  after a year end), seg1:8BC6 resets yearly counters (+0x62..+0x6C),
  seg1:8B41 clears the credit debts.
- **Card target** seg2:51EA(current): one active player → the first active
  slot, two → the other one, more → a selection dialog.
  *Round 53:* argument 0xFE returns the last choice DS:8C04; 0xFF excludes
  nobody (Tschüss). The dialog SELPLAYER (seg2:5386) has one button per
  eligible slot with `%s\%d\pl_menu\%c%d.bmp` (letter, status), button size
  0x74 / 0x5B / 0x49 for 1024 / 800 / 640 and extra height 0x14 / 0x10 / 0x0E;
  window n · (size + 5) + 20 by extra + size + 30; buttons at y 5, the first
  eligible slot at x (size + 5) · (n − 1) + 5 and each following one size + 5
  further left (seg2:53BD ff.); button id 0x1392 + slot. Rebuilt in
  `src/select_player_view.cpp` for the cards with a target in the game window
  (background `%s\%d\p.bmp` (DS:1BA3) assumed; the window centred).
  *Round R4:* there is **no cancel**. The procedure (seg2:562C, dispatch seg2:598E)
  has no WM_KEYDOWN and no right-button case; WM_COMMAND (seg2:566A → 56E6) acts
  only on 0x1392–0x1399 (notification 2: focus change, the name area repaints,
  seg2:5670; otherwise DS:94BC = slot and DestroyWindow, seg2:56B9). Escape /
  Backspace on a button post WM_COMMAND 0xFFFF (seg1:0F77), the right button on a
  button posts VK_BACK to itself (seg1:0F37) — both reach DefWindowProc (seg2:56CC).
  Alt+F4 is swallowed by the focused SBBUTTON (WM_SYSKEYDOWN seg1:0F4B passes on
  only F10); the window is a child (style 0x44000000, seg2:538D) without a system
  menu. The modal loop seg6:0FB4 ends only when WM_DESTROY clears DS:A2A0
  (seg2:5780), i.e. after a choice. The card has been used up before (dispatcher
  seg3:0E94 → seg2:3C62), so it cannot be taken back; Mitspieler (seg4:0803 →
  51EA(0xFF) → seg2:5E93) must toggle one driver too. Rebuilt: the right button,
  Escape and Backspace do nothing in SELPLAYER.
- **Money cards** (each handler calls nextPlayer seg2:06FC afterwards, so the
  card replaces the turn):
  - Fiskus (seg3:14AD): target pays (cash + property value +0x5A) × 0.1 (DS:8636).
  - Konto (seg3:15B0): with more than one active player every cash balance is set
    to the average of all active players. *Round 151:* with one active player
    (seg2:0368 returns 1, seg3:15C4) the handler returns at once (seg3:15C9 → 1787):
    no text 11011 and no nextPlayer, so the turn goes on; the card is already used up
    by the dispatcher (seg3:0E94 → seg2:3C62 before the call at seg3:0EDB). The
    averaging goes through seg2:5A47 / 59E6 (seg3:16D1 / 1700), then stores the
    average at +0x42 (seg3:1708). Rebuilt in `Game::playCard` (rules revision 43).
  - Schulden (seg3:1790): the target is chosen first (seg3:17B6); only if the
    player's cash is negative (seg3:17E0) the target takes the negative amount and
    the player's cash becomes 0. *Round 147:* the card can be played at any balance
    (the card window checks only the category, seg3:0701); otherwise it is used up,
    text 11012 shows the amount 0.0 (DS:863E) and the turn ends. The rebuild refused it.
  - Zero (seg3:18EA): the player's own cash becomes 0 through seg2:5A47(player,
    cash) (seg3:191F), which also subtracts from +0x4A (modelled since round R1). *Round 151:*
    the following seg5:607B(0xFFFF) (seg3:1929) is the PLINFO panel refresh: with no
    panel window (DS:3C = 0) it creates PLINFO (seg5:6119, class DS:39F6), with
    0xFFFF it sets DS:3930 = 0xFF and repaints the whole panel (seg5:614D..6173),
    otherwise the item's rectangle (seg5:6176 ff., mask table DS:3920). No rule
    effect; the rebuild draws the panel from the state every frame. ZIEL's keys
    (seg7:4D22 / 4D37) call it too; where the finish and announcement voices really
    stop is traced in "Finish" and "Destination announcement" (round R4).
- **Status cards** set a bit in the target's +0x40 flags and then post
  WM_COMMAND 0x2710 (one-reel spin, seg4:0A27), so the player still moves:
  Zucker (seg3:19E5) bit 0x02 and Schlafen (seg3:1B0A) bit 0x04 → sit out once
  (texts 1000–1002, 1010–1012; `random(2)` yields 0..2); Blockieren (seg3:1AB9)
  bit 0x08 → the target cannot use cards (checked at seg2:0B29, texts 1020–1022),
  cleared when that player's turn ends; Steuerlos (seg3:1994) bit 0x20 → the
  target's vehicle is driven by the computer routine seg5:A1CD for a turn (not
  rebuilt, depends on the computer player). Tschüss sets bit 0x10 (seg3:2B88,
  meaning open).

Rebuilt: selling at full value from the town, credit charge and clearing at the
year end, target resolution, Fiskus, Konto, Schulden, Zero, Zucker, Schlafen
and Blockieren. Open: the remaining 23 card types, Steuerlos (needs the
computer player), the year-end window's random calls, what happens with a
negative balance.

### Dr. Drago and further cards (round 10)

- **Card names**: texts 12000 + type in `rally_d.dat` name every card type and
  confirm the mapping from the card pictures (e.g. 12016 Schlafen, 12039 Nessie).
- **Dr. Drago** rides with the player in DS:9CFC (0xFF = none; a new game sets
  0xFF at seg2:0324). Texts 3000–3255 describe his deeds. Placement seg2:9C97:
  at every stage change (seg2:146D/14A6/1535) and by the card Konferenz
  (seg3:30A9); it restarts the form countdown DS:3DB4 = random(3) + 4 (seg2:B48D).
- **Forms** DS:9CFD (data-segment initial value 0; texts F1/F2/F3 at DS:3DE2):
  at the start of the holder's turn (seg2:049F → seg2:B3A4) the countdown
  decreases; at zero it restarts and the form changes 0 or 2 → 1, 1 → 2 if
  year > 5 and random(2) ≠ 0, else 0. The change itself is animated through
  seg2:9804 (route of Dr. Drago's own vehicle; the vehicle pictures are not
  matched to the forms yet).
- **Meetings** seg2:B290 (called while a vehicle stops, seg2:1F67/1F92/1FA0/2057):
  with more than one active player, the holder passes Dr. Drago to the first
  other active player on the same field (+0x3C); a player stopping on the
  holder's field takes him.
- **Actions** seg2:B58C after field actions of kinds 1, 2, 3, 5, non-destination
  towns and sit-out turns (seg2:0977/0A15): form 0 → spend; form 1 / 2 →
  16-entry tables indexed by random(15):

  | Form | Spend | Give money | Tear card | Give property | Buy dearly | Block |
  | --- | --- | --- | --- | --- | --- | --- |
  | 1 | 4 | 6 | 2 | 2 | 2 | 0 |
  | 2 | 1 | 3 | 2 | 4 (incl. seg2:BDA8) | 3 | 3 |

  - Amount (seg2:B793): blue-field month table × 0.5 / 1.0 / 2.0 by form
    (DS:865E/6666/866E), drawn in 100 steps, × inflation.
  - Spend (seg2:BA92): text random(19), amount, holder pays, random(1).
  - Give money (seg2:BEA4): text random(4), amount; recipient = last of the
    net-worth ranking (seg2:5FF7 recomputes property values, seg1:8EE1 sorts by
    cash + property, descending) or, if that is the holder, a random other
    player (seg2:5F03: random(n − 1) over the others).
  - Tear card (seg2:BDCA): no cards → give property; else slot random(count − 1);
    an occupied slot is discarded (seg2:3C62), text random(7).
  - Give property (seg2:BCAF): seg2:B943 picks among the holder's entries with
    0 < value ≤ 30,000 × inflation (DS:8676) the k-th with k = random(n − 1);
    sold with factor 0 (DS:867E); none → buy dearly.
  - Buy dearly (seg2:BB95): the same pick among free entries; bought for value
    × 1.5 (DS:8686) without a cash check.
  - Block (seg2:BFB0): holder flag bit 0; seg2:089D checks bit 0 before bits 1/2,
    clears it and lets the player sit out only while still holding Dr. Drago
    (seg2:C00C, text 3251 + random(4)).
- **Weihnachten** (seg3:1156): outside December a message and the turn ends; in
  December with fewer than 4 hand cards and a card left the top card is given
  (seg2:4327), otherwise (random(10) + 10) × 1,000 (DS:862E) × inflation. The
  turn ends (seg2:06FC).
- **Hau ab** (seg3:3EFB): always draws a random other player (seg2:5F03); if the
  player holds Dr. Drago he moves there (seg2:B4E3); the turn ends.
- **Passive cards**: Schneepflug, Versicherung, Nägel weg and Nessie (category 4)
  have empty handlers — they are used by the event system (`schnee.sbs`,
  `nessie.sbs`, …), which is not analysed yet.
- **Sit-out texts** consume random(2) (seg2:0915/09B3); the yellow field text
  random(13) (seg2:1127). *Round R3b:* within one sit-out the sound (seg2:095B /
  09F9) starts first, then the modal message (seg2:096F / 0A0D), then Dr. Drago
  acts (seg2:0977 / 0A15). The rebuild shows each text where `nextPlayer` reaches
  it (`TurnHooks::announce` → `Game::announceSitOut`), before the sit-out hook and
  before a later player's event; the random sequence is unchanged.

Rebuilt: Dr. Drago state, placement, form countdown and changes, meetings, all
six actions, Weihnachten, Hau ab, the block flag in nextPlayer and these text
draws; nextPlayer now runs the credit charge, settlement and credit clearing
at the original points through hooks. Assumptions: Dr. Drago changes hands
when a move ends (the original checks while the vehicle stops on fields), and
the stage placement uses distances to the new destination. Open: Konferenz,
the event system with the passive cards, Tschüss, Punch, Erdbeben, Nägel,
Risiko, Flugzeug, Steuerlos, the card field, property cards in the town dialog.

### Dr. Drago's own vehicle on the map (round 139 analysis, round 140 rebuilt)

Dr. Drago is not only a portrait (`<n>pd.bmp`): rally.exe moves him as a sprite
of his own that follows the vehicle he rides with. *Round 140:* following,
frames and drawing are rebuilt in `src/drago_vehicle.cpp` (tested) and the game
window; *round 141* adds the form change's drive and Hau ab.

- **Per tick** seg2:9F61, called from the movement tick seg2:1C71 whenever
  DS:9CFC ≠ 0xFF (guard DS:3DE0 against re-entry). His pixel position is DS:9D01 /
  9D03. It calls the step seg2:A0A5 several times per tick (DS:3D8C):
  - travelling (DS:9CFE = 1) while his 64 × 64 rectangle (raised by 0x28 in form 2)
    misses the view inflated by 5 (seg2:9ECB → seg2:42A4): steps without drawing
    until he is visible, then 3 steps per tick;
  - forms 0 and 1: 3 steps, 4 once the trail holds two or more entries
    (seg2:4EC4 ≥ 2), back to 3 at one or none;
  - form 2: 3 steps when |x − holder +0x16| or |y − holder +0x18| exceeds 0x90
    pixels (_abs seg10:1DB2), otherwise 2.
- **Trail** ring buffer of 1000 words at DS:2C, head DS:28, tail DS:2A, count
  DS:26 (reset seg2:4CEB). seg2:4D3F appends a field unless it repeats the last
  one; the drive appends at seg2:1FF5 and seg2:20C7 (leaving / reaching a field),
  the form change at seg2:9A02 / 9A98, seg2:B33E / B38D elsewhere. seg2:4D9F /
  4EE7 consume it; seg2:ADAB(field, x, y) heads for the trail's next field and
  returns the direction DS:3D78. Within 0x30 pixels of the holder (both axes)
  he stops following.
- **Form change** seg2:B3A4 (start of the holder's turn): the music F1 / F2 / F3
  (seg1:2A30, DS:3DE2 ff.) and seg2:9804(new form). It picks a start field
  DS:94D8 with seg2:454E: a depth-first search over the links from the holder's
  field, first direction random(3) (seg6:1343), taking the first field whose
  7 × 7-cell neighbourhood (seg2:46C8: field cell inflated by 3) lies outside the
  view; route seg2:9996 between it and the holder, DS:9CFE = 1, DS:94EC = the new
  form. seg2:98D0 completes it (DS:9CFD = DS:94EC, seg2:9BAE, A0A5, AF38) and sets
  DS:96A0 = 1, which ends the wait loop at seg2:B454.
- **Pictures** (DS:3DAC ff.): form 0 `sbs\F_BIKE.sbs` (FAB… bicycle, FAR…),
  form 1 `sbs\F_CAR.sbs` (FA…, FAF… flying, R… rockets), form 2
  `sbs\F_BLIMP.sbs` (ZEP…, FAZ… in eight directions). The step looks at the cell
  properties (seg2:3260) of his cell and its neighbours; bit 8 (water) selects
  the alternative frame tables (DS:3C0E / 3C12 by direction × 0x14, DS:3B7E /
  3BA2 by DS:3D76 × 4, set with seg2:AC89). The overview's Q_FL marks DS:9D01 /
  9D03 (seg1:A6A7), not the holder's field.
- **Step** seg2:A0A5: cut a loop of the trail (seg2:4EE7: when its oldest field
  appears again later, skip to that copy), choose the direction DS:3D78 (moving) or
  DS:3D76 (facing while standing), pick the frames, then move one pixel by DS:3D90 /
  3DA2 {0, 0, 1, 1, 1, 0, −1, −1, −1} / {0, −1, −1, 0, 1, 1, 1, 0, −1} (directions
  1 up clockwise to 8 up-left) and drop the trail's oldest field once he is on it
  (seg2:ADAB = 0).
- **Direction** seg2:AE77(tx, ty, x, y): per axis 0 equal, 1 target larger, 2
  smaller; table {0, 5, 1, 0, 3, 4, 2, 0, 7, 6, 8, 0}[sx · 4 + sy]. seg2:ADAB uses a
  field's cell × 16. Straight lines, not roads: he cuts corners between fields.
- **Following, forms 0 / 1** (seg2:A5C4): one trail entry left within 4 pixels
  clears the trail; one left and farther than 0x30 → towards it; empty and far →
  straight at the holder; empty and near → stand and face the holder; two or more →
  towards the oldest; one left and near → stop facing the last direction. Form 1
  while rising or landing (DS:9D2F 1 / 3) keeps going straight (seg2:A732).
- **Following, form 2** (seg2:A9F3): straight at the holder while |dx| > 0x30 or
  |dy| > 3 (sic, seg2:A9FB), else stand facing him.
- **Frames.** Records as for events (delay at +0, count at +3, names from +4); a
  new record starts at frame 0 with countdown 0, each pass decrements and an old
  value below 0 reloads the delay and advances (seg2:B0D6 / B21C). Form 0: moving
  DS:3C0E (`far…` bicycle) or DS:3C12 (`fab…` boat) when his cell has bit 8; standing
  DS:3B7E / 3BA2. Form 1 on the cell grid (seg2:B255) and moving: from a field cell
  (bit 1) whose neighbour ahead has a road (bit 2), `fa…` with the rockets `r…`
  (DS:3C16 / 3CCA), or the hovering `faf…` (DS:3C1A) when that neighbour is a sea
  route (bits 2 + 4); off the grid the own cell decides. A hovering record starts a
  hop (seg2:AB09 → DS:9D2F = 1): seg2:AF38 raises DS:9D30 by 2 per pass to 0x18
  (state 2), seg2:AB49 looks three cells ahead for a field with a road and lands
  there (state 3, down by 2, then the position snaps to that cell). Any other record
  ends a hop. Form 2: `faz…` (DS:3C1E) under the blimp `zep…` (DS:3CD2), eight
  directions.
- **Placement** seg2:9C97 / 9CF9 (stage start, Konferenz, loading): the side behind
  the holder's facing ({2, 3, 0, 1}), turned clockwise until the holder's field has
  a link there, 48 pixels away (DS:3D7C / 3D84); standing record of that side, form 1
  hovering at height 0x18 on water, form 2 `zeprido` until the first step.
- **Drawing** seg2:AF38 into the sprite list (then −16, −16 like the vehicles):
  form 0 at (x, y − 10), form 1 at (x, y − height + 2) with the rockets at (+16, +28)
  up, (−7, +18) right, (+16, −4) down, (+42, +18) left (minus the height), form 2 the
  figure at (x − 5, y − 45) and the blimp at (x − 5, y − 85). seg2:4F91 draws him
  before the vehicles, in form 2 after them.
- **Form change drive** (round 141): seg2:9804 returns unless DS:9CFD and DS:9CFC
  are set (seg2:C0F9) and the form differs, so at a form change the start field
  search always draws random(3) after the countdown and form draws of seg2:B3A4
  (rules revision 39). Route seg2:9996(holder field, start field) fills the trail
  (cleared first; without the mask DS:A10F; the first field is dropped when the
  second lies in line with it on his row or column, seg2:9AA0). Forms 0 / 1 follow
  it while DS:94EC holds the new form (seg2:A109 / A254) and call seg2:98D0 when it
  is empty; form 2 flies straight at its last entry and calls seg2:98D0 on arrival
  (seg2:A8BB). seg2:98D0 then routes back seg2:9996(start, holder), sets DS:9CFD,
  loads the new form's sprites (seg2:9BAE) and takes a step; the new form follows
  that trail and ends the travel within 0x30 pixels (forms 0 / 1 seg2:A154 / A29F,
  form 2 seg2:A961; also when the trail runs out while far, seg2:A645), setting
  DS:9CFE = 0 and DS:96A0 = 1. While travelling out of sight he steps without
  waiting (seg2:9F96, the 64 × 64 rectangle against the view inflated by 5,
  seg2:9ECB), then three steps per pass. seg2:B454 pumps messages and idle passes
  until DS:96A0 is set: the game waits.
- **Hau ab** seg2:B4E3: route from the old to the new holder's field, the music
  F3 (DS:3DEB), DS:9CFC = the target, DS:9CFE = 1, DS:9566 = 2; the same wait
  (seg2:B55E), then seg1:2889.
- **Rebuilt approximations:** the trail gets the holder's fields from the drawn
  drive; a meeting adds the new holder's field; the vehicles' ticks of a frame run
  before his; when the old form's step triggers the switch, the old form's frame
  choice for that step is skipped (rally.exe sets one old-form record that the next
  step replaces). The game window waits through `GameScreen::dragoTravelling`
  (modal like a dialog, so input and computer turns pause). Not saved, as in
  rally.exe: loading frees his sprites and calls seg2:9C97(holder) (seg8:B05D ff.),
  so he is placed anew beside the holder. seg5:5E9D (a dialog that moves the
  current player to a town chosen by name, seg5:5D06) re-places him too; how that
  dialog is reached is not traced. Unread: the sprite list argument +0x18 (1, or 0 for the blimp).

### Events and Eastern Europe (round 11)

- **Trigger** seg8:1624, called inside nextPlayer after the month bookkeeping
  and before the new player's flag checks (seg2:07EF/07FC): DS:A0FE + 1 (player
  changes), DS:A111 counts down; seg5:186A reloads the season's tiles and, if the
  season changed, runs the screen wipe seg8:0E20 and **no event happens**
  (the wipe draws `rand()` % 20 pairs until a free cell of a 20 × 20 grid is hit,
  so its number of random calls is not fixed; not modelled). Otherwise DS:A108
  counts down; at 0 seg8:1B91 restarts it with `n · (10 − year / 32) + random(n)`
  (n = active players).
- **Eastern Europe**: the 176 fields with flagset 0x4000 are Eastern Europe. A new
  game sets DS:A10F = 0x4000 (destinations and routes skip them) and seg1:A15B
  stores in DS:A113 the total value of all entries of western towns (kind 4/6
  fields without 0x4000). While DS:A10F ≠ 0 the trigger compares the sum of all
  players' property values (+0x5A) with `factor[n] · 0.01 · A113`, factor =
  0, 10, 12, …, 24 (DS:870E ff., i.e. (8 + 2n) %); if it is exceeded or the year is
  above 40, seg8:69F0 opens the East (texts 5001–5003): it replaces the barrier
  tiles (e.g. 0x743 → 0x720/0x722) and sets DS:A10F = 0 (seg8:6C46).
- **Barriers**: the 15 cells with tile flag 0x20 (tiles 0x750/0x770) lie on
  exactly the 15 links between western and eastern fields (data check). When a
  vehicle would enter such a cell, seg2:2404 looks up the road-obstacle table
  DS:5E (30 × 0x17 bytes: +0 type, +6/+8 cell; seg8:68FF): without an entry or
  without hand cards it returns 0 and the vehicle stops. Obstacle types 4, 8, 16
  can be cleared with card 37 (Schneepflug), type 0x900 with card 39 (Nägel weg)
  (seg3:0D26 finds the card, seg3:0E11 discards it, seg8:697C removes the
  obstacle; humans confirm with IDYES).
- **Event tables** seg8:180B (16 entries each, random(15)), chosen by progress:
  turns < 120 / n → table 1; month < 500 / n → table 2; month < 900 / n →
  table 3; otherwise table 4:

  | Table | Rain | Storm | Snow | Rockfall | Robbers | Flood | Quake | Drought | Nessie | Volcano | Professor (1/0) |
  | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
  | 1 | 4 | 4 | 4 | 2 | – | – | – | – | – | – | 2 / 0 |
  | 2 | 2 | – | 2 | 2 | 3 | 2 | 2 | – | – | – | 2 / 1 |
  | 3 | 2 | 2 | 2 | 2 | 2 | 2 | 1 | 2 | 1 | – | – |
  | 4 | 1 | 1 | 1 | 1 | 3 | 2 | 2 | 1 | 2 | 1 | 1 / 0 |

  Handlers: Regen seg8:42B0, Sturm 1C01 (via 5F1E), Schnee 535E, Steinschlag 5A76,
  Räuber 1E70, Hochwasser 4964, Erdbeben 3C0B, Dürre 2386, Nessie 2A32,
  Vulkan 36FD, Professor 6FBF(1) / 6FBF(0) via 6F71 / 6F98.
- **Robbers** (seg8:1E70): victim = first active player in slot order with cash
  > 500,000 × inflation (DS:875E) standing on a field with a right-hand link and
  no sea route two cells to the right (animation space); the victim loses 25 %
  of the cash (DS:8766), text 5021 + random(2). Nobody qualifies → no effect.

Rebuilt: trigger, countdown, season-change skip, table selection, the East
(western value, opening threshold, barrier links, destination mask) and the
robbers; the other events are chosen and announced. Open: the effects of rain,
storm, snow, rockfall, flood, earthquake, drought, Nessie, volcano and the
professor (road obstacles in DS:5E, property damage, the passive cards Versicherung
and Nessie), Nebel (text 5180, not in the tables).

### Road obstacles and property damage (round 12)

- **Obstacle table** DS:5E (30 × 0x17 bytes): +0 type, +2/+4 fields of the link,
  +6/+8 cell, +0xA duration, +0xC saved map value, +0xE layer, +0xF double.
  seg8:67F8 adds an entry (an obstacle on the same cell is removed first, then
  the first free slot is used). Types: 0x4 Steinschlag, 0x8 Schnee, 0x10 Regen,
  0x900 Nägel (card), 0x400/0x800 investments (round 13). The events write the
  tiles 0x826 / 0x20F / 0x836 into layer 1 and give that tile flag 0x20 through
  seg2:3305 (which writes `pit[0x1000 + tile]`), so a vehicle entering the cell
  runs seg2:2404 (round 11). There, obstacle types 4/8/16 can be cleared with card
  37 Schneepflug, 0x900 with card 39 Nägel weg (humans are asked); otherwise the
  vehicle stops.
- **Expiry** seg8:61FE runs once per turn change (seg2:071B, before the player
  loop): entries with duration 0 are removed (seg8:62FB restores the tile),
  others count down. Durations: Regen and Schnee 3n, Steinschlag and Nägel 4n
  (n = active players).
- **Placement** seg8:5F43 (Regen, Schnee; up to ten tries, seg8:4460): a random
  active player (seg2:5F03(0xFF)), field distances from the destination with the
  route mask, `k = random(d − 1)` and k + 1 steps along the shortest way, then the
  first link (up, right, down, left) whose cell two steps from the field is no
  sea route and no foreground cell (seg6:2CF7). Steinschlag uses 11 fixed cells
  cycled through DS:7170 (first index 1). The Nägel card puts its obstacle two
  cells from the player in the chosen direction (seg3:2096).
- **Property damage** seg8:4F1B(town, card): every player slot owning entries 0–7
  of the town (8 and 9 are not checked) either holds the protection card (it is
  used once) or loses those entries (owner, burden, rent months and landlord
  cleared); then the whole-town owner is recomputed. Protection: card 38
  Versicherung, for Nessie only card 40 Nessie.
- **Damage towns**: Sturm, Dürre and Hochwasser use seg8:6C77(category) with
  categories 4, 3 and 1 (entry word +0x1E): towns with an entry of that category
  and any owned entry, otherwise all towns with such an entry, random(count − 1).
  Nessie (13 coastal towns, DS:680C), Erdbeben (12 towns, DS:6AC4) and Vulkan
  (Palermo/Ätna and Reykjavik/Hekla, toggling, DS:69F0) use fixed cyclic lists,
  each advanced before use. Text draws: one random(2) before the damage, two for
  the volcano.

Rebuilt in `src/obstacles.cpp`, `src/events.cpp`, `src/economy.cpp` and
`src/game_events.cpp`: obstacle table, expiry, placement, clearing with the
cards, rockfall cells, the six damage events and the protection cards. In the
field graph an obstacle blocks the link that contains its cell. Simplifications:
cards are used automatically (the original asks humans); random events can be
switched off as a variant. Open: Nebel (the professor, the Nägel direction and
the types 0x400/0x800 follow in round 13).

### Professor, football club and further cards (round 13)

- **Investment events**: the event tables' entries 6F71 and 6F98 both call
  seg8:6FBF with flag 1 (Professor, texts 5320 ff.) or 0 (Fussballclub, texts
  5400 ff.); the former names ProfessorGood/Bad were wrong. The offer goes to the
  player whose turn starts (DS:94E2) only if the cash exceeds 10,000 (DS:8776).
  The dialog changes the amount in steps of 1,000 (DS:877E) up to the cash. On
  confirmation an obstacle-table entry is added: type 0x400 (Professor, DS:9222
  set) or 0x800, duration = active players (seg2:0368), cell 0/0, layer byte
  +0xE = player, double +0xF = amount; the amount is subtracted at once
  (seg2:5A47).
- **Payout** on expiry (seg8:62FB → 652C for 0x400, 6394 for 0x800):
  `random(3) == 0` loses the money (texts 5381/5441 + random(2)), otherwise
  amount × 2.0 (DS:876E) is added (texts 5341/5421 + random(2)).
- **Erdbeben** (card 17, seg3:1BDE): text via seg2:B74D (random(2)), town of the
  player's field (seg2:276B, 0xFF = none), then seg8:4F1B(town, 38 Versicherung)
  and nextPlayer. Without a town (seg3:1C27) neither the message nor the damage
  follows, only the draw (round 147; the rebuild showed the text without a town).
- **Nägel** (card 18, seg3:1C96 / 2096): the player chooses a direction with a
  road; the obstacle (type 0x900, duration 4n) lies two cells away. The turn
  continues (no nextPlayer).
- **Punch** (card 20, seg3:2D17): for the first other active player standing on
  the same field (+0x3C), the start direction comes from the table {2, 3, 0, 1}
  indexed by the puncher's vehicle facing (frame byte +2, seg2:2AF6 / DS:1994:
  1 up … 4 left), i.e. the opposite direction. Four times: the link in the current
  direction is taken if it exists and the target field is not masked (DS:A10F);
  otherwise the direction turns clockwise, skipping the puncher's own facing.
  Then nextPlayer.
- **Konferenz** (card 21, seg3:2F5F): every other active player gets the
  current player's position (+0x16/+0x18, field +0x3C, +0x2D, frame +0x20,
  +0x1E = 0); if that player carries Dr. Drago, seg2:9C97 places him anew.
- **Flugzeug** (card 27, seg3:31D5): a window (resource 0x3381) chooses a target
  town; the flight ends with the town's field action.
  *Round 54:* window TEMP 0x114 × 0x100 (procedure seg3:3384) on `%s\f.bmp`
  (butt); heading text 5601 (Arial 20 weight 600), rows 5602 + i (London,
  Brüssel, Berlin, Rom, Oslo, Madrid, Athen, Moskau; Arial 18) at y = 0x32 +
  0x18 · i, x 0–0xF0, colour 0x45, centred; DS:8C1E = 7 (with Moskau) when the
  east is open (DS:A10F = 0), otherwise 6. A click on row 1 (Brüssel) while
  DS:A104 = 0 shows text 8300 instead (seg3:33F2). The chosen name is matched
  against the towns with `_fstricmp` (seg3:32CE ff.), then the player flies
  there (seg2:04AD) and the field action follows. Rebuilt in
  `src/flight_view.cpp`. **Input** (round 101, from
  `docs/original-flight-dialog-audit.md`, checked at seg3:3971 / 3565 / 346D):
  only WM_KEYDOWN and WM_LBUTTONDOWN; Up / Down clamp at the first / last row and
  play Plopp02 on channel 2 when the row changes (seg3:34A8 / 34F6); Enter books
  the row; a click books the row it hits; no mouse-move selection, no Escape and no
  right button (the booking cannot be cancelled); the chosen row is white.
  **Assumption:** the window centred.
- **Verdoppeln** (card 40, seg3:3FBB): sets DS:2EB0 = 1; the card handler at
  seg3:0E83 then skips removing the next card (seg2:3C62) and clears the flag.
  seg2:0CE8 clears it at every player change.

Rebuilt in `src/events.cpp` (`investmentPayout`), `src/game_cards.cpp` and
`src/game_events.cpp`. Simplifications: Punch uses the direction of the
player's last step as facing; Konferenz copies
it to the others like the frame pointer (seg3:307C–3096, roadmap R2). *Round R3a:*
the start facing seg2:5067 is the first direction with a link (record +2 + 2k ≠
0xFFFF), animation mode 0 with +0x2D = 2 / +0x2E = 0x18 on a cell with bit 8,
otherwise mode 2 with 0 / 0 (seg2:5137–51BB); without a field at the cell the
player leaves play (seg2:50B7). Callers: the new game for all eight slots
(seg2:0184, London: right), seg2:04AD (seg2:0581), the Master-Rallye (seg2:5E4D),
seg5:6C47 (a computer off the fields) and loading (seg8:B021). seg2:04AD, the
position setter of Tschüss (seg3:2CBC), Punch (seg3:2F26) and Flugzeug
(seg3:3302), also places Dr. Drago anew (seg2:9C97, a random(3) draw) when the
moved player carries him (compare with DS:9CFC at seg2:058C, call seg2:9C97 at 0597). Rebuilt as `Game::setPosition`; loading keeps
the saved facing and Dr. Drago's countdown (not seg8:B021 / B06A); the viewer invests
half the cash rounded to 1,000 with I and flies to the current destination with
Flugzeug. Open: property cards 1–8 (town dialog modes), Steuerlos, Tschüss,
Risiko, card fields, Nebel, computer players.

### Property cards, Tschüss, Steuerlos and Risiko (round 14)

- **Card window mask** DS:18: seg3:0887(mask) / seg3:0EE8(mask) set it; a card is
  enabled when `(category & DS:18) == category` (seg3:0701). Before spinning the
  mask is 2 (seg4:0951), from the town dialog's card button 1 (seg1:54C1), after
  a card 0. Category 1 = property cards 1–8, 2 = all other playable cards,
  4 = the passive protection cards (never enabled).
- **Property cards** (seg3:134D–1481) only set DS:3FC6 = card type + 1 and switch
  computer state machine to state 0xB (seg5:71B3, DS:8CDC; computers only, see
  round 16 — the earlier "advisor" reading was wrong). The town dialog's
  card button works only while DS:3FC6 = 0xFF (seg1:549E), so one mode is active
  at a time. Opening a town dialog resets the mode (seg1:4CE1 / 4FB9), closing it
  too (seg1:5399 / 5508 / 5767). seg1:7DC5 resets it after a purchase in mode 6
  (Mieten, seg1:7F87) or 7 (Nimm, seg1:802C); the other modes stay for further
  purchases or sales until the dialog closes. Mode 2 (and 0xFF) buys at the normal
  price; the discount branches are only taken for 3/4/5 (seg1:819B–826A).
- **Tschüss** (card 19, seg3:28ED): target from seg2:51EA(0xFF), i.e. any active
  player including the current one (with two players the dialog is shown too).
  The target gets bit 0x10 while it is carried, then seg2:04AD sets it onto the
  field from seg2:47D2: `random(fields − 1)` (DS:9CFA) until the field is not
  masked (DS:A10F) and its cell rectangle inflated by 3 (seg2:46C8, INFLATERECT)
  does not intersect the visible map rectangle (DS:956A/9568 + window size >> 4,
  seg2:42A4). Then nextPlayer (if DS:2EAE = 0).
- **Steuerlos** (card 13, seg3:1994): sets bit 0x20 of the target's +0x40 and
  spins (WM_COMMAND 10000). At the target's turn seg2:0A6F/0AB8 saves its status
  byte in DS:10, sets it to 2 (computer) and clears the bit; seg5:7509 (computer
  driving) tests the bit. The effect is a computer-driven turn (round 16:
  seg5:A1CD also sets DS:8D2A = 1, so the route is scored with random(1000)).
- **Risiko** (card 26, seg7:674D): opens its own window with a reaction game;
  rebuilt in round 19 (seg10:1DDE is `rand()`, not a clock as first noted).

Rebuilt in `src/game_cards.cpp`, `src/game.cpp` and `src/rules.cpp`
(`resolveAnyTarget`, `pickHiddenField`). Assumption: the visible map area is
40 × 30 cells centred on the Tschüss target (a 640 × 480 view; the original uses
the actual window and scroll position, so its random sequence can differ).
Open: Steuerlos (computer players), Risiko (mini-game), card fields, Nebel.

### Card fields and the deck return (round 15)

- **Card shop** seg3:4437 (window procedure seg3:4640) opens on a violet field
  (kind 5). Mode DS:9C82: 0 choose, 1 buy, 2 sell; selling is disabled while the
  hand is empty (seg3:5231 sets DS:3FBA = 1).
- **Offer list** seg3:5813(0): for each card type in table order, the first
  deck position (DS:9572, size DS:9C8C) whose card has that type; taken positions
  (0xFF) are skipped. seg3:5813(1) lists the own hand slots instead.
- **Buying** seg3:47F1: allowed with fewer than 8 hand cards (seg2:3C00) and
  cash (+0x42) ≥ the card value (DS:2EB4 via seg3:4121, no inflation). seg3:59AA
  swaps the chosen deck position with the draw position DS:94FA, seg2:4327 draws
  it (seg3:032D(1)) into the first free hand slot, seg2:5A47 pays the value.
- **Selling** seg3:4912: value × 0.5 (DS:8646) is paid (seg2:59E6); the card
  returns to the deck through seg3:03B5 and the hand slot is cleared.
- **Deck return** seg3:03B5(slot): the card is written at the return position
  DS:9C94, which advances cyclically (initially 0). seg2:3C62(player, hand slot)
  uses it for every removed card: played cards (seg3:0E94), cleared obstacles
  (seg2:245B / 2530 via seg3:0D26), used protection cards (seg8:4F6C) and cards
  torn by Dr. Drago (seg2:BE3A). Correction to round 7: used cards are not lost.

Rebuilt in `src/cards.cpp` (`Deck::giveBack`, `swapWithPointer`,
`cardShopOffers`, prices) and `src/game_cards.cpp` (`buyCard`, `sellCard`,
`discard`). The shop stays open until the turn ends.

### Computer players (round 16)

- **Status byte** +0x00: 1 = human, 2 = computer. seg2:5E93 toggles a slot,
  seg2:5C02 is true for status 2 while DS:94C4 ≠ 0, the game ends when no slot
  has status 1 (seg2:5C5A). Steuerlos makes the target status 2 for one turn
  (status saved in DS:10).
- **State machine** seg5:6D20, run from the game loop (seg2:1C67) for computers;
  state DS:8CDC (previous DS:8CDE) is set through seg5:71B3, which forces 0 for
  humans. seg5:6BF2 starts a computer turn in state 1; seg5:6CE8 resets.
  Jump table seg5:7102: 1 card choice (seg5:7386 → DS:8D20; 0xFF → state 2 spin
  via command 0x406, else state 3 plays it via 0x407), 5 plan the route
  (seg5:72C2), 6 drive it (seg5:A212 gives the direction, seg2:5BD9 steers;
  state 15 when no link fits), 7 wait 50 ticks and end the turn (0x7EF),
  9 play card DS:8D21 (seg3:4161), 11 follow-up per card type (table at
  seg5:6FCB), 12 card field (seg5:7D85 / 7E4F, list executed by seg3:4266),
  13 town (seg5:8F01 for the current field, executed by seg1:49D4).
- **Profiles** DS:534A + slot · 0x16, far pointers: +0 driving strategy (five
  records of 0x76 bytes at DS:4E60, 4ED6, 4F4C, 4FC2, 5038), +4 card weights
  (DS:50AE, 5164; read by the card choice, +0xA4 the Nimm threshold), +8 card
  worth table DS:521A, +0xC word (shop random range), +0xE money record
  (DS:52A2, 52D6, 530A: property weights by category in words 1–8, cash
  reserve doubles 0 / 10,000 / 20,000 / 50,000 at +0x12), +0x12 target odds
  (DS:5344 / 533E). Slots 0–7 use strategies 4ED6, 4ED6, 4E60, 5038, 4FC2,
  4E60, 4E60, 4F4C, card weights 5164, 50AE, 50AE, 50AE, 50AE, 5164, 50AE, 5164
  and money records 52A2, 52D6, 530A, then 52A2. Round 17 correction: the
  property weights of seg5:990A come from the money record (seg5:8F01 passes
  bp−6 = DS:5358), not from DS:534E.
- **Graph** seg5:8B1B(buffer, flag): 16 bytes per field (four links, nails count
  +8, snow count +0xC). Masked fields (DS:A10F) lose all links, links to them are
  removed. Each obstacle of type 4/8/0x10/0x900 (DS:5E) either removes its link
  in both directions (flag ≠ 0, used by the route planner; always for rockfall)
  or counts as clearable (flag 0: seg5:8CC7 `cmp [bp+0xA], 0; jne remove`; snow /
  rain +0xC, nails +8). *Correction (round 80):* the flag values were swapped in
  this description; the rebuilt callers were right. Directions come from
  seg5:A212: the index k with `next[k] == to`, else 4.
- **Distances** seg5:8DBF: breadth-first search from the destination DS:A10D.
- **Route** seg5:829D(player, steps = DS:A778, …): depth-first over neighbour
  indices 0–3 without immediate reversal (cycles allowed) to exactly `steps`
  fields; each end is scored by seg5:87A8 and a strictly higher (unsigned 16-bit)
  score replaces the stored route (initial best 0). Dr. Drago is tracked along
  the path: carried by the player (0) or by someone else (1); meeting the carrier
  takes him, meeting any other active player while carrying him passes him on.
- **Score** seg5:87A8: random(1000) if DS:8D2A ≠ 0 (Steuerlos); otherwise 1000
  plus, by the end field's kind, the strategy values: kind 1/2 the month entry
  (+0x00 / +0x22), divided by +0x20 / +0x42 when cash ≥ +0x18 / +0x3A ×
  inflation; kind 3 +0x44 (+0x46 with 8 cards); kind 4/6 +0x48, then +0x4A
  without spare money, +0x4C (+0x58 when cash ≥ +0x50 × inflation) if
  seg5:8F01 plans a purchase in the town the player **currently stands in**,
  else +0x4E; kind 5 +0x5A (+0x5C without spare money). Distance d to the
  destination: 0 → +0x5E (1000); else −d · +0x60 while stages remain (DS:A104),
  −d · +0x62 on the last stage, +0x60 back when d > 6; for 1 ≤ d ≤ 6 +0x66 if
  the hand holds card number d + 0x1D (seg3:3FFC counts 1-based numbers: the
  matching "d Felder" card). Dr. Drago: +0x68[form]
  when he is passed on, +0x6E[form] when taken over. Finally + random(+0x74).
- **Spare money** seg5:9A34: cash − reserve[k] × inflation, at least 0; k = 0, or
  Dr. Drago's form + 1 while the player carries him.
- **Town plan** seg5:8F01(list, player, field): nothing unless the field is kind
  4/6 with a town not wholly owned by the player. Entries up to the first empty
  one are classified (free, own, another player's, rented, burdened) and scored
  by seg5:990A (income factor +0x1C plus money-record word 1 + category, at least
  0; the original towns use categories 1–6), then bubble-sorted, highest first. Unless cards are blocked: Nimm on the
  first other player's entry worth more than weight +0xA4 (100,000); otherwise
  the best discount card (Kaufen 25, Gold 50, Platin 90). With spare money every
  free entry whose (discounted: 0.75 / 0.5 / 0.1) value is below the remaining
  spare money is bought. Without any purchase the list is dropped; then Abkaufen
  for one other player's entry below the spare money, else Mieten for the first
  other player's entry. DS:8D28 counts the purchases.

Rebuilt in `src/ai.cpp` (tables, graph, distances, score, route, town plan) and
`src/game_ai.cpp` (`runComputerTurn`, `setComputer`, Steuerlos).

### Computer card play (round 17)

- **Card choice** seg5:7386(player) → DS:8D20 (number), 8D21 (slot), 8D22
  (target): nothing while cards are blocked. Distances d0 come from the flag-1
  graph (snow, rain and nails passable) and the destination; field distances of
  players from DS:22. For each hand card of category 2 the score starts at
  random(W+2) (W = card weights DS:534E), then by number (table seg5:7C44):
  2–5 Walzen W+0x14+2i if W+4+2i ≤ d0 ≤ W+0xC+2i and not under Steuerlos;
  "n Felder" W+0x1C (1000) when it reaches the destination exactly; Hau ab
  W+0x1E[form] while carrying Dr. Drago; Zucker / Schlafen / Blockieren
  W+0x24 / 0x28 / 0x34 + random(next word) with a target from seg5:9B0B mode 0;
  Fiskus W+0x30 with a mode 1 target; Weihnachten W+0x2C in December; Konto
  W+0x40 when cash + W.double+0x38 × inflation ≤ the average cash; Zero W+0x4C
  when cash ≤ W.double+0x44 × inflation (−50,000); Schulden W+0x58 when cash ≤
  W.double+0x50 × inflation, mode 1 target; Tschüss (own DS:22 distance ≥
  W+0x68): the other player closest to the goal within W+0x6A scores
  W+0x6C, or himself (W+0x5E when his distance ≥ W+0x5C, else W+0x64 when ≥
  W+0x62) if that scores higher; Konferenz W+0x74 when he is ≥ W+0x70 away and
  someone is within W+0x72; Punch W+0x7A when within W+0x78 and someone shares
  the field; Erdbeben W+0xAC in a town wholly owned by someone else; 1 oder 2
  W+0xB0 within two fields. Property cards, Steuerlos, Risiko, Nägel and
  Flugzeug keep the random start value. The best score must exceed W+0 (10),
  which is also the starting best; ties keep the earlier card.
  *Round 98:* in all eight profiles W+0 = 10 and W+2 ≤ 2, so a card that keeps the
  random start value (property cards, Steuerlos, Risiko, Nägel, Flugzeug) never
  wins: rally.exe's computers never play these cards from the hand (the Nägel
  window, seg3:1E5E, has no computer path either).
- **Recomp extension** (not rally.exe; `[Extensions] ComputerExtraCards`,
  `--computer-cards`, saved per game, off by default; since round 106 also a switch
  "Computer spielen alle Karten" in the player setup between stop and star, toggled
  by a click or F9 — PLAYER handles F1–F8 and F10 itself): computers may also play
  these cards. Nägel (round 98): when another active player trails at most 8
  fields (flag-1 distances to the destination), the nails go into the available
  direction (Game::nailsDirections) whose neighbour lies farthest from the
  destination; the score is W+0 + 1, so original cards with a higher score still
  win; no extra random draws. Round 99, same score: Steuerlos on the other active
  player (not sitting out, not already steerless) nearest to the destination when
  he is at most 10 fields away and nearer than the computer; Risiko when the
  computer is at least 10 fields away (the Risiko phase then presses as in
  seg7:6927); Flugzeug to the bookable airport (rows 5602 + i matched to towns by
  name at start, `Game::setAirports`; Moskau only with the east open, Brüssel not
  on the last stage) nearest to the destination when it saves more than six
  fields. *Correction:* the property cards are category 1 and never enter this
  loop; the town plan (seg5) already uses Kaufen, Gold, Platin, Mieten, Nimm and
  Abkaufen, so only Verkaufen and Kredit stay unused by computers. Round 100,
  in the town plan after the original choices and only when no property card was
  chosen there: Verkaufen when the cash is negative — the computer's unrented
  entries of the town, most valuable first, are sold at × 1.5 until the cash is
  no longer negative (plan action kind 3); Kredit when nothing could be bought —
  the best scored free entry (BuyMode 9, no cash check).
- **Targets** seg5:9B0B(player, mode): other active players not sitting out
  (bits 1, 2); mode 0 value 100 − distance + W+0x84[slot] + random(W+0x94[slot]),
  mode 1 cash + property value. One candidate is taken; otherwise sorted
  highest first and random(15) is compared with the odds (two candidates:
  odds[0] → first; more: odds[1] → first, odds[2] → second, else third).
- **Card worth** seg5:81AF(player, number): table DS:521A of {number, worth
  before year 20, before 40, later}; unlisted 0xFF. seg5:7CD2 picks the hand
  card with the lowest worth (an unlisted one at once).
- **Yellow field with a full hand** seg3:1057: text 403, the card window in
  discard mode; humans click a card, computers drop seg5:7CD2's choice through
  seg3:4161; then the card is drawn as usual (seg2:1127). Correction to round 7:
  the card is not skipped. *Round 57:* seg3:1057 loads text 403 (0x193),
  formats it with the player name, shows it (seg6:51EA) and calls seg3:0887(0);
  the game window opens the card window after the message and drops the clicked
  card (`Game::discardForCard`). The window cannot be closed without dropping a
  card: the cancel of mode 0 only plays sound 7 (seg4:0B47, round 93).
- **Card field** state 12: seg5:7D85 sells every hand card without a worth
  (adding half its value), seg5:7E4F takes spare money (seg5:9A34) plus the
  sales, collects affordable offers with a worth, scores them worth +
  random(word +0xC), +10 for the card that reaches the destination exactly,
  sorts highest first and buys in order until one is too dear or the hand is
  full. seg3:4266 executes sales first.
- **Investment offer** seg8:6FBF has no computer branch: the modal dialog waits
  for input on any turn. *Round 138:* the modal loop seg6:0FB4 posts 0x7EF to the
  window after the computer wait, but the offer window's procedure (dispatch
  seg8:7C9C, keys seg8:73AB) has no case for it, so it stays open: the human at
  the keyboard answers for the computer, and the amount is taken from the
  computer's cash. The game window now does the same (`Game::setInvestmentWindow`:
  the computer's turn waits while the offer is open). **Assumption:** without any
  human (a demo) or without a window the computer declines, where rally.exe would
  wait for input.

Rebuilt in `src/ai_cards.cpp` and `src/game_ai.cpp`. DS:22 is the distances
from the destination with the route mask (`Game::distanceTable`): round R3b
showed that the array holds exactly that whenever the computer's code runs (see
"DS:22 as state"). Not rebuilt: Risiko.

### Risiko, Nebel and the player setup (round 19)

- **Risiko** (card 26, seg7:674D, window procedure seg7:6BF0): seg2:6426(1, 5, 0)
  spins one reel; the result is the step count DS:A778. Levels L = 2, 3, 4
  (seg7:685D): the light DS:9C92 starts lit (= L), the first pause is
  (400 − 75L) / 10 units of seg6:0CDF(10), then a tick counter starts at
  rand() % 2 and every (475 − 75L) / 10 units the light toggles between L and 0.
  A press (DS:A29E; any key but Escape, or the left button area) while lit sets
  the steps through seg2:7F14 to L · steps / (L − 1) (×2, then ×1.5, ×4/3: four
  times the spin after level 4, text 41003) and moves to the next level; a press
  while dark gives 0 (text 41002). Escape or the right button area (DS:A0EE)
  stops and keeps the steps (text 41001). Afterwards seg7:6BB9: 0 steps →
  nextPlayer, otherwise the move starts (seg2:61B7). Computers press once the
  counter exceeds 7: when rand() % 100 < 25 (even in the dark) or while lit
  (seg7:6927); they never stop. The card choice gives Risiko only its random
  start value, so computers do not pick it.
  *Round 58 (window):* CreateWindow seg7:67E7 (class / title RISIKO, style
  0x54000000, 0xF6 × 0x9E); frames of `%s\risiko.sbs` (DS:54C2, sbs directory,
  each 230 × 142): R-NORMAL, R-LIGHT0, R-LIGHT2, R-LIGHT3, R-LIGHT4, R-BUTTON.
  The paint (seg7:6DD3) picks the frame by DS:9C92 (0 → light0, 2/3/4 → light2/3/4,
  otherwise normal) and copies the pressed buttons from R-BUTTON: x < 0x64 from
  y 0x50 while DS:A29E (CHANCE), x ≥ 0x64 from y 0x50 while DS:A0EE (STOP).
  Click areas (seg7:6C83): y 0x50–0x8C, x 4–0x50 press, x 0x98–0xE2 stop. The
  strings hutman-1 / hutman-2 follow, but risiko.sbs has no such frames. Rebuilt
  in `src/risiko_view.cpp` (centred by seg6:0000, the frame in the client of the
  wooden border, rounds 111 / 112). *Round 117:* input sets DS:A29E (CHANCE) or
  DS:A0EE (STOP) and is ignored while one is set (seg7:6C0D); the flags are
  cleared only when a level starts (seg7:6874) and checked after the running wait
  (seg7:68C5). A STOP repaints (button pressed) and shows text 41001. A CHANCE
  press repaints, plays its sound and then toggles DS:9C92 by ±100 six times with
  seg6:0CDF(400) between them (seg7:6A79–6AB6) — normal frame, light, normal, … —
  before the steps are scored and the next level starts or the result text
  follows; computers press through the same window input (SendMessage,
  seg7:6957). Rebuilt: `RisikoView::blink`, `PlayView::holdRisiko` and the
  window holding back the press's messages for those 2.4 s. **Assumption:** a
  human's STOP shows its pressed button for 150 ms; click coordinates relative to
  the frame.
  *Round 64 (texts and sounds):* a press plays sound 0x17 on channel 2 in the
  dark and 0x18 while lit (seg7:6A46); stop shows text 41001 (seg7:69DE), a press
  in the dark 41002 (seg7:6AE3) and the win after level 4 text 41003 (seg7:6B52),
  each with the driver name (`%Fs`, player record + 1) in the message window
  seg6:51EA. Rebuilt in `src/game_risiko.cpp` (notices and sound cues).
- **Obstacle question (round 65):** seg2:2404 clears an obstacle with card 0x25
  or 0x27 (1-based numbers, types 36 / 38; seg3:0D26 finds it). Computers
  (seg2:5C02) use it at once; for humans the message window opens with buttons 4
  and text 600 ("Willst Du diese Karte wirklich einsetzen ?") and the card picture
  `%s\%d\%c\%dcard_%.2d.bmp` (DS:1A6C: bmp directory, art directory, language,
  art directory, card number), and only the answer 6 (Yes) uses the card
  (seg3:0E11, seg8:697C) and drives on.
- **Message window buttons** (round 65): seg6:51EA(picture, text, buttons) — a
  non-empty picture path replaces the showmaster (height = picture + 0x24);
  DS:8C36 holds the Windows MessageBox type (0 none, 1 OK / Cancel, 3 Yes / No /
  Cancel, 4 Yes / No) and DS:8C38 the answer as an IDOK / IDCANCEL / IDYES / IDNO
  value, which is also the index of the button rectangle (seg6:5F18: texts 114
  Ok → 1, 117 Abbrechen → 2, 115 Ja → 6, 116 Nein → 7, sized by GetTextExtent in
  Arial weight 600 at {18, 16, 14} + 5). WM_CREATE preselects 6 (1 for mode 1,
  seg6:5834). Layout for mode 4 (seg6:639C) in the client rectangle inflated by
  −10, left past the picture + 10, inflated by (−10, 0): Ja at the bottom left,
  Nein at the bottom right. Paint (seg6:5CC1): the chosen answer in palette
  colour 0xFF, the other 0x45. Keys (seg6:579F): left → 6, right → 7, Return /
  space close with the choice, Escape / Backspace send VK_RIGHT (No) and close;
  a left click inside a button rectangle answers it (seg6:54BB); closing plays
  Plopp02 on channel 0. Rebuilt in `src/message_view.cpp` (mode 4) and
  `Game::answerObstacle`; the open question is not saved (after loading the
  step asks again).
- **Quit question** (round 66): seg4:0EDF loads text 700 ("Soll D.T.V.R.
  wirklich beendet werden?") and calls seg6:51EA with buttons 4 and the empty
  picture DS:B6 (showmaster); answer 6 sets DS:88 = 1 and posts WM_CLOSE to the
  main window (seg4:0F26). Callers (round R4, WM_COMMAND jump tables seg4:0BE4 /
  0C21): title menu 1025 (seg4:0733 → 073F; a No runs seg8:0DBD, the title menu),
  MAIN1 1064 (seg4:082B → 0837; a No opens MAIN1 again, seg4:0851), command id 100
  (seg4:055E), the key Q of the main window (seg4:01A6) and WM_SYSKEYDOWN with
  VK_F4 (Alt+F4, seg4:0048; no title test). RALLY's WM_CLOSE (seg4:02F8) posts
  WM_QUIT only when DS:88 ≠ 0, which only seg4:0F26 sets: any other close is
  ignored. The main window is WS_POPUP | WS_CLIPCHILDREN (0x82000000, CreateWindow
  seg5:1BEA) without a caption or system menu. Alt+F4 reaches seg4:0048 only while
  the main window has the keyboard focus; the SBBUTTON windows swallow it (seg1:0F4B).
  The modal pump seg6:03F7 does not test WM_QUIT. Rebuilt in `src/game_window.cpp`
  (askQuit) for 1025, 1064, Q and Alt+F4 in the main window's key handler.
  **Recomp addition:** closing the desktop window (SDL_QUIT, also SIGINT / SIGTERM) asks the
  same question instead of being ignored; a second close while it is pending, or a
  close during the intro, ends at once (`GameWindow::desktopClose`). The question is
  drawn over every window, OPTION, PLAYER, TEMP and the credits picture included
  (`GameScreen::draw`), and takes the input before the modal film, sight and MCTRL
  windows; an open recomp menu is closed first.
- **Nebel**: texts 5180–5183 ("--- Event Nebel ---", the road stays closed) exist
  in the string resources, but no code adds the base 5181 (all other events add
  their text base after random(2), e.g. `add ax, 0x13B1` at seg8:1DFD) and the
  event tables contain no fog entry: the event is unused in this rally.exe.
- **Player setup** seg2:0000 (new game from seg4:0E37): a demo start sets slots
  0–3 to computers (seg2:01AF); otherwise the setup dialog seg1:02BA runs. With
  only one active slot afterwards, random(7) is drawn until it differs from that
  slot, the slot becomes a computer (status 2) and text 961 + random(2) announces
  the opponent (seg2:021F). The new-game order is: event countdown seg8:1B91
  (before the setup, so with no active slot and no random draw), player setup,
  season tiles seg5:186A, shuffle seg3:01AD, deal seg3:0286, first destination
  seg7:372F(2) → seg5:1E2C.

Round 20 (player setup and the end of computer games):

- **Slots** seg1:02BA reads `[Player]` P0..P7 from `rally.ini` in the Windows
  directory (DS:01D8, GetPrivateProfileInt through seg5:22C0) into the status
  bytes, defaults P0 = 1 (human), P1 = 2 (computer), P2–P7 = 0, shows the
  `PLAYER` window and writes the keys back (`P%d`, DS:1DE2). Drivers have fixed
  names per slot (texts 120–127 of the language resource).
- **Without humans** seg2:5C5A is true when no slot has status 1. A new game
  sets DS:16 = 0 (seg4:0DC8); Escape or a right click while no human plays
  (seg1:0211, DS:14 ≠ 0) calls seg2:5CAD (text 0x10, DS:16 = 1), and the next
  nextPlayer ends the game (seg2:0869 → WM_COMMAND 0x424). A game of computers
  therefore runs until it is stopped.

Rebuilt in `src/game_risiko.cpp` (Risiko as phase `Risiko` with the commands
`risikoTick`, `risikoPress`, `risikoStop`) and the `Game` constructor (computer
opponent for a single player; `soloOpponent = false` is a variant for tests);
round 20 in the slot constructor of `Game`, `humansLeft` and `requestDemoEnd`.
The driver names are read at run time from `dat/<lang>/rally_<lang>.dat`
(`src/ne_strings.cpp`, RT_STRING blocks of 16 length-prefixed Windows-1252 texts;
texts 120–127). Four blocks of the German file (ids 0x8020, 0x8040, 0x82B2,
0x8A4E) have a last text whose length byte runs past the block size given in the
resource table; the reader clips it at the block end, as a loaded resource ends
there.
Assumption: one pause unit of seg6:0CDF is 10 ms for the presentation timing.

### Game window (round 21)

- **Canvas size** seg5:1227: widths {640, 800, 1024} with heights {480, 600,
  768}; a command-line index selects one, otherwise 1024 × 768, reduced until it
  fits the screen (GETDEVICECAPS 8/10). The popup window `Rally` (seg5:1C06)
  covers the screen; the child `Canvas` (seg8:0000) is centred and paints
  `%s\%d\bk.bmp` (DS:17E2, the butt directory) with the art directory of
  seg5:1827: 1 for 1024, 8 for 800, 6 for 640 wide.
- **Map window** `Map` (seg6:1680): margin m = 16 (32 from 1024 wide); top-left
  (m, m) = DS:9C84/9C86, bottom DS:9C8A = (height − m) & ~15, right DS:9C88 =
  square. The drawn area DS:94DA..94E0 is (0, 0)..(size − 16): 432 × 432 at
  640, 544 at 800, 688 at 1024. The window first clears its WinG bitmap
  (PATBLT BLACKNESS, seg6:171E); tiles are 16 pixels at every resolution.
- **Element table** seg6:1140(index, &a, &b), ten pairs per resolution:

  | Index | User | 640 | 800 | 1024 |
  | --- | --- | --- | --- | --- |
  | 0, 1 | Tourism window (seg6:47DE / 4850) | (7, 5), (480, 100) | (30, 20), (600, 200) | (150, 50), (800, 250) |
  | 2 | portrait position (seg2:2DAD) | (510, 16) | (635, 16) | (815, 32) |
  | 3 | player info size, packed (seg5:60C6) | 138 × 135 | 176 × 160 | 220 × 200 |
  | 4 | card window (seg3:0F2C) | (0, 140) | (0, 154) | (0, 215) |
  | 5 | INFO window (seg6:5215) | (400, 360) | (500, 400) | (600, 550) |
  | 6, 7 | not referenced | (50, 10), (10, 10) | (100, 20), (15, 15) | (200, 50), (20, 20) |
  | 8 | name input (seg8:095B) | (185, 60) | (200, 70) | (220, 85) |
  | 9 | player info position (seg5:60EA) | (481, 329) | (595, 416) | (764, 537) |

  The packed sizes equal `bmp/<n>/info.bmp`.
- **Portrait** `PORTRAIT` (seg2:2E38): `%s\%d\P%d\%cp` + `d.bmp` while the
  slot carries Dr. Drago (DS:9CFD ≠ 0xFF and DS:9CFC = slot, seg2:2FCC) or
  `.bmp`; %d after P is the status byte (P1 coloured humans, P2 metallic
  computers), the letter comes from seg2:2D43: slots 0–7 → D F E M U J I V. The
  window is moved to the bitmap size + 16 (seg2:2EEC). File names are matched
  without regard to case (the disc stores `p1/dp.bmp`).
- **Window classes** (registered at seg5:0448–0D30, procedure in brackets):
  RALLY (seg4:0000), Map (seg6:1680), Canvas (seg8:0000), Tourism (seg6:4F5E),
  MENU (seg5:52E7), PORTRAIT (seg2:2E38), SLOT (seg2:6855), PLINFO (seg5:61C5),
  NICKNAME (seg2:3852), INFO (seg6:5492), PLAYER (seg1:0484), OPTION
  (seg1:AD73), INTRO (seg7:20E8), ESTATES (seg1:5334), SELPLAYER (seg2:562C),
  OUTLINE (seg1:A95E), ZIEL (seg7:4CDC), TITEL (seg8:048D), STAT (seg1:97F8),
  FLIC (seg7:63E1), MCTRL (seg4:103B), SUBTITLE (seg6:661F), JOY (seg2:8D81),
  BILANZ (seg8:992E), PICT (seg6:0B66).

Rebuilt in `src/layout.cpp` (SDL-free geometry) and `src/game_screen.cpp`
(`dtvr-recomp --game`): canvas background, map window with the black margin,
portrait and the player info frame. Assumptions: the viewport is centred on the
current player (the scrolling code is not traced yet); players are marked with
provisional squares until the vehicle sprites are placed. Not yet: the contents
of PLINFO (texts), menus (MENU / SBBUTTON), slot machine (SLOT), messages
(SUBTITLE / INFO), the dialogs and the map control (MCTRL).

### Player info panel and text (round 22)

- **Fonts** seg6:04F9(height, escapement, weight, italic) → CreateFont(−height,
  0, escapement, 0, weight, italic, 0, 0, 0, 0, 0, 0, 0x20, "Arial" (DS:1643)).
  PLINFO (seg5:6304) creates Arial weight 600 italic and upright with heights
  {24, 19, 16} and {18, 15, 12} for 1024 / 800 / 640 (seg6:12FA returns 0, 1, 2).
- **Text output** seg2:314A(dc, text, length, rect, format, shadow, colour):
  transparent background; with shadow n > 0 the text is first drawn in
  PALETTEINDEX(0) with the rectangle moved by −n, then in PALETTEINDEX(colour).
  Colours used: 0xFF white, 0x45 dark brown (67, 27, 0) in the game palette.
- **Slots** seg5:5F88(rect, &out, slot): table DS:3936 of (left, top, right,
  bottom) per resolution and slot, 255 = client edge. Contents (seg5:61C5):

  | Slot | Content | Font, colour, format |
  | --- | --- | --- |
  | 0 | text 301 + month % 12, `"%Fs (%d)"` with year + 1 | italic, 0xFF, shadow 2, centred (0x121) |
  | 1 | cash, seg6:137F: `"$%li"`, '-' before '$', '.' every three digits | italic, 0xFF, shadow 2, centred |
  | 2 | destination town, seg2:271C(DS:A10D), OemToAnsi | upright, 0x45 (0x100) |
  | 3 | two SBS frames `zahl_N`: DS:22 at the player's field, 99 unreachable (seg5:6AB2) | |
  | 4 | text 201 ("Züge") | upright, 0x45 |
  | 5 | `zahl_N` of DS:A778 − DS:9D40 (9 9 for −1, 0 0 for 0) (seg5:697F) | |
  | 6 | DS:B632, set on arrival (seg2:5ABC): the town name for kinds 4 and 6, else text 202 + kind; 19 characters | upright, 0x45 (0x120) |
  | 7 | SBS frame by the field kind: 1 BLAU, 2 ROT, 3 GELB, 5 LILA | |

  The units digit is drawn at x + the width of its own frame. The frames are in
  `sbs/<n>_assort.sbs` with upper-case names (`ZAHL_0`); seg6:406D therefore
  compares names without regard to case.
- **Keys of the main window** seg4:0259 (virtual keys): Escape / Backspace open the
  menu RIGHTBTN (seg4:01D1 → seg5:5214, see "In-game menu"), Enter seg2:0BC3, Space centres the
  map on the current player (seg2:3216, 05A8), arrows set the steering direction
  DS:A77A (1 up, 2 right, 3 down, 4 left), Insert / Delete seg6:2E54(3), keys 1–4
  seg6:0DA0(key − 1), C seg2:7F72, Q seg4:0EDF.

Rebuilt in `src/panel.cpp` (SDL-free contents) and `src/text.cpp` (SDL2_ttf with
Arial, else Liberation Sans; bold face for weight 600). *Round 137:* DS:B632
(slots 6 / 7) is written only by the drive: seg2:20CF → seg2:5AA8 fills it each
time a vehicle reaches a field (after Plopp01 / Carb01), seg2:1F36 empties it when
the vehicle leaves its field (also SELPLAYER seg2:51EA / 562C refill it for a
moment). So the panel names every field passed, also mid-move; a player change
keeps it. The rebuild formerly showed it only in phase Arrived; now
`GameScreen::labelField_` follows the drawn vehicle.

### Hand stack (round 114)

- seg3:0EE8(mask) shows the current player's hand under the portrait: card size
  130 × 187 / 102 × 145 / 82 × 117 by seg6:12FA (seg3:0EFC ff.), n = seg2:3C00;
  the stack, card width + (n − 1) · 3 wide, is centred under the PORTRAIT window
  (DS:3E, GetWindowRect mapped to the canvas), its first card at y = element 4
  of seg6:1140 + (n − 1) · 3 (640: 140); every occupied slot in order becomes an
  SBBUTTON (seg3:0603, id 0x1388 + slot) that is disabled at once
  (EnableWindow FALSE), each next one 3 pixels right and 3 up — a display, not a
  control. Called at every turn start (seg2:047D, also for computers), after the
  card window (seg3:0EA7, seg4:0B1F / 0B63) and after card shop trades
  (seg3:48BB / 48FE / 49D6); the card window removes it (seg3:0887 → 0AFB).
  Rebuilt in `GameScreen::drawHandStack` from the current hand every frame.
- **Turn menu order** (round 114): ONFIELD opens only after the events of
  nextPlayer (seg8:1624) have shown; the slot machine and event animations are
  modal. The game window therefore waits for messages and films the game queued
  in the same frame (a computer's turn end raised a robbery after the window had
  already taken the frame's messages, and the menu opened under its animation).

### Name plate (round 113)

- seg2:3700 creates NICKNAME (procedure seg2:3852) as a child of the canvas
  without a border: GetWindowRect of the portrait window DS:3E mapped to the
  canvas, x = left + (right − left) / 2 − width / 2, y = bottom + 5, the size of
  `%s\%d\mess.bmp` (DS:1AF6 / 1B17; 145 × 28 at 640 and 800, 185 × 36 at
  1024). The paint draws mess.bmp at (0, 0) and the current player's name
  (record +1) with seg6:04F9(13 / 12 / 11 by seg6:12FA, weight 600) into the
  client rectangle with its top − 3, format 0x125 (centred, vertically centred,
  single line, no clip), shadow 1, colour 0xFF (seg2:39CF). Rebuilt in
  `GameScreen::drawPortrait`.

### Name codes (round 112)

- seg2:6332, called for every new game after the deal (seg4:0E71), sets the
  current player DS:94E2 to 7 for its duration and compares slot 7's name (record
  +1; the driver Armino Gesserti) with the C runtime's case-sensitive _fstrcmp
  (seg10:22CA): "Armin Gessert" (DS:1BB1) sets its cash +0x42 to 500,000.0
  (DS:861E; fan sites say 50,000); "130663" (DS:1BBF) sets 1,000,000.0 (DS:8626),
  then walks the eight hand slots and calls seg2:4327 for every *occupied* one,
  which takes a card (seg3:032D(1)) into the first free slot, so one starting
  card cascades into a full hand; finally the name becomes "Armin Gessert"
  (DS:1BC6). Slot 7's status is not checked. Rebuilt as `Game::applyNameCodes`
  (called in `PlayView::newGame`).

### Window border (round 112)

- The child windows have no WS_BORDER (style 0x44000000 etc.); their wooden frame
  is a non-client handler their procedures call first: seg6:1A68 (8 pixels,
  frames B1–B8 of `assorted.sbs`, names DS:16A0) and seg6:1E83 (16 pixels, BS1–BS8
  at DS:16B8, all 16 × 16). WM_NCCALCSIZE (0x83) insets the client rectangle by
  the thickness on every side (seg6:1A8E); WM_NCPAINT (0x85) gets the window DC,
  tiles B2 along the top and B7 along the bottom from x = t while x < width − t,
  B4 / B5 down the left and right from y = t while y < height − t, then the
  corners B1 (0, 0), B3 (width − w, 0), B6 (0, height − h), B8 (width − w, height −
  h), each copied opaque (seg5:4FC6, WinGBitBlt; the pieces hold no colour 0).
- 8 pixels: Map (seg6:1763), PORTRAIT (seg2:30B8), SELPLAYER (seg2:5969), JOY
  (seg2:90AE), Nägel (seg3:2730), Flugzeug (seg3:3938), INTRO (seg7:259D), ZIEL
  (seg7:5084), FLIC (seg7:669E), Risiko (seg7:7138), SIEG (seg7:839C), INFO
  (seg6:593F), SUBTITLE (seg6:6991), OUTLINE (seg1:AAD4), TEMP (seg8:0D25), the
  investment offer (seg8:7C63) and BILANZ (seg8:A7D7). 16 pixels: MENU
  (seg5:57ED), Tourism (seg6:5152) and STAT (seg1:9A0A). Without a border:
  ESTATES, the card shop, the card window, SLOT, PLINFO, PLAYER and OPTION.
- The sizes fit: the map window is 448 wide at 640 and its drawn area 432; INFO
  is picture + 0x24 high, the client picture + 20 with 10 pixels around the
  picture; MENU is (items + 1) · 32 high and 192 wide, the client items · 32 by
  160, the size of MENUN / MENUG; Risiko 0xF6 × 0x9E around its 0xE6 × 0x8E
  frames; the scene windows 0x1F0 × 0x178 around their 480 × 360 DIBs. FLIC
  windows have the film's size (DS:8D3C / 8D3E) while the paint blits the whole
  film at (0, 0) of the client (seg7:65D9), so 16 pixels are cut off right and
  below.
- Rebuilt in `src/window_border.cpp` for all of them except JOY (not rebuilt);
  STAT since round R1. OUTLINE keeps the window size for its source (DS:8C5A / 8C5C =
  DS:9C88 − 9C84 etc., seg1:A30C) and stretches it to that size at (0, 0) of the
  client (seg1:AA80), clipped to the client (round 113). Replaces the black frames assumed for the scene windows.

### Window centring (round 111)

- seg6:0000(hwnd) centres a window on its parent: GetWindowRect of both, half the
  difference, MoveWindow. WM_CREATE calls it in INFO (seg6:5852), MENU
  (seg5:5779), ESTATES (seg1:5C22), STAT (seg1:98BF), SELPLAYER (seg2:574C), the
  joystick dialog (seg2:8ED0), seg3:1E5E, seg3:3381, the card shop (seg3:4C18; its
  CreateWindow passes two never-written locals as x, y), INTRO (seg7:2294), ZIEL
  (seg7:4DE6, finish and destination search share the class), seg7:60C4, FLIC
  (seg7:6477; created at (10, 10)), seg7:6BDF, seg7:7DBC, TEMP (seg8:09E7),
  seg8:7095, BILANZ (seg8:9A6D), seg8:B301, PICT (seg6:0B8E) and Tourism
  (seg6:4F86). SLOT does not. With subtitles on, seg6:160B(hwnd, 0, −22) lifts the
  intro (seg7:047E), the destination search (seg7:3AD9), the victory (seg7:73D6)
  and the balance (seg8:8BEC). Rebuilt: the stage count window, sights and films
  are now centred, the three scenes lifted; ZIEL / SIEG / BILANZ / card shop were
  already centred, which replaces those assumptions.

### Slot machine and animations (round 23)

- **Animation definitions** (data segment, e.g. DS:2398): u16 delay in ticks,
  u8 (0), u8 frame count n, then n far pointers to frame names (SBS, looked up
  by seg6:406D). In the file image the segment word of each far pointer holds
  the NE fixup chain. **Animation records** of 0x40 bytes (table DS:30,
  allocated at seg2:6D2F): +0 definition, +4 follow-up definition, +8 tick
  countdown, +0xA repetitions, +0xC frame index, +0xE callback, +0x12 argument,
  +0x14/+0x16 position, +0x1A target DIB. seg2:6DCB advances all records once
  per call: when the countdown expires the next frame follows; after the last
  frame the repetitions count down, then the follow-up definition or callback.
- **Slot window** seg2:6426(reels, maximum, …): loads `bmp\slot%i.bmp` (DS:2E80)
  and `sbs\slot%i.sbs` (DS:2E8E; only slot3–5 exist, the common frames come from
  `slot0.sbs`, loaded at start, DS:02B1), creates `SLOT` as a child of the map
  window (DS:A6F0) at x = client right / 2 − bitmap width / 2, y = client top −
  bitmap height (seg2:6528; *corrected in round 111*, the earlier reading took
  the RECT's top for its bottom), i.e. above the map. seg2:6717(&hwnd, 0x100)
  then slides it down 0x100 >> 4 = 16 times by 16 pixels, each step followed by
  seg6:0CDF(20) (wait until 20 ms of TimerCount have passed since its last call;
  5 ms with DS:16); with the 265 pixel bitmaps it ends at y = −9. At the end
  seg2:6B1D → seg2:67B8 slides it up the same way before DestroyWindow. *Round 115:*
  the slot procedure keeps a state DS:A780 that WM_KEYDOWN Enter / Space advance
  (seg2:6B4A; Escape / Backspace go to seg2:69AD): 1 pulls the lever, 2 (reels
  turning) ends the reels' runs at once (seg2:6AA2, state 3), 4 (set after the
  count, seg2:7EAC) slides the window out and destroys it (seg2:6B08). For a
  computer the wait loop seg2:65C4 posts the key itself after more than 0x1E
  passes in state 4 (seg2:664E); for a human nothing does — the window stays
  until Enter or Space. Rebuilt in `SlotView::key`.
- **Tick length** (round 117): one animation tick of the slot machine is one
  pass of its loop seg2:65C4: seg6:03F7 (messages), seg2:74E4 (draw), seg2:6DCB
  (advance the records once), seg2:6F69 (blit), seg2:1C37 (idle pass). The idle
  pass runs while DS:94C4 (a game) and DS:94E6 (the window is active) are set
  and ends with seg6:0CDF(20), which waits until 20 ms of TOOLHELP.TimerCount
  have passed since its previous call (5 ms with DS:16). seg2:6DCB has no other
  caller than such loops (seg2:65F4, seg7 / seg8 scenes); the 50 ms timer of
  seg5:01EB calls seg5:3BB9. A tick is therefore 20 ms (longer only when the
  drawing takes more), like the event scenes' passes (seg8:6EB3); the earlier
  50 ms assumption made reels, lever and the computer's wait 2.5 times too slow.
  *Round 116:* state 1 waits for the lever: a computer's loop posts the key at
  once (seg2:65B2), a human presses Enter or Space; seg2:6A09 then plays the
  lever record at (0x16, 0x70) — DS:2292 `slthad01`–`09`, follow-up DS:2338
  `slthad10`–`19` (slthad12 five times), delay 2 each — with sound 2 on channel 1
  (seg2:6A97). At the end of a definition seg2:6DCB first calls the record's
  callback, then switches to the follow-up and clears both (seg2:6EBA–6F3B), so
  seg2:7671 sets the reels turning when DS:2292 ends, while DS:2338 swings the
  lever back. In state 2 a
  key is ignored while DS:2292 still plays (seg2:6AA2). Until then the reel
  records are empty (seg2:6D1B), so only the housing bitmap with its painted
  reels shows. The procedure never asks who plays, so a key also shortens a
  computer's spin and closes its window early. The paint (seg2:6C2B) only blits
  the slot DIB; palette entries 10–153 come from the slot bitmap (seg2:6C63).
  Computers pull the lever by posting 0x7EF.
- **Reels** seg2:776A: value random(max) + 1 in DS:9584[i]; record 3 + i spins
  with DS:2398 (`drehung1`–`4`, two ticks each) for (i + 3) · 2 turns, then
  ends with DS:2E3C[value] (`walze_<v>h`, `walze_<v>`, four ticks each).
  Positions by number of reels (seg2:7674 tables):

  | Reels | Reel frames (x, y) | Sum tens / units (seg2:7C1E) |
  | --- | --- | --- |
  | 1 | (150, 146) | (144, 210), (166, 210) |
  | 2 | (108, 204), (240, 204) | (168, 204), (190, 204) |
  | 3 | (122, 178), (198, 162), (274, 178) | (192, 208), (214, 208) |
  | 4 | (166, 212), (200, 160), (252, 160), (286, 212) | (220, 212), (242, 212) |
  | 5 | (174, 154), (222, 118), (270, 154), (246, 204), (196, 204) | (216, 164), (238, 164) |

  The stop callback seg2:7C0A shows the sum of the reels stopped so far as
  `count_<d>` (definitions DS:2486 + 16·d, table DS:2E58). seg2:7EC2 finally sets
  DS:A778 to the sum. Reels 3–5 add gear animations (`slt5anl*`, `slt5anr*`,
  `slt5anu*` at (122, 82), (256, 56), …).
- **Timers**: a 50 ms timer (seg5:01EB → seg5:02CF / 3BB9, the water's colour
  cycle); others run every 1000 ms (seg2:3AF0) and 80 ms (seg2:8018). The slot
  loop (seg2:65C4) advances the animations once per pass, and each pass waits
  in seg2:1C37 for 20 ms (see "Tick length").

Rebuilt in `src/slot_view.cpp`: housing, spinning and stopping reels and the sum
display, started for every spin of the game (`Game::lastReels`).

**Slot gears and record timing (round 128):**

- seg2:6DCB advances a record: `old = +8--; if (old > 0) skip; +8 = def.delay;
  frame++` (`jg`), so a frame stays **delay + 1** passes; a new record (countdown
  0) advances in its first pass. *Corrects* the earlier reading of delay passes
  per frame: the lever (DS:2292, delay 2, 9 frames) ends after 25 passes (was 18),
  a reel turn of `drehung1–4` takes 12 passes (was 8).
  *Round 145:* the full-screen scenes (finish, victory, stage announcement, year
  balance; `src/scene.cpp`) still showed the start frame first and each frame
  `delay` passes. They now follow this rule too (`sceneFrameIndex`: frame first + 1
  in pass 0, delay + 1 passes each; `sceneLength`: (frames − 1 − first) · (delay + 1)
  passes until the follow-up; tested against a direct simulation). The stage
  announcement starts the search when the lever record DS:4A30 is on frame 9 with
  countdown 1 (seg7:3CE4 / 3CF1), at the start of pass 35, not 27.
- After the last frame: repetitions +0xA > 0 count down and the definition starts
  again; at 0 the callback +0xE runs, then the follow-up +4 replaces the definition
  or the record ends (def = 0). Records advance in index order: 0 lever, 2 sum,
  3 + i reel i, 8–10 gears; the lever's callback seg2:7671 creates the reels and
  gears, which advance in the same call.
- Reel i: DS:2398 with (i + 3) · 2 repetitions (seg2:77E1), follow-up DS:2E3C[v]
  (`walze_<v>h`, `walze_<v>`, delay 4), callback seg2:7C0A(i). The callback runs
  when the turning ends (start + 9 + 12 · (i + 3) · 2 passes), before the faces.
- Gears (seg2:78AB, after the reels' sound 5 on channel 3): five reels
  `slt5anl3–1` (DS:20B2) at (0x7A, 0x52), `slt5anr1–6` (DS:20F8) at (0x100, 0x38),
  `slt5anu1–6` (DS:214A) at (0x62, 0xC0); four reels `slt4anm1–3` (DS:2182) at
  (0x8E, 0x6A) — delay written as 4, 100 repetitions; three reels
  `slt3anm1–8, 8, 4–1` (DS:2208) at (0x62, 0x82), delay 5, no repetition (plays
  once). Frames from `sbs/slot<n>.sbs`.
- seg2:7C0A(i) adds the reel to the sum (record 2), raises the gears' delays by one
  (five reels: DS:20B2 / 20F8 / 214A at seg2:7E3A; four: DS:2182), and for the last
  reel (n − i − 1 = 0) stops channel 3 (with Wave), ends records 8–10, sets state 4
  and calls seg2:7EC2.
- seg2:6AA2 (a key in state 2, unless record 0 still plays DS:2292) sets the five
  reels' repetitions +0xCA … +0x1CA to 0, state 3, and plays sound 2 on channel 1:
  every reel ends its current turn, it does not stop at once (*corrects* the
  earlier immediate faces).
- *Round 141, corrects "ended gears vanish" and replaces the assumption about ended
  lever and reel records:* the loop calls seg2:74E4 with null bitmaps (six zero
  words pushed at seg2:65D0–65DA), so its test at seg2:7536 skips every record and
  nothing is restored. All slot records have +0xD = 0 (lever seg2:6A58, reels
  seg2:7823, gears seg2:792D …, sum digits seg2:7D2F / 7DDF), so seg6:41A2 copies
  each frame **opaque** with seg2:913A (index 0 included; `count_<d>` has 58 such
  pixels) into the WinG surface DS:94F6, not keyed with seg2:9497. seg2:6F69 draws
  only records with a definition (seg2:6FE4) whose frame name differs from the one
  drawn last (+0x36, lstrcmp at seg2:7021) or that intersect such a record
  (seg2:71F2); seg2:72DE then blits the dirty rectangles. A record ended by
  seg2:6DCB (def = 0 at seg2:6F42) or by seg2:7E7C (gears) is skipped, so its last
  frame stays: the lever `slthad19`, each reel `walze_<v>`, the sum digits and the
  gears' last frames (three reels `slt3anm1`). Only WM_PAINT (seg2:6C2B) copies the
  housing DS:94F2 over the whole surface; it needs an exposure — the map window is
  created with WS_CLIPCHILDREN (style 0x4600xxxx at seg5:1CB1), and the slide out
  moves the window with SetWindowPos (seg2:682F) and repaints only the map
  (UpdateWindow(DS:A6F0), seg2:6838). Not modelled: an exposure by another window
  during a spin (it would clear all frames until each changes again). No two record
  rectangles overlap for one to five reels (positions above, frame sizes of
  `sbs/slot0.sbs`, `slot3–5.sbs`), so the drawing order does not matter.

Rebuilt in `src/slot_records.{hpp,cpp}` (no SDL, `tests/slot_records_test.cpp`), played
from the lever pull by `SlotView`.

**Slot input (round 129):** the window procedure seg2:6855 (dispatch seg2:6BB1) takes
the mouse with SetCapture at creation (seg2:68CE) and has no hit test on the lever:
WM_LBUTTONDOWN sends itself WM_KEYDOWN VK_RETURN (seg2:695C), WM_RBUTTONDOWN and
WM_RBUTTONDBLCLK send VK_BACK (seg2:6970); WM_LBUTTONDBLCLK is not handled. Keys
Space / Return go to the state switch seg2:6A03, Escape / Backspace to seg2:69AD:
while DS:A780 ≤ 2 (lever not pulled, or reels turning without a stop key), DS:9AD8
is set and the current player is human (seg2:5C02 = 0), it plays sound 7 on
channel 1, stops channel 3 (seg1:317D), destroys the window and calls seg2:068F
(the turn menu). DS:9AD8 is seg2:6426's third argument: 1 only for Würfeln
(seg4:091F, seg2:6426(1, 5, 1)); 0 for the reel cards (seg4:0A2D–0A82) and Risiko
(seg7:6776). The reel values are drawn only when the lever ends (seg2:7671 → 776A),
so the cancel before the lever leaves the random generator untouched and the one
after it keeps that draw. Rebuilt as `Game::cancelRoll` (restores the state before
the roll through the save codec, `tests/game_test.cpp`) and the slot block of
`GameWindow::handleEvent`.

**Move after the window (round 131):** seg2:6426 loops (seg2:65C4) until DS:A780
is 0, i.e. until the window has slid out or was cancelled. Only then: seg7:672A;
a human with sum DS:A778 = 0 (cancelled; the sum is set at the count) returns
without a move (seg2:66A6); otherwise seg2:66B9 → seg2:61B7 starts the move
(reach, shut-in notices 6003 / 6005 / 6009, turn end) and seg2:0A2F follows. The
reel cards (seg4:0A2D ff.) and the status cards' spin go the same way. Rebuilt:
with a window (`Game::setSpinPresentation`, set by the game window) `roll()` and the
reel / status card spins leave the move pending (`spinPending`, not saved) until
`finishSpin()`, which the window calls when the slot window of that spin has closed;
cards, rolls and computer stages wait meanwhile. Without a window (tests, headless
play) the move starts at once, with the same outcome. A Würfeln can now always be
taken back before the window closes (*replaces* the round 129 restriction for
shut-in rolls).

**Edge scrolling with the drawn frame (round 131):** seg2:1A91 writes the drawn
frame's rectangle (x, y, x + width, y + height, after the −16 shift of seg2:3F21,
with the boat / plane lift of seg2:1878) into player +0x30…+0x36; seg2:29EB compares
that rectangle with the view inflated by −32. A rising plane therefore scrolls the
view before its frame reaches the edge. *Corrects* the check with the field cell
(16 × 16 at the ground position), which let a plane be cut off at the map's top.

### Vehicles on the map (round 24)

- **Animations by facing** table DS:1994 [facing · 4 + mode] (seg2:2AF6): far
  pointers to definitions whose second byte is the facing (1 up, 2 right, 3 down,
  4 left) with four frames at a delay of 4 ticks. `?` in the names is replaced by
  the driver letter of seg2:2D43: mode 1 and 2 use `?up1–4`, `?r1–4`, `?down1–4`,
  `?l1–4`, mode 0 `?fup…`, `?fr…`, `?fdown…`, `?fl…` (its trigger is not traced).
  Frames are 48 × 32 in `sbs/<letter>.sbs`.
- **Position** seg2:04AD(player, field): +0x16 = field x · 16, +0x18 = field
  y · 16, +0x3C / +0x3E = field; all other writers keep multiples of 16 at rest and
  drive decisions happen when (x & 15) = 0. seg2:1846 enters the vehicle into the
  sprite list (45 entries of 0x1A bytes, seg2:3D9C: +4 x, +6 y) at x = +0x16,
  y = +0x18 − +0x2E + 2; seg2:1A91 draws a frame with its top-left corner at that
  point minus the map origin (DS:956A / 9568 · 16), and the map tiles use the same
  mapping (seg6:2857, seg6:2C0A). *Resolved in round 84:* seg2:3EB0, which draws
  the list, first subtracts 0x10 from every entry's x, y, right and bottom
  (seg2:3F21–3F45), so a car's frame starts at the field cell · 16 + (−16, −14):
  vertical frames (opaque columns 16–31) cover the cell's column, horizontal ones
  (opaque rows 7–25) its row. +0x2E is not a height but the boat counter of the
  vehicle state +0x2D (0 on land, up to 0x18 at sea; round 75): a boat is drawn
  up to 24 pixels higher. Event animation records go through the same list
  (seg8:6E0F → seg2:3D9C) and are moved by (−16, −16) as well.

Rebuilt in `src/game_screen.cpp`: the vehicles of all drivers in play with the
facing of their last step.
**Frame animation (round 124, corrects "only while driving"):** the idle pass
seg2:1C37 (while DS:94C4 and DS:94E6 are set) calls seg2:4F7D → seg2:16C1 for every
vehicle in play with an animation (+0x20 ≠ 0): it draws frame +0x2C, then
`old = +0x1E--; if (old < 0) { +0x1E = def[0] (4); +0x2C = (+0x2C + 1) % def[3] (4) }`
(seg2:1941–19BD). +0x20 is set at the start (seg2:5142: mode 2, mode 0 at sea),
when a step starts (seg2:1EC0 / 2324: mode 1, mode 0 on sea routes) and at the
arrival (seg2:2101: mode 2). DS:1994 mode 2 (definitions DS:2006–2082) has the same
four frames `?up1–4` … as mode 1, so cars also animate while standing. seg2:2B30
resets +0x2C and +0x1E to 0 only when the pointer changes. Rebuilt in
`src/vehicle_anim.{hpp,cpp}` (tested against a direct simulation of the loop).
**Smooth driving (round 31):** the drawn position follows the game position at
4 pixels per idle pass of 20 ms (velocity 2 from DS:19EA / 19F4 shifted by DS:34,
seg2:1CB0; round 117: DS:34 is loaded from DS:19FE = 1 at seg2:202D, and its only
setter seg2:35FD has no caller in the relocation tables, so the speed never
changes); a held arrow key drives on (DS:A77A stays set while the key is
down, seg4:01F2), the next field step is taken when the car has reached its
field. Only steps along a link are driven; other position changes (start,
Flugzeug, Konferenz) are shown directly. The view origin follows the drawn
position in whole tiles.
**Computer turns on the map (round 32):** `Game::computerAdvance` runs a
computer turn in stages (card or spin with the route of states 5 / 6, one field
per call, then the stop's actions of states 12 / 13 and the end of the turn);
`runComputerTurn` runs the same stages at once, so both use the same random
draws in the same order (whole computer games compared before and after the
split: identical). The game window takes the next stage when the car has
reached its field. The stage state (remaining route) is not saved; the save
format and rules revision are unchanged.
The former assumption (−16, −8) is replaced by the traced offset (−16, −14) of
round 84.

### Messages (round 25)

- **Message window** seg6:51EA (about 70 callers) creates `INFO` (procedure
  seg6:5492) with the paper `%s\%d\p1.bmp` (DS:3ABC, butt directory) and the
  showmaster `%s\%d\sm.bmp` (DS:3AC9, bmp directory). Size: width a of element 5
  of seg6:1140 (400 / 500 / 600), height = showmaster height + 0x24; it is
  centred on the canvas (seg6:5852 → seg6:0000) and modal (seg6:0FB4). DS:8C36
  selects the buttons (0 none, 1 Yes / No with texts 114 ff., result DS:8C38);
  a click or key closes it.
- **Text position** (round 113): seg6:5B59 inflates the client rectangle by −10
  and adds the picture width + 10 to its left edge (seg6:5B6F), so the text
  starts 10 pixels right of the picture (it was drawn at the picture's edge).
- **Waiting** (round 113): the modal loop seg6:0FB4 closes the window by itself
  only while the current player is a computer (seg2:5C02): after 2500 ms
  (DS:85AE) times the pause factor (seg6:0E3A, keys 1–4), twice that while the
  window DS:A7BE exists, 100 ms once a demo ends (DS:16), never while a FLIC
  window is open (DS:A050); it posts 0x7EF to the window (seg6:10EE). For a
  human it waits for a key or click. Replaces the assumed 2 s without humans. The wait
  starts when the window shows, after an event's animation (round 116).
- **Paint** seg6:59C6: the paper fills the client area, the showmaster at
  (10, 10); the text goes into the client rectangle inflated by −10 with its left
  edge moved past the showmaster, in Arial weight 600 at {18, 16, 14}
  (1024 / 800 / 640, buttons + 5), palette colour 0x45, DT_WORDBREAK | DT_NOCLIP.
- **Texts** are language texts chosen as base + random(n) and filled with
  wsprintf (`%Fs` for driver names, amounts of seg6:137F and town names). Bases
  found after the random draws (`add ax, base`): 961 solo opponent (seg2:021F),
  4201 yellow field (random(13)), 5001 East opening (seg8:6C13), 5021 robbers
  (amount), 5041 storm, 5141 drought, 5241 flood (town), 5081 rockfall, 5101
  Nessie (town), 5121 rain, 5201 earthquake (town), 5281 Etna / 5301 Hekla (by
  DS:69F0), 5321 professor, 5401 football club, 5481 snow; all with random(2)
  except 4201. **Correction:** the East opening draws random(2) for its text
  (seg8:6C13); the rebuild had skipped it, so random sequences after the
  opening differed until round 25 (save rule revision 25).

Rebuilt in `Game::takeNotices` (text number and arguments from the simulation),
`formatNotice` (src/panel.cpp) and `src/message_view.cpp` (window, paper,
showmaster, word-wrapped text). *Round 138:* all 73 callers of seg6:51EA pass
buttons 0 except the obstacle question and the quit question (4); their pictures
are the showmaster (an empty name) or a path built in DS:82 (Dr. Drago, card,
professor, club), all rebuilt (`Notice::picture`, `cardPicture`).

### More messages (round 26)

- **Text helper** seg2:B74D(buffer, size, base, count) = base + random(count − 1).
  Dr. Drago (seg2): spending 3151 (20 texts, amount, seg2:BAD4), dear purchase
  3101 (3, town and price, seg2:BC41), giving a property away 3001 (11, town,
  seg2:BD42), tearing a card 3051 (8, seg2:BE70), giving money 3201 (5, amount
  and recipient, seg2:BEE6), breakdown 3251 (5, seg2:C05F, at the turn change).
  *Round 76:* these Drago messages pass the picture `%s\%d\qp.bmp` (bmp
  directory, art directory; DS:3DEE, 3DFB, 3E08, 3E15, 3E22, 3E30 at seg2:BAB5,
  BBB8, BCE1, BE51, BEC7, C040) to seg6:51EA, so Dr. Drago's portrait replaces the
  showmaster. The block action seg2:BFB0 sets bit 0 and shows text 3351 (0xD17,
  no draw) with the empty picture DS:3E2F, i.e. the showmaster; the rebuild had
  no message for it. Rebuilt with `Notice::picture` and `bmp/<n>/qp.bmp` in
  `src/message_view.cpp`.
- **Other message pictures** (round 77, all seg6:51EA calls preceded by a BMP
  path): the investment results show `%s\prof.bmp` (DS:71A5 / 71B1, seg8:65C0 /
  66AC) or `%s\fuss.bmp` (DS:718D / 7199, seg8:6428 / 6514) from the butt
  directory; the Christmas card gift (seg3:126F, text 13006 ff.) shows the given
  card's picture `%s\%d\%c\%dcard_%.2d.bmp`; the obstacle question uses the
  clearing card (round 65). The `BB.bmp` calls at seg3:19FD / 1AD1 / 1B22 go to
  seg2:51EA (target choice), not to the message window.
  Weihnachten (seg3): 13001 not December (seg3:1189), 13006 card (seg3:1223),
  13011 money (seg3:1292), three texts each.
- **Turn start** of the next player: Zucker clears bit 1 and shows 1000 +
  random(2) (seg2:090A), Schlafen clears bit 2 and shows 1010 + random(2)
  (seg2:09A8). Opening the card window (seg4:0930) with blocked cards shows
  1020 + random(2) (seg2:0B08). A full hand shows text 403 (no random number).
- **Investments**: the offer window (procedure from seg8:7098) draws its text on
  creation, 5321 + random(2) (professor) or 5401 + random(2) (football club),
  i.e. only when the offer appears (cash > 10,000). Results: professor 5341 win
  (amount) / 5381 loss, football club 5421 win / 5441 loss.
  *Round 56:* window TEMP 0x1BE × 0x8A (CreateWindow seg8:7027) on
  `%s\unipaper.bmp` (butt) with `%s\prof.bmp` or `%s\fuss.bmp` (102 × 102);
  click rectangles Ja (0x82, 0x5A)–(0xA0, 0x6E) and Nein (0x154, 0x5A)–(0x186,
  0x6E) set DS:8D5A = 0 / 1 and post Return (seg8:70BB / 70FF); keys at seg8:73AB
  (Backspace / Escape decline with sound 7, Return, arrows); text Arial 16 weight
  600 colour 0x45, Ja / Nein texts 115 / 116; after Ja text 5390 and 5391 with
  the amount in steps of 1,000 (DS:877E). Rebuilt in `src/invest_view.cpp`;
  **assumptions:** picture at (10, 10), text right of it, arrows left / right
  choose Ja / Nein and up / down change the amount (the exact key mapping of
  seg8:73AB is not traced), the window centred; computers keep declining.
- **Corrections:** the rebuild did not draw the random numbers of the blocked
  card window and of the investment offer before round 26.

Rebuilt in the game (notices with arguments) and shown by the message window.

**Card texts (round 27):** the handlers load text 11001 + card type (DS:9C71,
e.g. seg3:1524; no random number): Fiskus 11010 (victim, amount, player),
Konto 11011 (new balance), Schulden 11012 (victim, player, amount, player),
Zero 11013 (player), Konferenz 11022, "n Felder" 11030–11035 (player). The
Erdbeben card uses the event texts 5201 + random(2) with the town (seg3:1C00).
Nägel shows 5500 when the nails fall into water and Flugzeug 8300 for the fog
over Brussels; both are rebuilt (`Game::nailsInWater`, Platsch on channel 5;
`FlightView` row 1 before the last stage). Cards without a text of their
own: the property cards, status cards, Walzen, Risiko, Tschüss, Punch.
### In-game menu (round 28)

- **Resources:** RT_MENU in `dat/<lang>/rally_<lang>.dat`, named MAIN (title),
  MAIN1 (1065 Weiter spielen, 1060 Neues Spiel, 1062 Spiel speichern, 1066
  Mitspieler, 1063 Optionen, 1064 Rallye beenden), ONFIELD (1030 Würfeln, 1031
  Karte einsetzen, 1032 Zoom, 1034 Übersicht, 1035 Umfeld, 1036 Statistik, 1033
  Hauptmenü), ONTOWN (1050–1053, grayed at first), RIGHTBTN (1040 Zoom, 1041
  Übersicht, 1043 Umfeld, 1042 Statistik, 1044 Zurück), TOWNINFO. Items of the
  first popup are used (GetSubMenu(menu, 0)). Checked items (MF_CHECKED 0x08):
  MAIN Neues Spiel, MAIN1 Weiter spielen, RIGHTBTN Zurück, TOWNINFO Beenden.
- **Window** seg5:5214(&handle, name, x, y, ?): one menu at a time (DS:9EDE; an
  open one is destroyed first, seg5:52AC). CreateWindow class MENU, child of the
  canvas DS:40, 192 × 48; WM_CREATE (seg5:5699) loads the menu, sets the props
  MENU / MENUSUB / FOCUS = 0 and resizes to 192 × (items + 1) · 32 (seg5:5733);
  x = 0 centres it on the canvas (seg6:0000). Callers pass (0, 0) except TOWNINFO
  (element 1). Opened by: Escape / Backspace RIGHTBTN (seg4:01D1), the turn start
  ONFIELD (seg2:0864 → seg2:068F, after the message windows are gone, seg6:03F7),
  Hauptmenü MAIN1 (seg4:09DE), the title MAIN (seg8:0DDF).
- **Paint** seg5:57F0: WinG bitmap of the client size; items from y = 0, each the
  height of its frame: SBS `MENUG` (focused item) or `MENUN` (160 × 32, in
  `sbs/assorted.sbs`) blitted at (0, y) by seg5:4FC6. Text via seg2:314A, Arial
  weight 600 height 16, no shadow, at (+18, +7), focused (+20, +9); colour 0x43
  (123, 95, 27) for grayed / disabled items, 0xA4 (255, 239, 239) focused, 0x45
  otherwise. The last 32-pixel row and the 32 pixels right of the frames are
  not painted by the items (content there unknown; not drawn).
- **Input** seg5:52E7: mouse over an item sets FOCUS, a click sends WM_COMMAND
  (id) to the main window DS:42; keys (seg5:5657): Escape / Backspace the checked
  item (none: seg5:5C08), Tab / Down next (wrapping), Up previous, Enter the
  focused item (grayed / disabled: only a sound, seg1:2C9B(2, 7, 0)), `1`–`4` are
  passed to the main window.
- **Commands** seg4:0B76 (jump tables seg4:0BE4, 0C21): every entry first closes
  the menu. 1030 Würfeln seg2:6426(1, 5, 1); 1031 Karte einsetzen: blocked cards
  → text 1020 and the menu stays, else seg3:0887(2), seg3:10D7(0) (card window);
  1032 / 1040 Zoom seg6:2E54(3) and centring on the player; 1034 / 1041
  Übersicht seg1:A240; 1035 / 1043 Umfeld seg6:3C94; 1036 / 1042 Statistik
  seg1:4DE5 (ONFIELD entries reopen ONFIELD afterwards); 1033 Hauptmenü →
  seg8:0362, MAIN1; 1044 Zurück only closes; 1064 Rallye beenden seg4:0EDF,
  MAIN1 again when that returns 0; 1065 Weiter spielen seg4:0753; 1066
  Mitspieler seg4:0803.
  *Round 86:* 1060 Neues Spiel (seg4:075B) clears DS:94C4 / 96A0 / 14, stops the
  timer DS:A7C8, closes the game windows and returns to the title (seg8:03AB);
  1065 Weiter spielen calls seg2:068F (ONFIELD again); 1066 Mitspieler (seg4:0804)
  opens SELPLAYER with all drivers (seg2:51EA(0xFF)) and passes the choice to
  seg2:5E93, which toggles its status between 1 (human) and 2 (computer) and, for
  the current driver, redraws the portrait (seg2:2D8D) and restarts the computer
  state machine (seg5:6CE8, 6BF2). Rebuilt in `src/game_window.cpp`; a turn handed
  over after the spin is finished by the computer from its move or town plan
  (`Game::computerAdvance`; **assumption:** the restart within a turn in progress
  is not traced, the route over the remaining fields draws its numbers as a normal
  route plan).

Rebuilt in `src/ne_strings.cpp` (readNeMenu) and `src/menu_view.cpp`; flow in
`src/game_screen.cpp`. Simplifications: ONFIELD opens whenever a human player is
in the spin phase and no window is open (also after RIGHTBTN Zurück, which in the
original only closes); "Karte einsetzen" opens the card window (round 29);
Rallye beenden asks first since round 66 (see below);
Zoom, Übersicht, Umfeld, Statistik and MAIN1 entries besides 1064 / 1065 do
nothing yet; the keys `1`–`4` are not passed on while a menu is open.

### Card window (round 29)

- **Opening** seg3:0887(category) after "Karte einsetzen": DS:18 = category
  (2 before the spin), hand count seg2:3C00 = 0 → nothing opens (seg3:0922).
- **Layout** seg3:089B: card size by seg6:12FA (1024 / 800 / 640): 130 × 187,
  102 × 145, 82 × 117; columns by the number of cards n = 1…8: 1, 2, 3, 4, 3,
  3, 4, 4, rows 1 (n ≤ 4) or 2; 10 pixels apart; the block is centred on the
  canvas. Slots 7 down to 0 (hand bytes at player record +0x24, 0xFF empty)
  fill the rows left to right; positions are rounded down to even (`& 0xFFFE`).
- **Buttons** seg3:0603(type, x, y): CreateWindow class `SBBUTTON` (DS:3140;
  registered outside rally.exe, presumably by `sbtimer.dll`), style 0x54010000
  (child, visible, clip siblings, tab stop), parent the canvas; the picture is
  `%s\%d\%c\%dcard_%.2d.bmp` (DS:3127, seg3:0488): `bmp/<n>/<language>/<n>card_<type + 1>.bmp`,
  which matches the card sizes. The handle is kept per card type (DS:9EDA, 7
  bytes per type, +5). The focus goes to the buttons in slot order 0…7.
- **Unknown:** the button behaviour of SBBUTTON (focus look, keys, how the window
  closes without playing). *Round 59:* seg3:0701 sends 0x7ED with wParam 0 to
  the button of a card whose category matches DS:18 and wParam 1 otherwise. The
  class is registered by rally.exe itself.
- **SBBUTTON** (round 60): seg1:0D7E superclasses the system class BUTTON
  (GetClassInfo, DS:32B3; old procedure kept at DS:A0FA) under the name
  SBBUTTON (DS:32BA) with the procedure seg1:0E78. Messages: 0x7EA sets the
  picture pointer (window long 2) and repaints; 0x7ED with wParam 1 sets state
  bit 0x10 (seg1:14C1), wParam 0 clears it (seg1:1519); a key 0x20 (space) is
  ignored while bit 0x10 is set (seg1:0F9C), otherwise Plopp02 on channel 2 and
  WM_COMMAND with the control id to the parent. Paint seg1:1718: the picture at
  (0, 0), or at (1, 1) while state bit 1 (pressed) is set; with bit 0x10 the
  frame `dis_%c` (DS:32CB, language letter DS:A0F0) laid over it with colour 0
  transparent (seg2:9497) — in `sbs/<n>_assort.sbs` DIS_D / DIS_E have the card
  size and hold a black checker dither with a red "GESPERRT!" band; with bit 4
  (focus) a rectangle with CreatePen(PS_INSIDEFRAME, 5, PALETTERGB(255, 0, 0)) and
  the null brush around the client area. Rebuilt: disabled cards ignore clicks
  (masks 2 from the menu, 1 from the town dialog, all for the discard choice)
  and show DIS_D.
- **SBBUTTON input** (round 61): WM_SETFOCUS / KILLFOCUS set / clear bit 4
  (seg1:1111 / 1157). The card window calls SetFocus on the occupied slots 0…7 in
  turn (seg3:0A77), so the highest occupied slot keeps the focus. Left button
  down (seg1:1311): SetCapture, focus; a disabled button plays sound 7 on channel
  2, otherwise bits 9 (pressed, captured) are set. Button up (seg1:1398): bits 9
  cleared; inside the client rectangle the button sends itself space (0x20),
  which plays Plopp02 on channel 2 and posts WM_COMMAND. Keys (seg1:10B4):
  Backspace / Escape post WM_COMMAND 0xFFFF to the parent (cancel); Tab posts
  VK_LEFT with Shift, else VK_RIGHT (seg1:1060); Return sends space to itself
  when bit 8 of the window data +0x1B1 is set, else it goes to the parent
  (seg1:0FD7); right (seg1:2474) / left (250A) walk the siblings with
  GetNextWindow(GW_HWNDPREV / GW_HWNDNEXT) to the first one with the same top,
  down (25A0) / up (267F) to the same left, else the adjacent sibling; a found
  button with an id in 0x1388–0x1770 gets the focus and sound 8 on channel 2.
  Rebuilt in `src/card_view.cpp`; Return activates the focused card.
  *Round R8:* the walk follows the siblings' z-order (`SbButtonGroup`, entries
  topmost first): right / left take the first sibling with the same top towards the
  top / bottom of the z-order and stop at the first match; down / up the first with
  the same left, else the adjacent sibling; nothing wraps around; Tab is right,
  Shift+Tab left. Orders: the card window brings each card to the top as it is
  created from slot 7 down to 0 (BringWindowToTop seg3:0A26), so the lowest
  occupied slot is topmost; SELPLAYER creates slot 0 first (seg2:53EB), PLAYER the
  star, the stop and per slot 7…0 the SBINPUT name field and the driver
  (seg1:05C9, 0687, 071E, 0830), OPTION the star and the switches 7…0 (seg1:AECC,
  B090). **Assumption (Windows, not in rally.exe):** CreateWindow places a new
  child below its siblings, so creation order is z-order from the top (the dialog
  manager's tab order); the rows in SELPLAYER and the card window then walk in
  screen direction. The id range test is not modelled (all rebuilt entries are SB
  controls with ids in range). OPTION loads `pl_menu\stop.bmp` (seg1:AEF2) but
  creates no stop window; the rebuild shows one (not changed in R8).
- **SBBUTTON users** (round 62): CreateWindow with class SBBUTTON occurs at
  seg1:057C, 063A, 07E3 (player setup PLAYER: drivers, star, stop), seg1:AE7F,
  B043 (OPTION: star / stop and the op_menu switches), seg2:549E (SELPLAYER, ids
  0x1392 + slot) and seg3:0677 (card window). Initial focus: SELPLAYER calls
  SetFocus on each button right after creating it (seg2:5505), so the last
  candidate (the leftmost) keeps it; with a computer choosing, seg5:7190's
  slot gets the focus (seg2:5555) before the 1.5 s pause. PLAYER and OPTION call
  SetFocus(DS:A6E8) (seg1:0409, AD49). *Correction (round 97):* DS:A6E8 is entry 8
  of the button handle array DS:A6D8 (written at seg1:083B and seg1:AED7 with the
  loop index), i.e. the star in both windows (OPTION's first loop starts at index 8
  with id 0x1392 + 8 = 0x139A, seg1:AE1F; PLAYER enables it with EnableWindow,
  seg1:0853), so the star starts with the focus. A disabled star gets no mouse
  input (EnableWindow FALSE passes it to the parent). SBBUTTON passes F1–F10
  (0x70–0x79) to its parent (seg1:10FD → 1096), F10 also as WM_SYSKEYDOWN (seg1:0F4B). The shared state lives in `src/sb_button.cpp`
  (SbButtonGroup) and is used by the card window, SELPLAYER, OPTION and PLAYER;
  switches and drivers now change on release as SBBUTTON commands do.
  The setup star without players is disabled with EnableWindow, so a click on it
  does nothing (PLAYER has no WM_LBUTTONDOWN handler, round 97); Return starts
  PLAYER through seg1:0C3B; OPTION has no Return of its own (round 94). Rebuilt (`src/layout.cpp` cardWindowLayout,
  `src/card_view.cpp`): a left click plays the card through the same path as
  the digit keys (target `T`, nail direction by arrow key); a played card closes
  it. **Cancel** (round 93): Escape / Backspace on a card post WM_COMMAND 0xFFFF
  to the canvas (seg1:0F77), which passes 0x1388–0x1390 and 0xFFFF to RALLY
  (seg8:00B9); seg4:0B0A closes the window and returns to the town dialog (mask
  bit 1) or the turn menu (seg2:068F); with mask 0 (discard) it only plays sound
  7 on channel 2. The right button does nothing while the window is up: the
  canvas sends WM_RBUTTONDOWN to RALLY (seg8:018C), whose handler (seg4:0417)
  opens the menu RIGHTBTN through VK_BACK only with DS:96A0, which the card window
  clears (seg3:092D).

### Town dialog ESTATES (round 30)

- The menu ONTOWN (1050–1053) exists in the language file, but rally.exe never
  loads it; its commands are empty jumps (seg4:0A0F–0A15). The town dialog is
  the window class `ESTATES`.
- **Window** seg1:4BDD (town stop) and seg1:4E39: WinG bitmap 0x230 × 0x1CE,
  sprites `<dir>\buy.sbs` (DS:3FCC), CreateWindow child of the canvas, 0x22E ×
  0x1A4 (558 × 420); the x/y arguments are uninitialised locals ([bp-0x90],
  [bp-0x92]); the window is assumed to be placed centred on the canvas.
  Start state seg1:4CF1: scroll DS:8C4C = 0, active tab DS:3FBC = 0xFF, pressed
  tab DS:3FB8 = 0xFF, selected row DS:3FBE = 0xFF, card mode DS:3FC6 = 0xFF,
  tab mode DS:3FC2 = 0 (bit 0x10: the statistics variant with ZW_Z rows).
  DS:948C = seg1:7CDE(town): one player owns every entry.
- **Header** seg1:7275: frame KO_Z (558 × 42) at (0, 0); texts 321 Kaufen, 322
  Verkaufen, 323 Karte (LoadString) in Arial 20 weight 400 at x = 18, 160, 336,
  y = 10; colour 0x2C (255, 247, 0) for the active tab, 0x45 otherwise.
- **Rows** seg1:65A7: row i at y = (i − scroll + 1) · 42, frame GR_Z or GR_ZI for
  the selected row; owner icon (seg1:6163) at x = 0: `0n` free, otherwise
  `%c_n` with the owner's portrait letter (seg2:2D43), `%c_m` when DS:948C is
  set. Texts in Arial 18 weight 400 at y + 10: name (entry +0x00, OemToAnsi) from
  x = 0x54 + 10 = 94; price (seg6:137F) right-aligned in [294, 378]; income
  value · 0.001 (DS:86BE) · rate (entry +0x1C) · 2 if DS:948C (DS:86C6) in [374,
  468]; yield rate · 1.2 (DS:86CE) as `%.1f%%` (DS:4027) in [464, 548].
  Price column: × 1.5 (DS:869E) in the Verkaufen tab with the Verkaufen card,
  × 0.75 / 0.5 / 0.1 (DS:86A6 / AE / B6) in the Kaufen tab with Kaufen / Gold /
  Platin. Colour 0x43 for rows the tab's action does not apply to (seg1:6854:
  Kaufen without a card: free and cash ≥ price; Verkaufen: own entry, not
  rented; Mieten / Nimm: another player's entry), 0xFF selected, 0x45 otherwise.
  *Round 151, the complete rule seg1:6854..6BA6* ([bp−0x78] = 1 grey, cleared by
  one rule; DS:3FC2 = tab, DS:3FC6 = card mode, price [bp−0x76] as above): no tab
  → normal (685E); Kaufen with 0xFF (686A), 3 (6A5F), 4 (6ABB) or 5 (6B17): free
  (+1 = 0xFF) and cash +0x42 ≥ price; Kaufen with 6 (6924) or 7 (6982): owner
  neither the player nor free; Kaufen with 8 (69E0): the same and cash ≥ value;
  Kaufen with 9 (6B73): free; Verkaufen only with 0xFF (68C6): owner = player and
  landlord (+0xA of the record = entry +9) 0xFF. Every other pair stays grey: with
  any card mode the whole Verkaufen tab, with Verkaufen (2) the whole Kaufen tab.
  The colour is only drawn: the actions (seg1:5545 → 7DC5, seg1:55DB → 8327) do
  not test it, so the Verkaufen card still sells own rows at × 1.5 while they are
  grey. The grey test comes before the selection (seg1:6C2A). Rebuilt in
  `estateRows` (`src/estates.cpp`); before round 151 the Verkaufen tab ignored the
  card mode and Kaufen with mode 2 used the normal rule.
- Rebuilt (`src/estates.cpp` contents, `src/estates_view.cpp` drawing; flow in
  `src/game_screen.cpp`): the dialog opens when a human player stops in a town.
  A click on a tab selects Kaufen / Verkaufen, Karte opens the card window, a
  click (or Enter) on an enabled row buys / sells it.
  **Input** (round 93, procedure seg1:5334, dispatch seg1:5D4A / 5BE5): no
  WM_MOUSEMOVE handler. Without a tab the header has a keyboard focus DS:3FB6
  (0 at the stop, seg1:4CEB): left / right step it over four items with wrap-around
  (seg1:58EF / 591B), Enter (seg1:5406) selects Kaufen, Verkaufen, Karte or item 3,
  which closes the dialog with Plopp02 (seg1:54F8; Zaehler2 instead while DS:3FB8 =
  3). With a focus the focused tab text is white (0xFF), the others 0x45
  (seg1:738C / 740D / 7493); a selected tab clears it. *Round 110:* the 0x43 case
  is not "pressed" but **locked**: item 0 and 1 compare the focus with DS:3FB8,
  Karte with the byte DS:3FBA, and a focused locked item is 0x43; Enter on it does
  nothing (seg1:540F → 5C1C). Opening clears both (seg1:4CFD / 4D09) and locks
  Karte (DS:3FBA = 2) when seg2:3C00 counts no hand card. After the card window
  of the dialog (seg4:0AB0 played, 0B27 cancelled) seg1:8D1A returns to the
  header; with a property card mode DS:3FC6 it locks Karte, sets the focus to
  Verkaufen with Kaufen locked (mode 2) or to Kaufen with Verkaufen locked (modes
  3–9), and posts VK_RETURN, which opens that tab. Rebuilt in
  `EstatesView::open` / `afterCardWindow`. With a tab, up / down move the
  row with wrap-around (seg1:5942 / 5A7F; the statistics stop at the ends).
  Escape / Backspace and the right button (seg1:5365 sends VK_BACK) leave an
  active tab with Plopp02 and give it the header focus again (seg1:53BE),
  otherwise close the dialog (seg1:5391). The stop waits for the dialog
  (seg1:4DBC, seg6:0FB4 on DS:44); afterwards Dr. Drago acts and nextPlayer ends
  the turn (seg2:15A6, 161D). The mouse wheel scrolls only the statistics (recomp
  addition where the original has the PFEIL page buttons). *Round 104:* clicks
  (seg1:5DA2): the header rectangles (right edge excluded) are 0–0x9F, 0xA0–0x128,
  0x129–0x190 and 0x191–0x22E (item 3 closes), all 0x2A high; without an active tab
  a click gives the item the focus and sends Enter (seg1:5FD9), with a tab the
  header ignores clicks; a row click selects and acts only with a tab (seg1:6037).
  A purchase or sale that succeeds sends VK_DOWN to the dialog (seg1:55A5 / 5646),
  so the selection moves on. DS:3FB8 = 3 marks the forced sale (Statistik).
  **Assumptions:** the category
  icons and the statistics variant (ZW_Z rows, PFEIL1 / PFEIL2 page buttons,
  seg1:7506, paging by seven at seg1:59C7 / 5B15) are not drawn yet; the card
  modes Abkaufen / Kredit use the checks of the economy. Scrolling follows
  seg1:494F: past the eighth visible row the selection moves to the top (at most
  to count − 7), above the top the list goes back by seven; a tab selects the
  first visible row (seg1:543E / 5475).

### Zoom (round 38)

- seg6:2E54(mode): mode 3 toggles DS:9C70 between 0 and 1 and stores it in the
  current player's record (+0x15); modes 1 / 2 step it down / up within 0–1
  (returning 0 at the limit). Then the viewport size DS:94DE / 94E0 is the map
  window size − 16, halved with zoom 1 (seg6:2EE9); mouse coordinates in the map
  window are divided by DS:9C70 + 1 (seg6:17A1). Callers: Zoom 1032 / 1040
  (seg4:0883 / 0966, then the view is centred on the player, seg2:3216 / 05A8),
  Insert / Delete (seg4:0212).
- Rebuilt in `src/game_screen.cpp`: per player, zoom 1 shows half the world area
  twice as large (world and vehicles); Umfeld uses the halved viewport. The zoom
  flag is not part of the save file yet.

### Map scrolling (round 67)

- **Origin** DS:956A / 9568 (tiles of 16 world pixels); DS:94DA..94E0 the visible
  world area (map window − 16, halved with zoom, see "Zoom").
- **Centre** seg6:3064(px, py): origin = (p + 16 − size / 2) >> 4 per axis, then
  seg6:2B19(1). seg2:3216(player) passes the player record's +0x16 / +0x18.
  Callers: Space (seg4:01B2), Insert / Delete (seg4:022A), the zoom commands
  (seg4:08A7 / 097E), Tschüss (seg3:29E7), Flugzeug (seg3:3314), seg5:5E86, an
  event (seg8:2238), the player change (seg2:044E) and the spin (seg2:64E5).
  seg8 also centres on event places directly (seg8:1CED, 3AA0 with DS:69F0,
  4023, 5DA1 and others).
- **Clamp** seg6:2B19: x in [0, 0xFF − ⌈width / 16⌉], y in [0, 0x100 − ⌈height / 16⌉].
- **Edge scroll** seg2:29EB, called from the movement tick seg2:239F (after the
  pixel delta DS:956C / 956E = table DS:19EA / 19F4 << DS:34 is chosen): the view
  inflated by −32 must contain the vehicle rectangle (player +0x30..+0x36);
  otherwise seg6:30F8(direction) moves the origin, checked up (1), down (3),
  right (2), left (4), one direction per tick: with zoom by (size & ~0x20) >> 5
  tiles (half the view), else by (size >> 6) · 3 (three quarters).
- **Rebuilt** in `src/map_scroll.cpp` (SDL-free, tested) and
  `GameScreen::advanceVehicles` / `centreOnPlayer`: centring at a player change,
  a spin, Space, zoom and when a vehicle is placed without driving (Tschüss,
  Flugzeug); the edge check every movement tick of the current vehicle.
  The vehicle rectangle is taken as its 16 × 16 field cell.
- **Event places** (round 68): every event handler saves DS:956A / 9568, raises
  the zoom with seg6:2E54(2) (remembering whether it changed), centres on the
  place with seg6:3064(x << 4, y << 4) and plays its animation on the map (e.g.
  Sturm: `sbs\sturm.sbs` frames ST1–ST6 and `snd\E1`, seg8:1D00 ff.); after the
  message the zoom is lowered again when it was raised and the origin restored
  (seg8:1E17–1E35). Places: Sturm / Dürre / Hochwasser the damaged town's cell
  (town record +0 / +2, seg8:1CA0 / 2581 / 4B6B), Nessie, Erdbeben and Vulkan
  their place tables (seg8:3259, 4009 with DS:6AC4, 3A84 with DS:69F0), Regen /
  Schnee / Steinschlag the obstacle cell (seg8:44E9 / 55FA / 5DA1), Räuber the
  victim (seg8:2238 → seg2:3216). Rebuilt: `Notice::focusX / focusY` from
  `src/game_events.cpp`; while such a message is open `GameScreen::eventFocus`
  zooms in and centres, then restores. Nessie's second centring during its
  animation (seg8:33B5) followed in round 74 (`SceneStep::recentre`).
- **Event animations** (round 69): a handler fills an animation definition on
  its stack (u16 delay, u8 0, u8 frame count, frame names), loads its SBS set,
  allocates a record with seg2:6D1B(1) (+0 definition, +4 follow-up 0, +8
  countdown 0, +0xA repetitions, +0xC frame 0, +0x14 / +0x16 position, +0x1A
  the DIB DS:9490) and runs seg8:6E0F: per pass seg2:6DCB advances the record,
  the frame is drawn into the map DIB (seg2:3D9C), then seg6:0CDF(20) waits; the
  loop ends when the record is freed after its repetitions. Only then is the
  message text drawn (e.g. Sturm 5041 + random(2) at seg8:1DF5) and shown
  (seg8:6EF2). Sounds: seg1:2A30 starts the MIDI cue (`e1` …, already in the
  music catalogue) and seg1:2D40(7, `%s\snd\E1`) opens a WAV that this disc does
  not contain (snd holds only fx and midi), so nothing plays.
  **Sturm** (seg8:1C01): `sbs\sturm.sbs` (DS:5F3A), frames ST1–ST6 (DS:5F0E ff.),
  delay 6, repetitions 4, position town · 16 + (8, −0x1D) (seg8:1D47 / 1D4C).
  Rebuilt in `src/event_scenes.cpp` (table, record step) and
  `GameScreen::startEventScene`; the message waits for the scene.
- **Record step** seg2:6DCB (exact, round 70): per record `old = countdown--`; if
  old > 0 nothing else; otherwise countdown = definition delay and the frame
  index grows; past the last frame it returns to 0 and a repetition is used up;
  without repetitions the callback (+0xE, argument +0x12) runs, then the
  follow-up definition (+4) replaces the current one, or the record is freed.
  seg8:6E0F advances before drawing, so with countdown 0 the first drawn frame is
  frame 1 and every frame stays delay + 1 passes. **Correction:** round 69 showed
  each storm frame `delay` passes starting with ST1.
- **Robbers** seg8:1E70: `raub.sbs` (DS:60E7), 39 frames RA01–RA24 (delay 5,
  repetitions 0), position the victim's field (DS:52 record +0 / +1) · 16 +
  (0xC, 4); the view zooms and centres on the victim (seg2:3216). **Volcano**
  seg8:36FD: `vulkan.sbs` (DS:69FF), 54 frames V01–V29 (delay 5, repetitions 0),
  place table (DS:69F0) · 16 + (0xD, 0x11). **Rockfall** seg8:5A76: `stein.sbs`
  (DS:717F), 33 frames st01–st33 (delay 3, repetitions 0), place (DS:7170) · 16
  + (7, −0x17). Their WAV files `snd\e2`, `e4`, `e9` are not on this disc.
- **Earthquake** seg8:3C0B (round 71): DS:6AC4 selects one of twelve places
  (stack tables: centre bp−0x66 / −0x7E = our quake places, crack A bp−0x96 /
  −0xAE, crack B bp−0xC6 / −0xDE, B only at places 1, 6 and 11). seg2:6D1B(2)
  allocates two records with the same definition (`equake.sbs` DS:6AC9, 18
  frames BR01–BR15 + 3 × BR15, delay 5, repetitions 0) at (A + 1) · 16 and
  (B + 1) · 16; its own loop draws A, and B where B's x ≠ 0, until A is freed.
  seg6:0F60(0x7D0) before the loop and seg6:0F60(0x1F4) after it; seg6:0F60(n)
  runs seg2:1C37 n >> 5 times (none in a game without humans) — **assumed** n
  milliseconds. Then text 0x1451 + random(2) for the town on the centre cell
  (seg8:41E6 ff.) and the damage. No shaking. *Corrected (round 133):* seg8:3E52
  first starts the MIDI cue `e5` (seg1:2A30, DS:6AC6), and seg8:4049 / 4064 plays the
  WAV `<DS:7A>\snd\E5` (the file `butt/snd/e5`, no extension) on WaveMix channel 7
  when the loop starts; after the message seg8:427F calls seg1:275E. Not yet rebuilt:
  the event WAVs (seg1:2D40 calls of seg8: 1D2F, 2102, 260A, 3336, 351B, 35F1, 3964,
  4064, 4553, 4E6F, 5664, 5C75).
- **Event sounds** (round 135): every animated event plays a WAV with seg1:2D40(7,
  `wsprintf(<base>\snd\<name>)`, loops) on WaveMix channel 7 — the files are
  `butt/snd/<name>` without an extension (base DS:7A; Nessie's DS:82 holds the same
  directory, as its other paths `%s\fuss.bmp` and the BILANZ voices show):

  | Handler | Event (text) | WAV | When |
  | --- | --- | --- | --- |
  | seg8:1C01 | storm (5041) | `E1` (seg8:1D2F) | after the cue `e1`, with the first frame |
  | seg8:1E70 | robbers (5021) | `e2` (2102) | after `e2`, first frame |
  | seg8:2386 | drought (5141) | `e3` (260A) | after `e3`, first frame (the sun's descent) |
  | seg8:2A32 | Nessie (5101) | `E12` loops 1 (3336), `E12a` (351B), `E12b` (35F1) | E12 after seg6:0F60(500) with the lake phase; E12a / E12b before the loops of the last two phases (seg8:353B, 3611) |
  | seg8:36FD | volcano (5281/5301) | `e4` (3964) | before the cue `e4` and the zoom, first frame |
  | seg8:3C0B | earthquake (5201) | `E5` (4064) | after seg6:0F60(2000), with the cracks |
  | seg8:42B0 | rain (5121) | `e6` (4553) | after `e6`, with the flight |
  | seg8:4964 | flood (5241) | `e7` (4E6F) | after the wave's flight (seg8:4DB0), with RG01 ff. |
  | seg8:535E | snow (5481) | `e8` (5664) | after `e8`, with the flight |
  | seg8:5A76 | rockfall (5081) | `e9` (5C75) | after `e9`, first frame |

  The cue names pushed to seg1:2A30 (e1–e9, e11) were read at each call: `e3` is the
  drought's (*corrects* the former mapping to Nessie's text 5101; Nessie starts no
  cue but calls seg1:275E as well, seg8:36CC). Rebuilt as `SceneSound` entries of
  the scenes (`src/event_scenes.cpp`), started by `GameScreen::advanceVehicles` with
  the phase's first pass; tested in `tests/event_scenes_test.cpp`.
- **Event music ends with the event** (round 133): every event handler that starts
  an `e` cue (seg8:1D03, 20D6, 25DE, 3970, 3E52, 4527, 4C01, 5638, 5C49, 6A10) calls
  seg1:275E after its message (seg8:1E42, 2355, 2A01, 3BDA, 427F, 4933, 4EEA, 5A45,
  5EED, 6C41): stop / close the sequencer and start the next background cue g0–g5
  from zero. Rebuilt in `GameWindow::updateFrame` (`MusicPlayer::nextBackground` once
  the event's notice has been closed); the cue no longer plays on to its end.
- **Scene start draw** (round 72, rule correction, rules revision 31): rain,
  snow and flood call seg8:517B(&x, &y, …) before their animation; it draws
  random(3) (seg8:5211) to pick the corner the cloud / wave starts from: start =
  target + {(−c, −e), (c, −e), (c, e)}[r] with c = view width / 2 + arg, e =
  view height / 2 + arg. The draw comes after the place (seg8:5F43 / 6C77) and
  before the text draw base + random(2); the rebuild did not draw it, so every
  later random number differed after such an event. Storm, drought and flood
  leave at once without animation, text or draw when seg8:6C77 finds no town
  (seg8:1C60 / 2553 / 4B3C); rebuilt as well.
- **Flights** (round 73): seg8:5243(&x, &y, tx, ty, step) returns 1 when both
  axes are within `step` of the target; otherwise each axis further away moves
  by `step` in the direction of seg2:AE18(seg2:AE77(x, y, tx, ty)) — the eight
  directions from the signs of the differences, 1 = up, clockwise — and an axis
  within `step` snaps to the target. The flight loop advances the record, draws
  it at the current position, waits 20 ms and then moves (step 2 for rain, snow
  and flood, start margin 0x28 = 40 for seg8:517B, after the zoom).
- **Rain** seg8:42B0: regen.sbs (DS:6C36); R00 (delay 6, repetitions 1000) flies
  to the obstacle · 16 − (0, 0x11), then R01–R08 (delay 4, repetitions 0) and
  R09–R20 (32 frames, delay 4, repetitions 1) with seg8:6E0F; the text 0x1401 +
  random(2) follows, then the obstacle is entered (seg8:67F8). **Snow** seg8:535E:
  schnee.sbs (DS:7017); SN01 flies to the obstacle · 16 + (5, −0x19), then
  SN01–SN30 (delay 4) and SN31–SN46 (delay 4, repetitions 2). **Flood** seg8:4964:
  flood.sbs (DS:6E1D); RG00 flies to the town · 16 − (0, 0x11), then 46 frames
  RG01–RG26 (delay 8). Rebuilt in `src/event_scenes.cpp` (phases and the flight
  as a precomputed timeline, `Notice::variant` carries the random(3) corner).
- **Drought** seg8:2386 (round 74): sonne.sbs (DS:6239); x = town · 16 + 0x12;
  the sun S01/S02 (delay 7, repetitions 1000) starts at the view top · 16 − 0x14
  and sinks by 2 per pass (seg8:27A3) until |town row · 16 − y| ≤ 0x32; then at
  that height S03 … S16 (23 frames, delay 6), S17–S20 (delay 6, repetitions 3) and
  S21 … S02 (10 frames, delay 6) with seg8:6E0F.
- **Nessie** seg8:2A32 (round 74): nessie.sbs (DS:680E); centred on Loch Ness
  (0x4D0, 0x240), seg6:0F60(500), N01 … N27 (76 frames, delay 5) at (0x4D5,
  0x248), seg6:0F60(1000); then centred on (lake + 1) · 16 and NH01 … (22 frames),
  NH08 … (46), NH44 … (19), all delay 5, at lake · 16 + (0x12, 0x17), then
  seg6:0F60(1000). The lake cells are a second table indexed like the damage towns
  (DS:680C): x 18, 61, 82, 91, 78, 105, 112, 144, 146, 148, 179, 190, 133; y 13,
  67, 85, 93, 135, 107, 98, 83, 65, 33, 82, 82, 130 (stack bp−0x2B8 / −0x2D2). WAVs
  `snd\E12`, `E12a`, `E12b` are not on this disc. Rebuilt with phases that can be
  fixed, recentre the map (`SceneStep::recentre`) and wait; `Notice::variant`
  carries the place index. All event scenes are now rebuilt; definitions were
  extracted with the private helper `animdef.py`.
- **Correction (round 69):** the edge scroll of round 67 also ran for a standing
  vehicle; it now only follows a vehicle that moved in that tick, as in the
  movement tick seg2:239F.

### Stage end (round 43)

- **Order at the destination** (seg2:11A6 ff.), stages left DS:A104 ≠ 0: music
  `random(1)` → Z1 or Z2 (seg2:12D8, seg1:2A30 plays `snd/midi` only with Midi
  on, the draw happens always), the finish sequence seg7:2628 (window ZIEL, no
  random numbers), the reward via seg2:59E6, text 350 + random(2) (seg2:136F:
  player, stage A106 − A104 + 1, total A106, amount), the destination town's
  dialog seg1:4BDD, then — with A104 decremented — at 0 music A1 and the last-
  stage announcement seg7:372F(3), otherwise music (DS:1A55) and seg7:372F(1),
  which draws a raw rand() % 5 (seg7:3B4E); then the next destination and Dr.
  Drago (seg2:9C97).
- **Master-Rallye** (A104 = 0): music Z1 without a draw (seg2:11C2), seg7:2628,
  the reward, text 354 + random(2) (seg2:1231: player, amount), the victory
  sequence seg7:71C4 (window SIEG) and WM_COMMAND 0x424.
- **Corrections:** the rebuild did not draw these numbers before round 43
  (rules revision 27) and did not keep the total stage count DS:A106 (save
  format 4, record `stages`).
- **Pictures:** butt/ziel.bmp, zielausl.bmp, zielclub.bmp, sieg.bmp, bilanz.bmp
  (480 × 360), butt/<lang>/banner.bmp, the town photos butt/<lang>/ziel/<town>.bmp
  (480 × 240) and the animations in butt/sbs/ziel and butt/sbs/sieg.

### Finish sequence (round 45)

- **seg7:2628:** window ZIEL (0x1F0 × 0x178, child of the canvas at (0, 0)); the
  DIB `%s\ziel.bmp` (DS:4CB6, butt directory; missing → no sequence); the banner
  `%s\%c\banner.bmp` blitted at (6, 117) with colour key 0xFF (seg2:9497); the
  driver's car `%cZielCar.sbs` (letter of seg2:2D43) and the files of the table
  DS:4BE8 (pauke, duo, floete, tuba, genie, happy, paar, jazz, hut, ballon, frau,
  winken, zoepfe, pfahl, lampe) from `%s\sbs\Ziel\%s`; the town photo
  `%s\%c\ziel\%s.bmp` (DS:4CF0) whose name is the first eight characters of
  the town name with ö/ü/ä/Ä/Ö/Ü → o/u/a/A/O/U and space → `_` (seg7:28AD).
  seg7:1CE0 copies the photo only where the DIB holds 0xFF (seg7:1DDF), so the
  trees of ziel.bmp stay in front; seg7:1E3C draws first frames of six files.
  `%s\snd\z.wav` (DS:4D02) is played (seg1:3579).
- **Records** (seg7:2C55 ff., 0x40 bytes each at DS:30): position, start frame
  (+0xC), repetitions 999 and the definitions DS:4594 (pauke, 209/182), 419C
  (duo, 339/221), 427C (flöte, 130/206), 461C (tuba, 267/209), 4314 (genie,
  25/214, 20 frames), 4420 (happy, 79/182), 4500 (paar, 405/175), 4490 (jazz,
  180/189), 4760 (car CAR1–3, 460/244, delay 1), 43B0 (hut, 269/304), 4120
  (ballon, 175/280, delay 3), 420C (frau, 78/296), 468C (winken, 29/294), 46FC
  (zöpfe, 347/300), 4736 (pfahl, 0/82), 4744 (lampe, 117/129); delay 4 unless
  noted. The SBS frames of these files use index 0xFF as transparent colour.
- **Loop** seg7:3536: the car record moves 2 pixels left per pass (seg7:3564)
  until x < −160 (0xFF60), then the window closes; each pass draws the records
  over the DIB (seg2:74E4), advances them (seg2:6DCB), shows the DIB (seg2:6F69)
  and waits 20 ms (seg6:0CDF). No random numbers.
- **Rebuilt** (`src/finish_view.cpp`, name mapping `finishPhotoName` in
  `src/board.cpp`): the scene and its animation when a destination is reached
  and the car has arrived. The window is centred on the canvas (seg6:0000, round
  111) with the wooden border of seg6:1A68 (round 112). **Assumptions:** records draw with their top-left corner at the position.
  **Input** (round 94): the window ZIEL (seg7:50D6) takes Escape, Space, Enter
  and the left button: seg5:607B(−1) (only the PLINFO repaint) and DS:A29C = 0 end
  the loop seg7:3536..3677 (set to 1 on its first pass, seg7:35F0); the window is
  destroyed with the clean-up at seg7:3680..3715. The loop also ends when record 8
  has left (x < −0xA0, seg7:3546 → DestroyWindow 3555). *Round R4:* the voice
  `snd\z.wav` (30.3 s, longer than the scene) stops in both cases through ZIEL's
  WM_DESTROY (seg7:4DF1): seg1:3C3B (seg7:4E28) calls waveOutReset when a wave device
  and [Setup] Voice are on (seg1:3C4F..3C71). Rebuilt: `sound_.stop(0)` on the key
  and when `FinishView::update` ends the scene.

### Destination announcement (round 49)

- **seg7:372F(mode)**: 2 at a new game (seg4:0E94), 1 for a new stage
  (seg2:1486), 3 before the last stage (seg2:1453). It first chooses the final
  destination seg5:1E2C(DS:A10B, DS:A10D) into [bp−0x38] (seg7:3755) — with the
  old stage start, because seg2:148E sets DS:A10B to the reached field only after
  the call — and in mode 1 then draws rand() % 5 for the voice variant
  (seg7:3B4E; otherwise 3). Modes 1 / 2 store [bp−0x38] in DS:A10D (seg7:3F46 /
  45E7); mode 3 never does and sets Brussels instead (see "Last stage /
  Master-Rallye", round R3a).
- **Scene:** butt/club_map.bmp with the show master records of the balance
  (DS:4800 speaking 79/155, DS:4798 foot, DS:4AE8 lever 150/184; DS:4A30 pull
  and DS:4938 / 4968 hold at 80/159) and the light DS:4782 (licht1 / licht2 of
  ziel\licht.sbs) at field cell + (0xB0, 0x24) (seg7:3DEF / 3E0C). Voices
  `%s\%c\VOICE\ZIELSUCH\Part01%c.wav`, then at frame 9 of the pull FX HEBEL01
  and the search starts (seg7:3CE4); `Part02%c` when the pull is done; the
  search stops when Part02 has played out (seg7:3F28) and the light moves to the
  final destination (seg7:3F43); then `Part03%c`, the town (`%s.wav` with the
  eight-character name) and `Part04%c`; Master1 / Master2 belong to the last
  stage. During the search the light jumps until the 20th jump every 4 passes,
  then when the wait exceeds jumps − 16 (seg7:3D35); **every jump calls
  seg5:1E2C and draws random numbers**, and the number of jumps depends on how
  long Part02 plays, so these draws cannot be reproduced.
- **Rebuilt** (`src/stage_view.cpp`; rule fix in `src/game.cpp`, rules revision
  30): the final destination is chosen before the variant draw and with the old
  stage start excluded; the scene for modes 1–3 with the original voices. The
  light's jumps use a display-only random generator. **Assumptions:** the
  mode-3 order (Master1, search, Master2), the show master's frames between the
  states, at most 20 s per voice.
- **Input** (round 91): ZIEL (seg7:50D6) handles WM_KEYDOWN with Escape, Space or
  Enter and WM_LBUTTONDOWN only: seg5:607B(−1) (the PLINFO repaint, no voice
  effect) and DS:A29C = 0 end the current loop. Modes 1 / 2 run the loop
  seg7:3BB5..46E8 with the sub-state [bp−0x34] (1 Part01, 2 lever, 3 Part02, 4
  Part03, 5 town, 6 Part04): in the states below 4 and in state 4 — the test at
  seg7:44CA (`mov ax, seg seg1 / or ax, 0x3E64 / je`) checks the far address of
  seg1:3E64, never 0, not whether Part03 plays — seg7:44D4..46BC puts the light on
  the destination (DS:A10D = [bp−0x38], seg7:45E7), sets state 4 (46A7) and stops
  the voice with seg1:38B7, which sets DS:3E44 = 1; it starts no voice. In the next
  pass state 4 (seg7:4045) finds seg1:3E64 = 0 (DS:3E44 set, seg1:3E78) and goes to
  state 5 with the town's name (seg7:4078..4176), then Part04: *correction (round
  R4):* a key before or during Part03 skips Part03. In states 5 / 6 the loop and the
  announcement end (jump to seg7:4C28). Mode 3 runs seg7:4730..4B85: during Master1 (state 0) seg7:4B38 sets
  the flag again, so only the voice stops; later the loop ends. *Correction (round
  R4):* seg7:4B89..4C22 is no wait of 60 passes but a search over the 60 slots of
  the destination list DS:1A for the field whose town name equals `BRÜSSEL` /
  `brüssel` (DS:4E4B / 4E53, lstrcmpi), which becomes DS:A10D — the last stage
  always leads to Brussels (not rebuilt in `src/game.cpp`; see the R4 notes); then
  the clean-up follows at once. **Voice stop (round R4):** where a loop goes on,
  seg1:38B7 (waveOutReset, DS:3E48 = −1) stops the voice at seg7:46BC / 4B59; where
  it ends, the clean-up destroys ZIEL (seg7:4CC2) and its WM_DESTROY calls seg1:3C3B
  (seg7:4E28, waveOutReset). Either way the voice stops at once. Rebuilt in
  `StageView::skip`. The loop seg7:3536..3677 that also tests DS:A29C is the finish
  (seg7:2628), not the announcement.

### Year balance (round 44)

- **Call** at the year change (seg2:0784 ff.): music (DS:1A23), seg1:8AA7
  (credits charged), seg1:85F9 (settlement), year + 1, inflation random(5),
  then **seg8:88C6** (window BILANZ), seg1:8BC6, seg1:8B41 (credits cleared),
  seg1:275E.
- **seg8:88C6** draws rand() % 7 at once (seg8:88DF, a choice used later for the
  voice `pos_%c`), creates BILANZ with `%s\bil_leer.bmp` (480 × 360, butt) and
  `butt/sbs/bilanz/bilanz.sbs`, and when the window is up draws rand() % 5
  (seg8:8C49) and rand() % 4 for the voice `%s\%c\VOICE\Bilanz\Bilanz_%c.wav`
  with 'A' + r (seg8:8C57). During the animation further draws depend on its
  timing: rand() % 4 for `Next_%c_%c` after 300 ticks without a human
  (seg8:8E38) and rand() % 2 for `pos_` (seg8:9605) in some animation states;
  they cannot be reproduced and are not modelled. *Correction (round 92):* the
  draw at seg8:9605 belongs to the closing part (below) and is not timing-bound
  for computers; it is modelled since rules revision 35.
- **Rebuilt:** the three fixed draws (hook `yearBalance` between inflation and
  clearing the credits; rules revision 28).
- **Closing part** (round 92): the players' loop seg8:8CC5..9258 continues to
  seg8:925C when, after the last player (seg8:8E20, slot index 8), the show
  master would go on, or at once when DS:A6F2 is cleared. seg1:38B7 stops the
  voice, `Next_a_a` (DS:7D85) plays; when it has played out the lever is pulled
  (Hebel01, DS:7DA5); at lever frame > 13 `pos_<letter>` (DS:7DB7, lower-case)
  follows, the letter being [bp−0x7E] = the next letter after the last player:
  4 → 5, 5 → 4, otherwise rand() % 2 + 4 (seg8:95E5..9613). The loop
  seg8:93CF..9871 ends when DS:A6F2 is cleared, on DS:8D5C, or after 500 passes
  without a human (seg2:5C5A). The draw is modelled in `Game::yearBalance`
  (rules revision 35) as if the show always reached the lever; see the timing below.
- **Closing draw timing (round 141, replaces the assumption of round 92).** seg8:925C
  stops the voice, starts `Next_a_a` (seg8:928A, 1.57 s in the German data), sets
  state [bp−0x6E] = 0, pass counter [bp−0x6C] = 0 and DS:A6F2 = 1 (seg8:92A1);
  DS:8D5C is cleared only on the regular path (seg8:8E1A), not on the Escape jump
  (seg8:9253). One pass of seg8:93CF..9871: the state logic, records (seg2:74E4,
  6DCB, 6F69), seg6:0CDF(20) (seg8:97EB), the blit, the idle pass only while
  DS:94C4 is set (cleared at window creation, seg8:9A5E), the message pump seg6:03F7
  (seg8:9842; it dispatches the window's input), then the end test. State 0 → 1 in
  the first pass with seg1:3E64 = 0 (`Next_a_a` played out, seg8:943B): record 0 =
  DS:4A30 (`HEBEL05`–`13`, 23 frames, delay 3), follow-up DS:4968. Counting that
  pass as 0, pass 35 sees frame 9 with countdown 1 and plays `%s\FX\HEBEL01.wav`
  (DS:7DA5, base DS:72; snd/fx/hebel01.wav, 1.55 s) through seg1:3579, the voice
  path (seg8:94E7–9547). The rand()
  % 2 (seg8:9605; skipped when the letter is 4 or 5) runs in the first pass q ≥ 53
  (frame > 13, seg8:959A) in which seg1:3E64 reports Hebel01 played out — about
  pass 35 + 78, i.e. roughly 3.8 s after the closing part began (more when passes
  take longer than 20 ms; without a wave device or with [Setup] Voice off the
  TimerCount path of seg1:3E64 still waits the WAVs' lengths, round 102).
  **A human can skip it:** Enter, Space or a left click (DS:8D5C = 1), Escape or a
  right click (DS:A6F2 = 0) dispatched by the pump of any earlier pass end the loop
  at that pass's end test, so no draw happens; likewise a click still pending when
  Escape jumped from the players' part ends the closing part after its first pass.
  Without humans the 500-pass exit (seg8:9857; the counter restarts at seg8:9452 and
  9656) cannot come first with these WAV lengths, so a game of computers always
  draws. **Also input-bound:** Escape in the players' part (seg8:9253) skips the
  remaining players' rand() % 4 (seg8:8E38), and the closing letter is then the one
  after the last *spoken* `pos_` / `neg_` ([bp−0x7E] advances only at seg8:913D, after
  the wsprintf of `pos_` 90EA or `neg_` 9115; Escape falls through the end test
  seg8:9253 into the closing part seg8:925C, which reads it at 95E5), which also
  decides whether seg8:9605 draws. The rebuild draws all of them at the year change
  (deterministic; equal to the original whenever the show is watched to the lever).
  *Round 144, rebuilt as an authoritative phase:* the year balance hook suspends the
  player change after the inflation (`TurnChange::suspended`, `Game::Phase::Balance`)
  when a window shows it. The show sends commands at the original points:
  `Game::balanceAnnounce` as the show master goes on to the next player (seg8:8E38,
  rand() % 4), `Game::balanceSpeak` when the player's `pos_` / `neg_` starts (seg8:913D;
  *round 151*, before that the letter was counted per announcement, wrong when Escape
  came between a player's `Next_` and his result), `Game::balanceClose` at the closing
  lever (seg8:9605, the letter after the last spoken result) and `Game::balanceFinish`
  when it ends; the
  finish resumes nextPlayer at seg1:8BC6 / 8B41 (credits cleared, month) with the
  event trigger, sit-outs and Dr. Drago following in the original order. Escape or
  a click before a draw skips it. Without a window (the terminal mode, tests) the
  hook draws everything at once, the show watched to the end. The phase is never
  saved (BILANZ is modal; the autosave waits). If `bil_leer.bmp` is missing the show
  does not start (seg8:89C0) and the player change goes on without those draws;
  the rebuild still takes the window's own rand() % 5 / % 4 (seg8:8C49 / 8C57) in that
  case, which rally.exe would skip too (not modelled, needs a missing file).
  Rules revision 40.
  *Round 146, the show master in the closing part:* seg8:92A7 sets record 0 to
  DS:4938 (HEBEL02, 03, 02, 03, 04; delay 3). In every pass in which seg1:4071
  reports the position of `Next_a_a` inside one of its sync.dat talk intervals, or
  seg1:3E64 reports it ended, seg8:9428 / 9436 set the record's countdown to 2 and
  its frame to 0 before seg2:6DCB steps; between the intervals it runs on. The
  rebuild showed the speaking record DS:4800 there; `BalanceView` now holds DS:4938
  with the talk intervals of `dat/<c>/sync.dat`. Rebuilt in `BalanceView`: the closing
  lever's pass timing, Hebel01 on the voice path, the pending click on Escape and the
  500 passes counted from the `pos_` voice.
  *Round 150, the closing part pass by pass* (`src/balance_closing.cpp`, tested in
  `tests/balance_closing_test.cpp`; replaces the time-based stages of rounds 141 / 146).
  Setup seg8:925C..93CA: seg1:38B7 stops the voice, `Next_a_a` (DS:7D85) starts, state
  [bp−0x6E] = 0, counter [bp−0x6C] = 0, DS:A6F2 = 1; record 0 = DS:4938 at (0x50, 0x9F),
  follow-up 0, repetitions 999, frame 0, countdown 0 (seg8:92A7..9323); record 2 = the
  lever DS:4AE8 at (0x96, 0xB8), repetitions 999, countdown 0x190 (seg8:932B..93B2).
  One pass of seg8:93CF..9871:
  1. seg8:93CF: record 2 countdown = 0x190 (its single frame stays); counter + 1 up to
     0x7530; the subtitles.
  2. seg8:9408: (seg1:4071 talking, or seg1:3E64 = 0) and state 0 → record 0 countdown
     2, frame 0.
  3. seg8:943B: state 0 and the voice ended → state 1, counter 0; record 0 = DS:4A30,
     follow-up DS:4968, repetitions 0, frame 0, countdown 0; record 2 cleared (seg8:94D7).
  4. seg8:94E7: state 1, DS:4A30 on frame 9 with countdown 1 → Hebel01 (DS:7DA5) through
     the voice path; DS:8D5E = 0xA and a repaint (the graph).
  5. seg8:956A: state 1, the voice ended and (DS:4A30 with frame > 13 or DS:4968) →
     record 0 = DS:4938, countdown 0, frame 0, repetitions 999 (seg8:95BF..95DF); the
     letter (4 → 5, 5 → 4, else rand() % 2 + 4, seg8:95E5..9613) and `pos_<a + letter>`
     (DS:7DB7); state 2, counter 0. No hold in state 2: DS:4938 runs while `pos_` plays.
  6. seg8:965B: state 2 and the voice ended → in every such pass record 0 = DS:4968
     (HEBEL03, follow-up 0, repetitions 999, frame 0, countdown 0, at (0x50, 0x9F)) and
     record 2 = the lever again (seg8:96F3..977A); the state stays 2.
  7. seg2:74E4, 6DCB, 6F69, seg6:0CDF(20), the blit, the pump seg6:03F7 (input), then the
     end tests: DS:A6F2 = 0 (Escape / right click, seg8:984E), or counter ≥ 500 and
     seg2:5C5A (seg8:9857), or DS:8D5C (Enter / Space / left click, seg8:986C) end the
     loop; seg6:6554 closes the subtitles and seg1:3C3B (seg8:98B3) resets the wave output
     (waveOutReset), so a voice still playing stops; the rebuild stops its voice channel.
  The voices end when seg1:3E64 reports it: with a wave device and [Setup] Voice the
  device's position (seg1:3F1F), otherwise TimerCount against the sync.dat record's
  length (seg1:3EB1..3F19); `SoundPlayer::busy` uses the device, or without one the
  WAV's own length (round 102). The former minimum of 1.5 s per
  voice is gone. Not in rally.exe: a voice counts as ended after 60 s (3000 passes, the
  longest show voice lasts 19.6 s) so a stalled audio device cannot hold a show without
  humans. Input (seg8:99AC..99DC) reaches the loop only through the pump right before the
  end tests, so the rebuild applies Escape and clicks between passes: the closing part
  ends at once; Escape in the players' part starts the closing part (its end test,
  seg8:9253), and a click still pending then ends it after the closing part's first pass.
  *Round 148, the players' part pass by pass* (`src/balance_players.cpp`, tested in
  `tests/balance_players_test.cpp`). Before the loop seg8:8A48 sets record 0 (DS:30)
  to DS:4800 at (0x4F, 0x9B), repetitions 999, frame 0, countdown 0, and record 2 to the
  lever DS:4AE8 (seg8:8B49); seg8:8C8F starts `Bilanz_`, state [bp−0x6E] = 0,
  [bp−0x6] = the first slot in play (seg8:8CA1). One pass of seg8:8CC5..9258: the pass
  counter [bp−0x6C] + 1 (up to 0x7530), the subtitles, then in this order
  (phase = state % 3, every test reads the state as the earlier steps left it):
  1. seg8:8D03: state 0, seg1:3E64 playing and DS:8D5C → clear it, seg1:38B7 stops
     the opening voice.
  2. seg8:8D2B: seg1:4071 (a sync.dat talk interval) in phase 0 or 1 → record 0
     countdown 2, frame 0.
  3. seg8:8D65: phase 0 and the voice ended → frame 4 when record 0 is DS:4938, else
     frame 0; countdown 2 (the show master waits on HEBEL04).
  4. seg8:8DC8: phase 0, the voice ended, and DS:8D5C, or state 0, or more than 300
     passes and seg2:5C5A → clear DS:8D5C; slot 8 → the closing part (seg8:8E28; also
     with no player in play); else rand() % 4 (seg8:8E38, `Game::balanceAnnounce`) and
     at once `Next_<A + r>_<A + state / 3>` (DS:7D15, seg8:8E70); state + 1, counter 0.
  5. seg8:8E85: phase 1 and `Next_` ended → state + 1, counter 0; record 0 = DS:4A30
     (23 frames, delay 3, repetitions 0), follow-up DS:4968 (HEBEL03 × 3, delay 9999),
     frame 0, countdown 0, at (0x50, 0x9F); record 2 is cleared (seg8:8F3D) and stays
     so for the rest of the players' part (the closing part sets it again, seg8:932B).
  6. seg8:8F47: phase 2, record 0 = DS:4A30 on frame 9 with countdown 1 (pass 35 of
     the pull) → `%s\FX\HEBEL01.wav` (DS:7D37) through the voice path seg1:3579 /
     37EA; DS:8D5E = the slot and InvalidateRect / UpdateWindow (seg8:8FC4).
  7. seg8:8FE2: phase 2, seg1:3E64 = 0 (Hebel01 ended) and (DS:4A30 with frame > 13,
     from pull pass 53, or DS:4968) → record 0 = DS:4938, countdown 0, frame 0,
     repetitions 999 (the follow-up field is left); `pos_` / `neg_` (seg8:9075) with the
     letter, letter + 1 mod 7 (seg8:913D); state + 1, counter 0; the next slot in play
     (seg8:9168).
  Then seg2:74E4, 6DCB (step: `old = countdown--`; at old ≤ 0 countdown = delay,
  frame + 1; past the last frame frame 0 and a repetition used, without one the
  follow-up replaces the definition with the countdown kept, seg2:6E2B–6F51) and
  6F69 (show), seg6:0CDF(20), the blit, the message pump and the end test DS:A6F2.
  So per player: `Next_` starts when the show master goes on, the lever follows when
  it has played out, Hebel01 at lever pass 35, `pos_` / `neg_` in the first pass from
  53 on in which Hebel01 has played out; a click given earlier (during `Next_`, the
  pull or `pos_`) is kept and lets him go on as soon as `pos_` / `neg_` ends; without
  humans 301 passes after `pos_` / `neg_` started. The draw order (seg8:88DF, 8C49,
  8C57, one seg8:8E38 per announced player, seg8:9605) is unchanged; the rebuild only
  moved `Next_` before the lever. Rules revision and save format are unchanged (the
  show is presentation; its draws were already commands). The players' part runs one
  pass per 20 ms at most and does not catch up after a stall (a pass of rally.exe waits
  20 ms).
- **The board (round 149,** `src/balance_board.cpp`, drawn by `src/balance_board_view.cpp`,
  tested in `tests/balance_board_test.cpp`). WM_PAINT of BILANZ (seg8:9AB8) copies
  `bil_leer.bmp` into the WinG surface (seg2:913A at seg8:9B8B), draws the board chosen
  by DS:8D5E, then every record (seg8:A651, seg6:41A2) and blits the surface. DS:8D5E is
  0xFFFF at the start (seg8:88F8: an empty board), the slot from the players' Hebel01 on
  (seg8:8FC7, InvalidateRect / UpdateWindow at once) and 0xA from the closing Hebel01 on
  (seg8:954C); other values draw nothing (seg8:9B93 / A1E5). Colours are PALETTEINDEX in
  the game palette DS:9CD2 (0x45 = (67, 27, 0), 0xED = (255, 0, 0), equal in
  `bil_leer.bmp`, `bilanz.bmp` and `bmp/slot*.bmp`).
  - *Sheet of slot s* (seg8:9BA5..A1E2): rectangle left 0xBB, top 0x3C, right 0xBB + 0xF6,
    bottom 0x3C + 0x100; DrawText through seg2:314A (shadow 0), labels format 0x100 (left),
    values 0x102 (right). Title: CreateFont(−18, 0, 0, 0, 400, …, FF_SWISS 0x20,
    "MS Sans Serif" DS:7DD5), text 8001 (`Bilanz für %Fs`) through sprintf (seg10:18DC) with
    the name at player +1, colour 0x45; top + 0x2D. Then seg6:04F9(16, 0, 400, 0)
    (Arial 16): text 8002 with the growth (below), + 0x19; 8003 with +0x66 (always 0x45),
    + 0x19; 8007 with +0x51A (credit debt), + 0x37; 8004 with +0x5A, + 0x19; 8005 with
    +0x42, + 0x23; then seg6:04F9(16, 0, 600, 0): 8006 with +0x42 + +0x5A. Values through
    seg6:137F (`$%li` of the truncated value, the first two characters swapped when the
    value is below 0, a '.' every three digits), colour 0xED when the value is below 0.0
    (DS:8786), else 0x45. Growth (seg8:9C91): with DS:A102 ≥ 2 the DWORDs
    +0x66 + 4·A102 − +0x62 + 4·A102, i.e. worth[A102 − 1] − worth[A102 − 2]; otherwise
    +0x66 + 4·A102 − 30000 (seg2:033A returns DS:85BE = 30000.0): worth[0] − 30000 after the
    first year (the year's rent − 30000 for A102 = 0, only reachable with `--balance`).
  - *Graph* (seg8:A1EE..A64C): maximum from 30000.0, minimum from 0.0 over worth[i],
    i < DS:A102, of the slots in play; scale = 190 (DS:878E) / (max − min). Per slot in
    play: CreatePen(6 = PS_INSIDEFRAME, 2, PALETTERGB from the table seg8:9942: 0B5337,
    2F2F2F, C30000, 736F6F, DB531B, 075B9F, 9F3793, 87634F as RRGGBB) with NULL_BRUSH,
    MoveTo(0xBB, (max − 30000)·scale + 67), LineTo(0xBB + (i + 1)·0xEB / A102 (seg10:20A0),
    (max − worth[i])·scale + 67) for every year (y through `_ftol`, DS:8796 = 67); then the
    `bilanz.sbs` frame `"DFEMUJIV"[slot]` (seg2:2D43, found by seg6:406D) copied opaque
    26 × 26 to (0xC0 + 30·k, 0x107), k counting the slots in play. Then a pen of width 1,
    PALETTERGB(A7, A3, 6B): for k = 0, 5, … < 0xBE the lines (0xB7, 0x43 + k)–(0xBF, …)
    and (0xBB + k, scale·max + 64)–(…, scale·max + 72) (DS:879E / 87A6); then width 2,
    PALETTERGB(8B, 87, 57): the axis (0xBB, 0x43)–(0xBB, 0x101) and the zero line
    (0xBB, scale·max + 68)–(0x1A6, …) (DS:87AE).
  - *The values* come from seg1:85F9, run every month (seg2:07DC) and at the year end
    before DS:A102 + 1 (seg2:0795 / 079A): for each slot in play (seg1:862C) +0x52 and
    +0x5A are cleared in that slot's own pass (seg1:8644 / 865B), entries 0..7 of every
    town are settled (seg1:86B5), +0x66 += _ftol(+0x52) (seg1:8A07), +0x62 += _ftol(+0x4A)
    (seg1:8A33; the month's cash flow of seg2:59E6 / 5A47; modelled since round R1 as
    `monthFlow` / `yearFlow`, the board never shows +0x62 itself). After every month
    seg1:8CBA (called at seg2:07E6) clears +0x52 and +0x4A of all slots 0..7 (seg1:8CED /
    8D04; rebuilt in round R1 as `Economy::clearMonth`, before it the rebuild kept +0x52
    until the next settlement); the statement STAT reads both first (see "Monthly
    statement"). While DS:A102 ≤ 300,
    worth[A102] = +0x6A + 4·A102 = _ftol(_ftol(+0x5A) + +0x42) (seg1:8A3B..8A97). seg1:8BC6
    clears +0x66 and +0x62 after the balance (seg2:07CA), seg1:8C26 the 300 worth entries
    at a game start (seg4:0DE9 → seg1:8C8E). Rebuilt in `Economy::settleMonth(active, year)` (rules
    revision 42: the settlement now skips slots not in play, settles entries 0..7 only and
    clears a slot's values in its own pass: the tenant's pass adds a rented entry's value
    to the landlord's +0x5A, so a landlord with a higher index than his tenant loses it
    again in his own later pass, one with a lower index keeps it; the supplied town data
    has no entries 8 or 9) and `clearYearRent`; saved as `worth` records (format 6).
  - *Not reproduced exactly:* "MS Sans Serif" is drawn with the text renderer's Arial /
    Liberation Sans; worth[300] aliases in rally.exe the low DWORD of the credit-debt
    double (+0x6A + 4·300 = +0x51A), the rebuild keeps a separate entry (only reachable in
    year 300); GDI's nearest-colour choice for PALETTERGB is replaced by the nearest
    palette entry in RGB distance; the width-2 pens are drawn as 2 × 2 squares along the
    line (GDI's geometric pen shape not traced). Worth entries past year 300 and DS:A102
    above 301 (reads beyond the table) are not modelled. The disassembly shows no write
    to DS:A102 other than the increment at seg2:079A (a reset by a block copy, e.g. when
    loading, was not searched); the rebuild starts every game at year 0.
- **Input** (round 92, procedure seg8:992E, dispatch seg8:A829): WM_KEYDOWN Escape
  and WM_RBUTTONDOWN clear DS:A6F2; Enter, Space and WM_LBUTTONDOWN set DS:8D5C.
  In the players' part DS:8D5C stops the opening voice (seg8:8D17) or lets the
  show master go on (kept until used, seg8:8E1A); in the closing part both flags
  end the show.
- **Show (round 47):** window BILANZ (0x1F0 × 0x178) with `bil_leer.bmp` (the empty
  board; `bilanz.bmp` is not used by rally.exe) and three records: the show
  master speaking (DS:4800, intro\redet, 79/155), his foot (DS:4798 red_foot,
  74/276) and the lever (DS:4AE8 HEBEL, 150/184). Opening voice
  `Bilanz_<A + rand() % 4>`; then for every player in play (slot order): the
  show master waits (DS:4938 at 80/159) for a click (DS:8D5C) or, without
  a human, 300 passes (seg8:8DF4; the first player follows at once), announces
  `Next_<A + rand() % 4>_<A + i>` (seg8:8E38: one draw per player), pulls the
  lever (DS:4A30, HEBEL05–13, and HEBEL01 on the voice path) and says
  `pos_` / `neg_<A + letter>` (order and timing: round 148 above): positive when cash + property value
  reach the value of the ranking's middle entry (index (count − 1) / 2,
  seg8:9075); the letter starts at the rand() % 7 of seg8:88DF and advances by one
  per player modulo 7 (seg8:913D). The voices are under butt/<lang>/voice/bilanz.
  Subtitles (seg6:6596, window SUBTITLE) only with `[Setup] Subtitle`.
- **Rebuilt (round 47):** the per-player draws (rules revision 29) and the show
  (`src/balance_view.cpp`, data `Game::balanceEntries`). *Superseded:* the draw of
  seg8:9605 (round 141/144), the subtitles, the players' part (round 148) and the
  closing part (round 150) are rebuilt pass by pass since; no minimum voice length
  remains.

### Monthly statement (round R1)

- **Callers.** nextPlayer (seg2:06FC): when the order reaches the first active slot
  (seg2:1A41 = DS:94E2, seg2:0766) and DS:A100 % 12 ≠ 11, seg2:07DC settles (seg1:85F9),
  seg2:07E1 calls seg1:972F (STAT), seg2:07E6 seg1:8CBA, seg2:07EB DS:A100 + 1, then the
  event trigger (seg8:1624). December's branch (seg2:0784..07D9: credits, settlement,
  DS:A102 + 1, inflation, BILANZ, seg1:8BC6, 8B41, seg1:275E) has no STAT and jumps to
  seg2:07E6 too. The second caller is the Master-Rallye (DS:A104 = 0) at seg2:12A3: after
  the finish (seg2:11DF → seg7:2628), the reward 300,000 × inflation (seg2:11FF → 59E6),
  its message 354 + random(2) (seg2:1285) and the modal loop seg2:1297; after STAT another
  loop (seg2:12A8), then the victory seg7:71C4 and WM_COMMAND 0x424. No other caller.
- **seg1:972F.** seg1:8EE1 (the ranking into DS:957A..9581, 0xFF for empty places),
  CreateWindow(class / name "STAT" DS:4084 / 407F, style 0x44000000, x / y two
  never-written locals, 0x24E × 0x1C4 = 590 × 452, parent the canvas DS:40); DS:96A0 (the
  main window's input) is saved and cleared; seg1:9AA2 and seg1:9DE2 paint the DIB once;
  BringWindowToTop, ShowWindow(5), InvalidateRect, UpdateWindow, SetFocus; seg6:0FB4(&DS:A7BE)
  and seg6:03F7 until no message is left; DS:96A0 restored. No sound, voice or music call
  and no random draw in seg1:972F..A15A.
- **Procedure seg1:97F8** (dispatch seg1:9A43): WM_CREATE (seg1:9812) opens `%s\buy.sbs`
  (DS:4089, sbs directory DS:76/78) and creates a 0x230 × 420 WinG DIB (header DS:AD8C; the height is 420 times the
  run-time orientation DS:A06A, which seg2:913A handles both ways) with the palette DS:9CD2, then centres the window with seg6:0000 (seg1:98BF);
  WM_DESTROY frees both and clears DS:A7BE; WM_PAINT (seg1:992D) WinGBitBlts the client
  rectangle (558 × 420) from (0, 0) of the DIB; WM_ERASEBKGND returns 1; WM_NCCALCSIZE /
  WM_NCPAINT go to seg6:1E83 (16-pixel border BS1–BS8, seg1:9A0A); WM_KEYDOWN (seg1:98CA)
  with 0x20, 0x0D, 0x1B or 0x08 and WM_LBUTTONDOWN (0x201, seg1:9A7A) call DestroyWindow;
  0x30F / 0x311 go to seg5:3941 (palette); all else to DefWindowProc. There is no case for
  WM_RBUTTONDOWN, other keys or timers.
- **Closing on a computer's turn.** seg6:0FB4 with DS:A7BE ≠ 0 doubles its timeout
  (DS:859E = 2.0): while the current player DS:94E2 is a computer (seg2:5C02) it posts
  0x7EF after _ftol(_ftol(pause × 2500 (DS:85AE)) × 2) ms, or 100 × 2 once a demo ends
  (DS:16); seg1:0176 turns 0x7EF into WM_KEYDOWN 8 (VK_BACK), which destroys STAT.
  seg1:00D2 drops 0x100–0x102 and 0x201–0x204 / 0x206 while a computer is the current
  player; seg1:01D7 receives the dropped Escape / WM_RBUTTONDOWN (the end of a demo).
  The current player is the new month's first slot at seg2:07E1 and the winner at seg2:12A3.
  *Recomp addition:* like the message window's wait, the rebuild divides this time by
  `[Extensions] ComputerTempo` (Computerzüge 1 / 2 / 4), so 5000 ms × pause becomes 2500 or
  1250 ms; the demo end's 200 ms is not divided.
- **Paint, header (seg1:9AA2).** Frame `KO_Z` (seg6:406D) copied opaque by seg2:913A at
  (0, 0) and (0, 42) of the DIB (a plain row copy with clipping, index 0 included: KO_Z has
  82, the drivers' frames 80 such pixels). seg6:04F9(24, 0, 600, 0) (Arial 24 weight 600),
  rectangle (0x3A, 10, 0x22E, 0x2A + 10): sprintf `"%s %s"` (DS:4099) of LoadString 340
  (`Monatsabschluß für`) and 301 + DS:A100 % 12 (the month that ends; DS:A100 is counted
  afterwards), seg2:314A format 0x100, shadow 2, colour 0xFF. Then Arial 20 weight 400,
  top + 42: 341 at left 58, 342 at 208, 343 at 328 (format 0x100), 344 with right 0x224
  (format 0x102), all shadow 1, colour 0xFF.
- **Paint, ranks (seg1:9DE2).** Arial 18 weight 400; for rank i = 0..7 the rectangle
  (0, (i + 2) · 42, 42, + 20): `KO_Z` at (0, (i + 2) · 42); if DS:957A + i ≠ 0xFF the frame
  `"%c_m"` (DS:40A4, letter seg2:2D43 `DFEMUJIV`) at the same place, top + 10, then
  seg6:137F amounts, format 0x102, shadow 0, colour 0x45 (also when negative): +0x4A with
  right 142, +0x52 with right 252, +0x42 with right 392, +0x42 + +0x5A with right 542.
- **+0x4A, the month's cash flow.** Writes: seg2:59E6 adds and seg2:5A47 subtracts the
  same amount on +0x42 and +0x4A (seg2:5A28 / 5A89); seg1:8CBA clears it (seg1:8D04) after
  every month and at a game start (seg4:0DE9 → seg1:8C8E → 8CBA). No other write (searched
  all segments for stores and `lea di` copies to +0x42..+0x4F indexed by a player record;
  block copies of whole records, e.g. by a load, were not searched). Callers of
  59E6 / 5A47 (34): purchases and sales seg1:7E4C, 8115, 8150, 82D0, 8383, 846A, 84A4, 85D9;
  the settlement's surplus and income seg1:88AC / 88ED; credits seg1:8B2D; money fields
  seg2:0E6D / 0FFD; rewards seg2:11FF / 133D; time-limit fines seg2:3B8A / 3BB2 / 3BD1;
  Dr. Drago seg2:BB02, BF38, BF55; cards seg3:12F5, 159A, 16D1 / 1700 (Konto: the difference
  to the average, then the average written directly at seg3:1715), 1810 / 1839 (Schulden),
  191F (Null), card shop seg3:4896 / 49B1; robbers seg8:22A4; investments seg8:6491, 6629,
  735F. The only direct writes of +0x42 are the start cash (seg2:0178), the name codes
  (seg2:6374 / 63B3) and Konto's seg3:1715; they leave +0x4A alone. seg1:8A1C..8A37 adds
  _ftol(+0x4A) to the DWORD +0x62 in the settlement; seg1:8BC6 clears +0x62 (seg1:8C16);
  +0x62 itself is never read (seg8:9D01 reads it only as worth[A102 − 2]).
- **Rebuilt (round R1):** `Economy::addMoney` (59E6 / 5A47) and every economy rule update
  `PlayerMoney::monthFlow` / `yearFlow`; `Economy::setCash` is the direct write;
  `Economy::clearMonth` is seg1:8CBA (also +0x52, which the rebuild had kept until the next
  settlement). nextPlayer's hook `monthStatement` suspends the change after the settlement
  (`TurnChange::statement`), `clearMonth` runs at seg2:07E6 in both branches. With
  `Game::setStatementWindow` (the game window) the game waits in `Phase::Statement` until
  `statementFinish` (`src/game_statement.cpp`); without a window (headless, AI tests) nothing
  waits and no draw differs (`tests/statement_test.cpp`). Sit-out texts of the players reached
  before the wrap to the first slot are announced before STAT (and before BILANZ): their
  modal messages are shown inside seg2:089D (seg6:51EA at seg2:096F / 0A0D, Dr. Drago's
  block seg2:C00C at 08DC), whose non-zero result jumps back (seg2:0819) to the search
  before the month test seg2:0766. The first slot's own sit-out text comes after STAT, since
  its seg2:089D runs after seg2:07E1. *Saving:* no save is written in `Phase::Statement` or
  `Phase::Balance` (`SaveSlots::save` / `saveAuto` refuse, the save dialog plays Fate01); a
  save dialog opened from the recomp menu keeps its input over STAT. The sheet: `src/stat_sheet.cpp`;
  the window: `src/stat_view.cpp`, wired in `src/game_window_statement.cpp`
  (`--statement` shows it with the values of the started game). Save format 7 (`flow`), rules
  revision 46.
- **Not reproduced exactly / assumptions:** the window rectangle is centred on the
  rebuild's canvas like the other centred windows; the DIB's last two columns (560 − 558) are
  not shown in rally.exe either. Under seg1:00D2 a human's input during a computer's STAT is
  dropped; what seg1:01D7 does with them besides the end of a demo (seg2:5CAD, a sound through
  seg1:2C9B) was not traced further and is not rebuilt for STAT. The recomp display keys (F11,
  Alt+Return, F12) and F8 stay available while STAT is open.

### Victory sequence (round 46)

- **seg7:71C4** after the Master-Rallye: music "S" (DS:5E9A), the ranking
  seg1:8EE1 into DS:957A–957C (slots by cash + property value, +0x42 + +0x5A,
  sorted with adjacent swaps until none happens, highest first; one player in
  play: seg2:1A41), then seg7:7250(first + 1, second + 1, third + 1) with 0 for
  missing places (fewer than three / two players).
- **seg7:7250:** own window class, window SIEG (0x1F0 × 0x178); in WM_CREATE
  (seg7:7E7E) `%s\Sieg.bmp` (butt) and, for the language letters d / D, the
  banner `%s\s_bann_d.bmp` with colour key 0xFF at (3, 133); the files of
  DS:5E6A: sieg\many-l, many-r, konfetti, hutman, kapelle, zoepfe, intro\redet
  and `x_winner.sbs` with the letters of the three places, plus the winner's
  `ziel\%cZielCar.sbs`; the voice `%s\%c\VOICE\SIEG\SIEG.WAV`.
- **Records:** DS:557A many-l (0, 147), 5618 many-r (311, 144, delay 3), 56A8
  konfetti (365, 200), 5518 hutman (305, 195), 5828 car CAR1 (405, 260), 5724
  kapelle (8, 198), 57C6 zoepfe (298, 289), the winner (record 9: definition
  DS:5D6C[slot + 1], x = 0xE4 − DS:5D8E[slot + 1], y = 0xCA − height of its first
  frame) and DS:4800 redet (39, 239, start frame 1, delay 3). The second and
  third places are drawn as `<L>W00` (DS:5D9C) into the scene, centred at x 0xB9 /
  0x10F with their feet at y 0xDC / 0xE3 (seg7:79CB / 7A6C, seg7:1F52).
- **Loop** seg7:7BCD: 20 ms per pass like the finish; after 5000 passes the
  window gets VK_RETURN (seg7:7D57) and closes; then WM_COMMAND 0x424 ends the
  game. No random numbers.
- **Rebuilt** (`src/victory_view.cpp`, shared records in `src/scene.cpp`,
  `Game::ranking`): the scene after the finish of the Master-Rallye, then the
  title. **Input** (round 90, seg7:83EE): after the first pass (DS:9CDE,
  seg7:7CE9) WM_KEYDOWN with Escape, Space or Enter and WM_LBUTTONDOWN destroy the
  window; other keys and buttons do nothing. Centred by seg6:0000 (round 111), with the wooden border of seg6:1A68 (round 112).

### Sound mixing (round 132)

- **Device volume** seg5:2100 ff. (start): with Wave on and a wave device that has
  a volume control (WAVECAPS bit 2), `waveOutSetVolume(0, 0xFFFFFFFF)` (seg5:2164);
  with Midi on and MIDI device 1 able to (bit 0), `midiOutSetVolume(1, 0xFFFFFFFF)`
  (seg5:2194). Both full; there are no other volume calls and the MCI strings
  (DS:3357–34F9: `open sequencer!%s\midi\%s alias playerSnd`, `play … from %d
  notify`, `status … position`, `stop`, `close`, `cdaudio`) set no level.
- **WaveMix** seg1:2C10: MIXCONFIG { wSize 10, dwFlags 3, wChannels 1, wSamplingRate
  11 } — mono, 11 kHz; channel session opened with WaveMixOpenChannel(…, 0, 1),
  activated (seg1:2C82). The effect channels are summed by WaveMix without
  attenuation.
- **Voices** seg1:3579 → 3661 (with a wave device DS:9552 and Voice DS:B5D4): the WAV
  is read with mmio and played with its own `waveOutOpen` (seg1:3476) after
  `WaveMixActivate(FALSE)` (seg1:33E6); WaveMix is activated again when the voice
  has ended (seg1:3D86). While a voice sounds the effects are silent; the MIDI
  music goes on unchanged. The finish's `snd\z.wav` (seg7:2C48) takes this path too.
- **Order with messages:** the message window seg6:51EA is modal (seg6:0FB4); code
  after it — and its sounds — runs only when it has been closed (seg2:0EF1 plays
  moneywin, seg2:0F05 then opens the message). Rebuilt: every `Notice` gets a
  sequence number, every `SoundCue` the number of the next notice
  (`afterNotice`); the game window plays a cue only when all notices before it have
  been closed (`GameWindow::updateFrame`), instead of every cue of a command at once.
- **Focus** WM_ACTIVATEAPP seg4:0CAE: losing it minimises the window, pauses a voice
  (`waveOutPause`, seg4:0D4E) or deactivates WaveMix (seg4:0D6E) and calls
  seg1:293F (music); gaining it restarts the voice or activates WaveMix (0CD7 /
  0CEE) and calls seg1:299A.
- **Rebuilt** (`src/sound.cpp`, rule in `src/audio_mix.{hpp,cpp}` with
  `tests/audio_mix_test.cpp`): one 44.1 kHz stereo device; a voice flushes the
  effect channels and new effects are ignored until it ends; without the focus the
  effects stop and the voice pauses; music plays at 100 %, and one master volume
  (`[Audio] MasterVolume`, recomp) scales the whole output, since rally.exe offers
  only on / off switches. The effects are clipped to the sample range like
  WaveMix's sum; music, effects and voice — separate devices in rally.exe that met
  in the sound card's analogue mixer — are added and bent softly towards full scale
  above 3/4 instead of being cut (**assumption** about the analogue mix; the
  relative loudness of MIDI synthesis and samples depended on the hardware).

### Sounds (round 40)

- **Table** seg1:2E90 ff. (locals [bp−0x12E] − 4 · n): sound n opens
  `%s\fx\%s` (DS:3917) with WaveMixOpenWave into DS:9D3C[n]: 0 Plopp01,
  1 Fate03, 2 Plopp02, 3 Lachen01, 4 Hebel01, 5 Walze10, 6 Zaehler1, 7 Zaehler2,
  8 Taster01, 9 Carb01, 10–13 Hupe01/02/03/05, 14 moneywin, 15 Sfx08, 16 Elefant,
  17 Fate01, 18 Platsch, 19 Hebel05, 20 Schlaf01, 21–24 Gamble01–04, 25 Woelfe1,
  26 NHorn1, 27 Huhn1, 28 Kuehe1a, 29 Lachen02, 30 Schweine, 31 Baer1, 32 pf1,
  33 pf2, 34 w, 35–38 MEMO01–04 (the files are 8-bit mono 11,025 Hz PCM).
- **Play** seg1:2C9B(channel, sound, loops): only with a WaveMix session
  (DS:A0F4), Wave on (DS:B5D0) and DS:954E; sound < 0x27, channel ≤ 7;
  WaveMixPlay with flags 5 (channel 7: 0) and the loop count (0xFFFF endless).
  seg1:317D flushes a channel (WaveMixFlushChannel).
- **Call sites** (about 100): menus (seg5:5486 / 5534 Plopp02 on a choice,
  5523 Zaehler2 on a grayed item, 5561 / 55D3 Taster01 on moving), dialogs in
  seg1 / seg3 / seg8 (Plopp02, Zaehler2, Taster01), the slot machine (seg2:78B1
  Walze10 endless on channel 3; seg2:7C83 Zaehler1 on channel 4 when the count
  starts; seg2:7E74 flushes channel 3), money fields (seg2:0EF1 moneywin /
  seg2:1081 Fate03, channel 3), Zucker / Schlafen sit-outs (seg2:095B Sfx08,
  09F9 Schlaf01, channel 5), blocked cards (seg2:0B8D Sfx08), keys 1–4
  (seg6:0DA0: MEMO sounds), driving (seg2:1F75 Plopp01, 2082 / 21EB Carb01),
  Dr. Drago (seg2:BB73 / BB84 Lachen01 / 02), Risiko (seg7 Gamble01–04),
  animals in seg8:848C.
- **Rebuilt** (`src/sound.cpp`, cues in the simulation as `Game::takeSounds`):
  eight channels mixed with SDL audio (a new sound replaces the channel's),
  played only with Wave on; wired so far: menu sounds, the slot machine, money
  fields, the two sit-outs and blocked cards. Music (snd/midi) is not played.
- **Round 50:** the town dialog (procedure seg1:5337): Plopp02 for the tabs
  Kaufen / Verkaufen / Karte (seg1:5436 / 546D / 54B2); a purchase or sale is
  tried for the clicked row (seg1:5583 → seg1:7DC5 / 832A) and sounds Plopp02 when
  it succeeds, Zaehler2 when it is refused (seg1:5598 / 55D0, 5633 / 566B), so a
  grayed row now only sounds. The save dialog: Fate01 on channel 6 when saving
  fails (seg8:B4DC), Zaehler2 when loading fails (seg8:B3F9).
- **Round 51:** seg3 sounds: Weihnachten's card gift Hebel05 (seg3:1257);
  Nägel into water Platsch (seg3:1C99, rebuilt); the Flugzeug choice
  (seg3:3384) and the card shop (seg3:4643) Plopp02 / Zaehler2.

### Nägel direction (round 55)

- **seg3:1C96:** when the player's +0x2D is 2 (assumed: the vehicle on a sea
  route) the nails fall into the water: Platsch on channel 5 and text 5500, the
  card is used up. Otherwise the window TEMP 0x226 × 0x104 (CreateWindow
  seg3:1D27, procedure seg3:1E5E) on `%s\n.bmp` (butt) with text 510 opens; four
  triangles (seg3:1E72, client coordinates) up (0x78, 7), (0x87, 0x16), (0x69,
  0x16); right (0xDC, 0x69), (0xEB, 0x78), (0xDC, 0x87); down (0x69, 0xDC),
  (0x87, 0xDC), (0x78, 0xEB); left (6, 0x78), (0x15, 0x69), (0x15, 0x87) are
  tested with CreatePolygonRgn / PtInRegion (seg3:1F5D) and post VK_UP / RIGHT /
  DOWN / LEFT; then `%s\flc\karte1.flc` (an FLC film) and `%s\snd\Naegel.wav`.
- **Window** (round 96): WM_CREATE (seg3:223D) marks direction n (up, right, down,
  left) available (DS:320A–3210) when the player's field links there (field record
  +2 + 2n ≠ 0xFFFF) and the cell two steps away lacks the sea-route bit 0x04; the
  last available one is selected (DS:8C10). Without any the window fails
  (seg3:241C) and the card takes the water path (seg3:1E02: sound 0x12 on channel
  5, text 5500). Paint (seg3:24AD): n.bmp; the 0x60 × 0x60 map pixels from the
  player − 0x28 stretched to (0x19, 0x19, 0xC0, 0xC0) (seg3:2517); the available
  triangles in PALETTEINDEX(0), the selected in 0xED (black / red, seg3:27C8);
  text 510 in the client rect with left + 0xF0 inflated by (−10, −20), Arial 18
  weight 400, colour 0x45; text 114 (Ok) from top + 0xBE in weight 600, white,
  centred. Input (dispatch seg3:2769): no Escape, no right button — it cannot be
  cancelled; an arrow key or a click on a triangle selects an available direction
  (seg3:1FF2 ff.); Enter or a click on (0x118, 0xD2)–(0x1FE, 0xF0) confirms it
  (seg3:1FA4, seg3:2096).
- **Rebuilt** (`src/nails_view.cpp`, `Game::nailsDirections`, rules revision 36):
  all of the above. **Approximations:** the compass shows the world bitmap
  without the vehicles of the map DIB; the film follows as before; the window
  centred.
- **Water case** (round 75, rules revision 32): player record +0x2D is the
  vehicle state — 0 land, 1 turning into a boat (+0x2E counts up to 0x18,
  seg2:1793), 2 boat, 3 turning back (seg2:17EE, back to 0 at +0x2E = 0) —
  set when a seaway starts or ends (seg2:280F ff.). With state 2 seg3:1CD8 plays
  sound 0x12 on channel 5 and text 5500 and jumps to the common end (seg3:1E3D,
  WM_COMMAND 10000 spins); no direction is asked and no obstacle placed, the
  card is used. Rebuilt as `Game::nailsInWater`. *Correction (round 85):* +0x2D is a
  plane state, and a player at rest has it on sea fields, whose cells carry field +
  water (0x09; seaway cells between fields carry road + sea route, 0x06); the check
  of round 75 asked for the seaway bits and was practically never true. Rules
  revision 34.

### Card shop (round 52)

- **Window** seg3:4437: TEMP (558 × 462) with `%s\%c\bcards.sbs` (butt/<lang>:
  strips K1 / K2 558 × 42, card pictures 1CARD_01–41 of 44 × 63, transparent
  0xFF). Mode DS:9C82 (0 choose, 1 buy, 2 sell), choice DS:9C9A (0 Kaufen,
  1 Verkaufen, 2 Beenden), chosen card DS:A0EA, list DS:A0F6 (seg3:5813: offers,
  or the own hand in mode 2), count DS:A6EE.
- **Paint** seg3:4D0E: nine strips K1 (choosing) or K2 at y = (i + 1) · 42; the
  list's pictures `1card_%.2d` (type + 1) from (15, 51), 60 × 74 pixels per cell,
  nine per row; a red PS_INSIDEFRAME pen of width 2 around the chosen card.
  Header seg3:5234: KO_ZI while choosing, otherwise KO_Z; texts 321 Kaufen at
  x 18, 322 Verkaufen centred and 371 Beenden right-aligned to x 553, Arial 20
  weight 400; Verkaufen gray (0x43) without hand cards (DS:3FBA), the active
  mode 0x2C. Info line seg3:553C on KO_Z at y 420: text 12000 + type, three
  spaces and the price (value, halved by DS:8646 when selling) from x 10, the
  cash right-aligned to x 548, colour 0x45.
- **Input** seg3:4640: mouse in the header while choosing (x 0–0x6E, 0xB4–0x140,
  0x1CC–0x22E, seg3:5051) sets the choice and posts Enter; a click on a card
  chooses it (seg3:510C); Enter (seg3:4701): choosing → buy mode, sell mode (not
  with an empty hand) or close; buy / sell mode → seg3:47F1 / 4912 with the chosen
  card; Escape / Backspace (seg3:46A2): back to choosing, while choosing close;
  Left / Right move the choice or the chosen card. Plopp02 / Zaehler2 sound the
  steps (seg3:46B1 ff.).
- **Rebuilt** (`src/card_shop_view.cpp`): the window for a human player's stop on
  a card field; closing it ends the turn. **Input** (round 101, from
  `docs/original-card-shop-audit.md`, checked at seg3:4A41 / 46A2): a click only
  selects a card (seg3:5148), a double click (0x203) posts Enter; Left stops at
  Kaufen, Right wraps from Beenden to Kaufen; Up / Down move a whole grid row;
  Escape / Backspace (and the right button, seg3:4688) return silently from buy /
  sell to the header, from the header play Plopp02 and close; clicks outside the
  dialog do nothing. After selling the last card the view returns to the header
  (seg3:49E0 ff.). The header previews its choice: Left / Right rebuild the list
  with seg3:5813(choice) (seg3:4A79 / 4AEF) — Kaufen the offers, Verkaufen the
  hand, Beenden none (seg3:5997, count 0); opening builds it with 0 (seg3:452F),
  returning from buy / sell keeps the list (seg3:46E0). The window is centred by seg6:0000 (seg3:4C18, round 111).
- **Round 41:** driving: seg2:206B plays Carb01 when the steps taken equal the
  allowance (and DS:9C74 = 0), Plopp01 otherwise (channel 1); the window plays
  the game's cues when the drawn car has reached its field. Dr. Drago spending:
  the random(1) of seg2:BB5E chooses Lachen01 (0) or Lachen02 (1) on channel 5.
- **Pause** keys 1–4 (seg4:018B → seg6:0DA0(key − 1, 1)): DS:1642 = 0–3, saved as
  `[Setup] Pause` (seg6:0EBF; read at seg6:0D60, values > 3 become 0), and
  MEMO01–04 for 0–3 on channel 0. seg6:0EE0(ms) waits ms × DS:858E[DS:1642] =
  1.0 / 1.5 / 2.0 / 3.0 (seg6:0E3A); the rebuild stores the value but has no
  such waits yet.
- **Demo** (1024, seg4:05A8): seg4:0DA9(1) sets DS:14 = 1 (demo), skips the
  intro, and seg2:01AF makes slots 0, 2, 4, 6 computers (status 2) with the zoom
  byte +0x15 = 1, 0, 1, 0 instead of the player setup (seg1:02BA); then
  seg6:0DA0(1, 0) selects pause 1 without a memo. Escape or a right click while
  no human plays end it (seg1:021D, seg2:5CAD). Rebuilt with these slots and
  zooms; without a human the message window closes after the computer wait of
  seg6:0FB4 (2500 ms × the pause factor, 100 ms once the demo ends; see
  "Waiting", round 113, which replaced the former 2 s assumption).
- **Credits** (1026, seg4:070C): seg6:0A4D(…, "Credits.bmp" DS:9A, 1) shows
  `butt/<n>/Credits.bmp` in a window PICT over the canvas and waits (seg6:0FB4);
  any key or a left click closes it (round 94, PICT dispatch seg6:0C8E → seg6:0B80;
  other buttons go to DefWindowProc).

### Options (round 39)

- **Settings** seg5:1FA0 reads the profile section `Setup` (GetPrivateProfileInt
  via seg5:22C0, default 1): Midi → DS:B5D2, Wave → B5D0, Colorcycle → B5DC,
  Animations → B5DA, Views → B5D8, Voice → B5D4, TimeOut → B5D6, Subtitle →
  B5DF (default 0), Joystick → B5DE (cleared without a joystick).
- **Window** OPTION (seg1:ACE7, commands 1023 / 1063): background
  `%s\%d\%c\menu.bmp` (butt directory, language letter; the labels are part of
  the picture: Sound-F/X, Musik, Sprachausgabe, Zeitlimit, Informationen,
  Animationen, Wellen und Jahreszeiten, Joystick), star / stop of pl_menu at the
  player setup's places, and eight switches `%s\%d\op_menu\%d%s.bmp` (value,
  name) at the places of the setup's driver slots 0–7: switch i shows
  DS:B5D0 + 2i with the name i of the table DS:7E0A (far pointers to fx, midi,
  spra, zeit, seh, anim, farb, joy; the second words are fixup chains). A copy
  of the values is kept at DS:9478 (seg1:ADEE) for stop. op_menu holds two
  pictures per switch and a third for joy (`2joy.bmp`).
- **Rebuilt** (`src/settings.cpp`, `src/options_view.cpp`): the window, the
  switches (joy with three states) and the settings in
  `$XDG_CONFIG_HOME/dtvr-recomp/settings.ini` (`[Setup]`, the original keys).
  Applied so far: Views (tile 0x33 on the map) and Wave (round 40).
  **Input** (round 94, dispatch seg1:B677): WM_COMMAND 0x1392–0x1399 toggle the
  switches at once (seg1:B2E2 ff.; Wave without a device DS:954E only plays sound 7
  on channel 0); 0x139A star, 0x139B stop and 0xFFFF (Escape / Backspace on a
  focused SBBUTTON) all run seg1:B539, which closes the window and keeps the
  values — the copy at DS:9478 (seg1:ADEE) is never read back. WM_KEYDOWN
  (seg1:B629): F1–F8 send 0x1392 + n, F10 sends 0x139A. There is no mouse handler
  of its own (the switches are SBBUTTONs). Rebuilt in `src/options_view.cpp`; the
  former assumptions (Return accepts, stop / Escape discard) are removed.
  Applied by now: Wave, Midi, Views, Colorcycle (round 83), Joystick (round 79),
  Voice, TimeOut (round 102) and Animations (round 103, below).
- **Animations** (round 103): ambient map animations. seg8:7CFB (called when the
  map is set up) scans both layers of all 256 × 256 cells (x outer) for the first
  tile of a definition in the far-pointer table DS:7B62 (51 definitions: +0 tile,
  +2 layer, +4/+5 width/height in cells, +6 passes per frame, +7 repeats (0xFF
  endless), +8 pause range, +0xA sound, +0xC sound chance, +0xD frames — the first
  tile of each frame, 0-terminated, the cells of a frame row-major) and builds
  records of 0x13 bytes (+0 definition, +4/+6 position, +8/+0xA its far corner,
  +0xC pause, +0xE delay, +0xF repeats, +0x10 frame, +0x11 restart). seg8:7F7D
  puts the records inside the view into the list DS:A05E (and resets frames).
  seg8:83A0 (returns at once without DS:B5DA) advances each listed record: a pause
  counts down; on a restart the sound plays on channel 6 when random(chance) = 0;
  the delay counts down, then the next frame's tiles are written with seg6:273B
  (keeping the flag bits 0xF000) and the cells redrawn (seg6:2C0A); after the last
  frame a repeat starts it again, otherwise a pause random(range) follows.
  seg8:83A0 runs from the idle pass seg2:1C37 (every wait of seg6:0EE0 / 0F60 and
  the game loop, 20 ms plus drawing) and from the event scene passes (seg8:6E23)
  and starts (seg8:26DF, 4628, 4CD6, 5739); its random numbers therefore depend
  on how long the game waits and cannot be reproduced — the simulation keeps the
  sequence of a game without animations. *Round 114:* a record entering the
  list gets pause random(+8), delay 0, the definition's repeats, frame 0xFF and
  restart 1 (seg8:8277), so the objects start staggered (they all started
  together after loading). Rebuilt in `src/ambient.cpp` (table
  generated from the data segment) and `GameScreen::tickAmbient`: the listed
  objects advance every 20 ms on a copy of the map with their own random numbers.
  *Round 132 (replaces the former simplification that every object advanced):*
  seg8:7F7D, called only from the redraw seg2:05A8 (after centring, edge scrolling,
  turn starts and event scenes), first moves every listed record to its last frame
  (seg8:7FAB: frame up to the terminator, back by one) and writes its tiles, clears
  the list (seg8:8359), then lists the records (in scan order, at most 0x96,
  seg8:828B) whose rectangle — pixels: cell · 16 to (cell + size) · 16 (seg8:7EED,
  7F1E) — touches (DS:956A, DS:9568, DS:956A − DS:94DA + DS:94DE, DS:9568 − DS:94DC +
  DS:94E0) by seg2:42A4 (inclusive overlap). DS:956A / 9568 is the origin in *tiles*
  (seg6:3099 `sar 4`), DS:94DE / 94E0 the view in pixels: rally.exe mixes the units,
  so the list holds objects near the world's top left, not the visible ones.
  Measured with europe.map (1276 objects, 99 with a sound): every origin lists 42–50
  objects, none with a sound — the farm animals' sounds never play in rally.exe.
  Rebuilt exactly (`AmbientAnimations::setView`, tested).
- **Voice** (round 102): seg1:3579 always selects the sync.dat record but loads and
  plays the WAV only with a wave device (DS:9552) and [Setup] Voice (DS:B5D4),
  independent of Sound-F/X (seg1:3661); otherwise seg1:3E64's TimerCount path
  keeps the scene waiting for the voice's length. Rebuilt as
  `SoundPlayer::playVoice` (busy for the WAV's length when not played).
- **TimeOut** (round 102): the "n Felder" cards (types 29–34, seg3:3AB1 … 3ED0)
  call seg2:3ABD: with DS:B5D6 set, DS:9470 = (steps + 2) × 5, DS:12 = 1 and a
  1000 ms timer (seg8:030B, callback seg2:3B0E). The callback runs while the game
  is running (DS:94C4), the application is active (DS:94E6, WM_ACTIVATEAPP at
  seg4:02C9) and DS:12 is set: it counts DS:9470 down and at 0 plays sound 0x22 on
  channel 6; afterwards every second it subtracts 500 (DS:85FE) for the first ten
  seconds, 750 (DS:8606) for the next ten, then 1000 (DS:860E) from the driver's
  cash (seg2:5A47), plays sound 0x15 on channel 6 and calls seg5:607B(1) (not
  rebuilt). nextPlayer clears DS:12 (seg2:0716 → 3A8E). Rebuilt as
  `Game::timeLimitSecond`, driven by the game window while it has the input focus;
  the timer is not saved.

### Übersicht and Umfeld (round 37)

- **Übersicht** seg1:A240 (commands 1034 / 1041): window OUTLINE (DS:40BC) with
  the rectangle of the map window (DS:9C84..9C8A), holding capture and focus;
  the bitmap `%s\%d\map.bmp` (DS:40AE, butt directory: 432 / 544 / 688 pixels)
  at (0, 0). Scale = 4096 (DS:86DE) / bitmap width; a field's point is
  cell · 16 / scale, truncated, + 1 (seg1:A8C7). With CreatePen(PS_INSIDEFRAME,
  2, PALETTERGB(255, 0, 0)) the route is drawn from the current player's field
  (player +0x3E): distances from the destination DS:A10D with the mask
  (seg2:43A8), then always the first neighbour (up, right, down, left) with a
  smaller distance (MoveTo / LineTo until distance 0). Ellipses of radius 6
  with that pen: green brush PALETTERGB(0, 255, 0) at the stage start DS:A10B,
  blue PALETTERGB(0, 0, 255) at the route's end. SBS `Q_FL` at Dr. Drago's pixel
  position DS:9D01 / 9D03 ÷ scale when DS:9CFC ≠ 0xFF; `%c_FL` (DS:40D1; 15 /
  17 / 20 pixels in `<n>_assort.sbs`) at each driver's pixel position (+0x16 /
  +0x18) ÷ scale, starting after the current player so that its flag is drawn
  last.
- **Umfeld** seg6:3C94 (commands 1035 / 1043): capture and focus to the map,
  DS:169C = 1, target DS:96A2 / 96A4 = origin; a loop moves the origin
  DS:956A / 9568 to the target (seg6:2B19 keeps it inside the world) and
  redraws. A left click in the map window (seg6:1798, WM_LBUTTONDOWN; coordinates ÷
  (zoom DS:9C70 + 1); corrected in round 78, not a mouse move): in a strip of 0x3C pixels at an edge of the viewport DS:94DA..94E0
  (corner squares excluded) the target moves by (width ÷ ±3) ÷ 16 tiles in that
  direction. Keys (seg6:1991): arrows and numpad 2 / 4 / 6 / 8 move the same way;
  Enter, Escape and Space end it; a left click ends it (seg6:176E).
- **Rebuilt** (`src/overview_view.cpp`, Umfeld in `src/game_screen.cpp`).
  **Input** (round 101, from `docs/original-overview-audit.md`, checked at
  seg1:AA00 / AB34): Enter, a double click or (unzoomed) a left click toggle the
  zoom; while zoomed the arrows pan a 3 × 3 grid of half-size crops (steps
  (size >> 2) − 4) and a left click opens the MCTRL steering cross; Escape,
  Backspace and the right button close it. Q_FL marks Dr. Drago's own vehicle
  (round 140). **Assumption:** after
  Umfeld the view follows the current player again.

### Shut-in players (round 80)

- **Move start** seg2:61B7, called after every spin (seg2:66B9), the fixed movement
  cards (seg3:3A2B ff.) and Risiko (seg7:6BCE): r = seg5:99B3(player). When
  0 < r < DS:A778 the allowance becomes r with text 6003 (r = 1, no draw) or 6005
  + random(2) with `%Fs` and `%d` = r, then seg5:607B(5) and the move goes on.
  When r = 0 (or the allowance is 0) text 6009 + random(2), DS:A778 = 0, seg5:6CE8
  and nextPlayer seg2:06FC: the turn ends.
- **Reach** seg5:99B3 builds the graph with seg5:8B1B(buffer, 0) and runs
  seg5:9DE8(player): depth-first from the player's field (+0x3C) over the links
  0–3, the link back to the previous field removed in each new frame (seg5:A0C5);
  a link with nails / snow counts needs as many Nägel weg (card number 0x27) /
  Schneepflug (0x25) cards, counted with seg3:3FFC and used up along the path
  (the counts are cleared both ways and restored when backing out); a field that is
  already on the path returns 0x1E at once (a cycle), as does a depth of 0x1E;
  otherwise the deepest depth. Rebuilt as `reachableSteps` (src/ai.cpp, tested)
  and `Game::startMove`; rules revision 33. seg5:607B(5) is not rebuilt.

### Planes (round 85)

- **Cells** of the board fields (europe.pit, seg2:3260): 555 land fields carry field +
  road (0x03), 112 sea fields field + water (0x09), 33 only the field bit; the
  seaway cells between sea fields carry road + sea route (0x06).
- **Take-off** seg2:1EB5: at a step the animation is chosen by the cell next in the
  driving direction: road + sea route → mode 0 of table DS:1994 (`?fup` / `?fr` /
  `?fdown` / `?fl`, 48 × 32 biplane frames in `sbs/<letter>.sbs`), else mode 1
  (the car). seg2:2B30 sets the definition; one of the four plane definitions
  (DS:1FAE, 1F82, 1FE2, 1F5A) calls seg2:27EE: from state 0 or 3 the state +0x2D
  becomes 1 with height +0x2E = 0 (seg2:2656(3)). seg2:2318 keeps the plane while
  +0x2D ≠ 0.
- **Height** seg2:1793 / 17EE in the drawing pass: state 1 adds 1 << DS:34 (2) up to
  0x18, then state 2; state 3 subtracts it down to 0, then state 0 (seg2:2656(2)).
  The sprite's y is +0x18 − height + 2 (seg2:1878).
- **Landing** seg2:2866: while state 1 / 2 the cell three cells ahead in the driving
  direction is read; field + road (a land field) sets state 3 (seg2:29D1).
- **Rebuilt** in `src/vehicle_air.cpp` (tested) and `GameScreen::advanceVehicles`:
  per movement tick; vehicles placed directly start flying on sea fields.

### Films (round 83)

- **Files** `butt/flc/karte1.flc`–`karte4.flc`: Autodesk FLC (magic 0xAF12), 256 ×
  192, 8 bit; 76 / 9 / 135 / 27 frames at 100 / 71 / 71 / 67 ms; frame chunks
  (0xF1FA) with COLOR_256 (4), BYTE_RUN (15) in the first frame and DELTA_FLC (7)
  after it, plus the ring frame.
- **FLIC window** seg7:62AA(parent, x, y, file, loops): opens the film (seg7:51CB,
  mmio), creates the class FLIC (procedure seg7:63E1) of the film's size at (x, y)
  in the parent, starts a multimedia timer (timeSetEvent) and stores `loops` in
  DS:8D46; after the last frame DS:8D48 counts a pass and at `loops` passes
  WM_COMMAND 0x7EF goes to the parent (seg7:6619). The callers wait modally
  (seg6:0FB4 on DS:A050), close it (seg7:63A3) and stop channel 7 (seg1:317D).
- **Cards** (parent the canvas DS:40, x = y = 10, sound `%s\snd\<name>.wav` from
  the butt directory on channel 7 before, seg1:2D40): Zucker karte3 + Zucker.wav
  (seg3:1A25 / 1A72), Schlafen karte2 + Sab.wav ten times (seg3:1B4A / 1B97),
  Nägel karte1 + Naegel.wav after the direction window (seg3:1DA9 / 1DCE), Punch
  karte4 + Punch.wav (seg3:2EC2 / 2F17).
- **Rebuilt** in `src/flc.cpp` (decoder for chunks 4, 7, 11, 12, 13, 15, 16; tested
  with a synthetic film and malformed data; all four original films decode),
  `src/film_view.cpp` and `Game::takeFilms`; films queue, play modally and swallow
  input. The films are read at run time from the installation, not converted.

### Colour cycling (round 82)

- **Timer** seg5:01EB: SetTimer(…, 0x32 = 50 ms, seg5:3BB9). Each tick rotates the
  ten 4-byte entries at DS:9526 by one (entry 9 becomes entry 0, the others move up,
  seg5:3BCD ff.) and, while DS:13A4 ≠ 0, [Setup] Colorcycle (DS:B5DC) ≠ 0 and the
  main window is not iconic, seg5:2FB4(0x90, 0x0A, DS:9526) writes them to the VGA
  DAC directly (port 0x3C8 index, 0x3C9 data) as palette indices 0x90–0x99.
- **Colours** seg5:3AEE ff. fill DS:9526 from palette entries 0x90–0x99 (reduced
  to 6 bits with `shr 2` when the device gives 8-bit values). In the SBB tile sets
  these ten indices are the sea blues (about 6,500 pixels each per set, the
  neighbouring indices 0x9A–0x9F unused): the cycle makes the water ripple.
- **Seasons** with Colorcycle = 0 seg5:1929 fixes the tile directory to 1, so the
  map keeps the summer tiles ("Wellen und Jahreszeiten" switches both).
- **Rebuilt** in `GameScreen::cyclePalette` / `drawMap`: the world stays an
  indexed surface whose entries 0x90–0x99 rotate every 50 ms; only the visible area
  is converted into a streaming texture each frame. **Not rebuilt:** the hardware
  palette also affected sprites and windows using these indices; here only the map
  cycles.

### Message coverage (round 81)

- All 73 calls of the message window seg6:51EA were listed with their text id or
  random base (private helper `msgbases.py`) and compared with the rebuild: every
  language text shown by the original during a game is rebuilt (shut-in texts
  6003–6011 were the last gap, round 80). Not rebuilt: the English save / load
  errors stored in rally.exe itself (DS:7EE9 ff., e.g. "Error saving player",
  "Wrong structsize. File not compatible", seg8:ABE5 ff. / B08D ff.); the native
  save format reports its own errors on stderr.

### Mouse steering (round 78)

- **Map click** seg6:1798 (map window procedure from seg6:1681): outside Umfeld
  (DS:169C = 0) seg4:0F52(map, main, x, y) runs: only while DS:96A0 ≠ 0 (no event
  running) and no cross is open (DS:94E4 = 0) it creates the window class MCTRL
  (DS:B7, registered at seg5:0B60 with the procedure seg4:103B) of 0x40 × 0x40 at
  the click − 0x20 (MapWindowPoints to the main window), focus and modal loop
  seg6:0FB4.
- **MCTRL** seg4:103B: WM_CREATE captures the mouse and clips the cursor to the
  window (ClipCursor), state DS:8B92 = 0, DS:8B93 = 1. WM_MOUSEMOVE (seg4:10A3)
  sets DS:8B93: 1 inside (0x15, 0x15)–(0x29, 0x29); for y < 0x14: 5 when y > x,
  3 when 0x40 − y < x, else 2; otherwise with y' = 0x3F − y: 5 when y' > x, 3
  when 0x40 − y' < x, else 4. The paint copies DS:8B93 to DS:8B92 and draws the
  frame of the state from `sbs\assorted.sbs`: 1 → M0, or M1 when DS:9D40 =
  DS:A778 (the move is complete), 2 MT, 3 MR, 4 MD, 5 ML (names DS:C6 ff.).
  WM_LBUTTONUP (seg4:11B3) destroys the window, sets DS:86 = 1 and posts
  WM_KEYDOWN with table[DS:8B92] = {0, Return, Up, Right, Down, Left} to the
  main window; WM_RBUTTONDOWN destroys it without a key; WM_DESTROY releases
  capture and clip.
- **Rebuilt** in `src/mouse_control_view.cpp` (regions tested) and the game
  window: a left click on the map in a human player's move or after arriving
  opens the cross; the key goes through the same path as the keyboard.
  M1 is shown when the move is complete (DS:9D40 = DS:A778, `Game::arrivalDue`,
  round 137) or after arriving. **Assumption:** the cursor is not confined,
  positions outside count as the nearest edge.

### Joystick (round 79)

- **Game port** seg10:0010(n): with interrupts off, writes port 0x201 and counts
  the axis bits (joystick 0: bits 0 / 1, joystick 1: bits 2 / 3) up to 0x7530
  loops into DS:A7C4 (x) / A7C6 (y); the buttons are bits 4–5 (6–7) of the port,
  inverted, in DS:9ADE (bit 0 button 1, bit 1 button 2). seg10:00BF detects the
  sticks.
- **Direction** seg2:8C2B(n): two readings that both pass the calibrated limits
  (table DS:32E2[n]: +8 / +0xA x, +0x12 / +0x14 y) give VK_UP, VK_DOWN, VK_LEFT
  or VK_RIGHT in this order of checks, else 0, in DS:9698[n]; buttons in
  DS:A0DE[n].
- **Tick** seg2:8A30 (timer procedure; SetTimer(…, 0x50 ms) at seg2:8018 after the
  calibration seg2:8252, called from seg2:7F72 on the key C when [Setup]
  Joystick is set): only while a window has the focus and DS:94E6 ≠ 0; joystick
  1 when DS:A126 bit 0 is set (joystick 2 is never read: an unconditional jump
  at seg2:8A83). Per joystick: no direction after one → repeat counter DS:A0E2 =
  0 and the steering direction DS:A77A = 0; a new direction → WM_KEYDOWN of it to
  the focused window at once; the same → after three ticks every tick; button 1
  set and the buttons changed → VK_RETURN, button 2 → VK_ESCAPE. The JOY window
  seg2:8D81 is the calibration dialog (pictures DS:32F2 / 32FD; Escape, Return,
  Space close it).
- **Rebuilt** in `src/controller.cpp`: the SDL game controllers (Xbox and
  other pads; also hot-plugged; all of them merged, see the recomp pad layer) are joystick 1 while [Setup] Joystick ≠ 0: d-pad or
  left stick give the direction, A is button 1 (Return), B button 2 (Escape).
  The repetition is the tested `JoystickRepeater`; its keys are pushed into the
  SDL queue so menus, dialogs and driving receive them like the keyboard, and a
  released direction sends key-up (clears the held arrow, DS:A77A). **Not
  rebuilt / assumptions:** the calibration dialog (SDL reports calibrated axes;
  the stick limit is half the range), the two-stick mode.
- **Evidence for the key details:** the PostMessage calls of seg2:8A30 (seg2:8B44,
  8B84, 8BBC, 8BF4) send WM_KEYDOWN (0x100) with lParam 0 to the GetFocus window
  (seg2:8A35, skipped when it is 0, seg2:8A49): no repeat bit, no WM_KEYUP; only
  the centring clears DS:A77A (seg2:8B05). The Risiko window's WM_KEYDOWN
  (seg7:718A → 6C0D) reads only wParam ([bp+0xA]) and the pending flags DS:A29E /
  A0EE, so a held joystick direction presses CHANCE every 80 ms after the three
  waiting ticks in the original too; the rebuild keeps that. A diagonal is
  vertical (check order of seg2:8C2B), for the d-pad and the stick alike.

#### Recomp pad layer (round 1 "pad core", `src/pad_input.cpp`)

RECOMP ADDITION, no counterpart in rally.exe. `PadInput` runs the unchanged
`JoystickRepeater` and adds, each on a new press (edge) in the same 80 ms tick:
Start → the recomp menu (Pause page, toggle like F8, not F8 itself, which is a
switch key in OPTION / PLAYER; in OPTION the page Grafik like its own link; not over
film, intro, sights, the steering cross or the quit question); View/Back →
Backspace only where it reaches RIGHTBTN (the map with no window, not in Risiko or
the town dialog) or while RIGHTBTN is open (its checked Zurück), otherwise nothing;
X → Backspace; LB / RB → Shift+Tab (KMOD_LSHIFT) / Tab; Y → WM_COMMAND 1031 "Karte
einsetzen" while ONFIELD has the input (its item has flags 0 in rally_d.dat, never
grayed), F9 in PLAYER, otherwise nothing; A → Space instead of Return while PLAYER
or OPTION has the input (SBBUTTON: Return is Start / nothing there). The right stick
posts arrow key-downs (the original repetition, no key-ups) only while Umfeld is
open and no other pad key falls in the same tick (an Escape / Return of that tick
closes Umfeld). These steps are no SDL key events: `PadOutput::scroll` goes straight to
`GameScreen::handleSurroundings` in `updatePad` while Umfeld is open, so they can never
reach the map's keys or become a driving direction.
The held driving arrow is cleared by its key-up before any window takes the event
(quit question, film, intro, sights, MCTRL, recomp menu), on focus loss and when
Umfeld closes. The context comes from `GameWindow::updatePad` (src/game_window_pad.cpp).
Hazard rules (recomp): every connected SDL game controller is merged into joystick 1
(directions by the seg2:8C2B order over all pads, buttons ORed), so any pad may act;
when the pad stops counting (last pad removed, [Setup] Joystick off, window not
focused) the held direction gets its key-up at once; buttons held when the pad
starts counting or when pads are added / removed are ignored until released. The
[Setup] Joystick gate applies to both layers: with the option off the pad does
nothing at all.

### Start and Blue Byte logo

- **Order in seg5:0000** (the start-up returning 1 on success): … seg5:1BBB creates
  the popup `Rally` DS:42 and the canvas DS:40 (style 0x46000000, centred) and shows
  both (seg5:1C76 / 1C81); the canvas WM_CREATE loads `butt\<n>\bk.bmp` (seg8:001A,
  DS:17E2) and its WM_PAINT (seg8:01B7) copies it under every paint rectangle. Then
  `%s\assorted.sbs` (DS:E8, seg6:3E38; failure → error message seg6:0639, return),
  seg2:7F72, **seg6:0A4D("BB.bmp" DS:F8, 0)** at seg5:0151, seg1:31C0, the MIDI cue
  `I` (DS:FF, seg1:2A30), `[Setup] ShowIntro` (DS:101 / 10B, default 1, seg5:22C0 →
  DS:9562); with ShowIntro the intro seg7:0000 and **seg6:0A4D("BB.bmp" DS:111, 0)**
  again at seg5:0193; then seg5:3A4D, the board loader seg5:14AB, seg8:7CFC, seg3:0000
  (deck), seg5:1C95, seg1:2B9A, the cursor from before back (seg5:0207; IDC_WAIT 0x7F02 since
  seg5:001B, so the hourglass shows over the logo), **seg6:0B37**
  (seg5:020C) and the title seg8:03AB (seg5:0211).
- **seg6:0A4D(name, focus)**: wsprintf `%s\%d\%s` (DS:165C) with the butt directory
  DS:7A, the art directory of seg5:1827 (6 / 8 / 1) and the name: `butt/<n>/BB.bmp`
  (the installation has `bb.bmp` 175 × 313 in 6, 219 × 391 in 8, 280 × 500 in 1,
  8-bit). The DIB is read by seg5:3E04 into DS:163E; without it the function
  returns. CreateWindow class `PICT` (DS:166A), title DS:1665, style 0x44000000
  (WS_CHILD | WS_CLIPSIBLINGS, no border), x = y = 0, width / height = biWidth /
  biHeight, parent the canvas DS:40 → DS:163C; BringWindowToTop, ShowWindow(5),
  then seg6:03F7 dispatches the waiting messages until none is left (seg6:0B05; while
  the application is inactive, DS:94E6 = 0, it first waits with WaitMessage). With
  focus 0 nothing more happens: no SetFocus and no modal wait (seg6:0B11 → seg6:0FB4
  only with focus ≠ 0, as for Credits). **The logo has no timer**: it stays while
  seg5:0000 goes on loading.
- **PICT window procedure** seg6:0B66: WM_CREATE (seg6:0B8E) centres it with
  seg6:0000 on the canvas (half the size difference, C division, not below 0);
  WM_ERASEBKGND returns 1; WM_PAINT (seg6:0BD3) selects and realises the game
  palette DS:A130 and seg6:0938 copies the DIB with key 0 (seg5:4FC6) into a WinG
  bitmap and WinGBitBlt's it to (0, 0); WM_KEYDOWN and WM_LBUTTONDOWN destroy the
  window (seg6:0B80); WM_DESTROY frees the DIB and clears DS:163C; 0x30F / 0x311 go
  to seg5:3941. seg6:0B37 destroys DS:163C when it is set.
- **Removal**: the intro calls seg6:0B37 at seg7:044D, after its CreateWindow (whose
  WM_CREATE loads club.bmp and the SBS files) and the record set-up, right before it
  shows its own window; the set-up calls it at seg5:020C before the title.
- **Colours**: the game palette is the resource `PAL` of type `PALETTE`
  (DS:1418 / 1420) of `dat\<lang>\rally_<lang>.dat`, 256 RGB triples (seg5:2C33).
  `bb.bmp` has no pixel of colour 0 (the blit is opaque); its colour table and the
  game palette differ in 18 entries, of which the logo's pixels use 16, 17 and 254 in
  all three sizes (pixel counts 6: 1 / 122 / 16, 8: 10 / 188 / 31, 1: 11 / 364 / 36;
  the row padding bytes of the 175-wide `butt/6` file are 162, not pixels), by at
  most 7, 4 and 2 per channel (e.g. 254: (0x61, 0x7A, 0xF2) in the bitmap, (0x63,
  0x7B, 0xF3) in `PAL`); `bk.bmp` differs from `PAL` in the same 18 entries.
- **Rebuilt** in `src/logo_view.cpp` (tested in `tests/logo_view_test.cpp`):
  runGameWindow presents bk.bmp with the centred logo right after the renderer exists,
  then loads the language file, the art, the audio and (in start) the intro's files;
  start removes it (seg7:044D / seg5:020C). **Recomp change (user decision
  2026-11-25): the second logo after the intro (seg5:0193) is not shown**; when the
  intro ends the title opens at once (the logo is already removed). `--logo` (tests) keeps the logo
  until a key or left click. The intro is now drawn over bk.bmp as well (the canvas
  is visible and painted while INTRO runs), not over black.
- **Recomp addition: minimum display time** (user decision, 2026-11-24). rally.exe
  has no timer; the logo stayed for its loading time, seconds on 1995 machines, while
  the rebuild loads in about 0.2–0.3 s (measured with the dummy video driver). After
  loading, GameWindow::waitLogo keeps the logo until `kLogoMinimumMs` = 2500 ms after
  its show() (`src/logo_view.hpp`), running an event loop meanwhile: window, focus
  and display keys (F11, Alt+Enter, F12) work, a closed window quits. A key press
  (controller buttons arrive as Return / Escape; a key-down of Shift, Ctrl, Alt or GUI
  alone does not count, so Alt+Enter switches fullscreen without ending the logo, and
  in rally.exe Alt arrives as WM_SYSKEYDOWN, which PICT does not handle) or a left
  click ends the wait, as
  WM_KEYDOWN / WM_LBUTTONDOWN of PICT destroy it (seg6:0B80); the rest of that input
  (key repeats and release, the button release) is dropped, so it does not reach the
  intro or title. rally.exe's second logo after the intro is the same call (seg6:0A4D with
  DS:111, focus 0, same window procedure); the rebuild does not show it. Smoke tests without
  `--logo` skip the wait to keep their frame counts.
  **Differences:** the bitmap's own colour table is used (as for all bitmaps of the
  rebuild), not `PAL`; in rally.exe PICT never has the focus (focus 0, no SetFocus),
  so there a key would reach another window and only a click would remove the logo,
  and only when a message pump runs (seg6:0B05 and the loaders' own pumps, not
  checked); the hourglass cursor (IDC_WAIT, seg5:001B … 0207) is not rebuilt.

### Intro (round 89)

- **Start:** seg5:0174 reads `[Setup] ShowIntro` (DS:101 / 10B, default 1) and
  runs seg7:0000 before the title. Window class INTRO (DS:1262), 0x1F0 × 0x178,
  child of the canvas DS:40, with a 480 × 360 DIB; with subtitles on it is moved
  up 22 px (seg7:0476, seg6:160B(hwnd, 0, −22)).
- **WM_CREATE** seg7:2176: backdrop `butt\club.bmp` (DS:1388) and the SBS files of
  the table DS:1240 under `butt\sbs\intro` (zoom, paar2, redet, tuer, leute2, bild,
  kopf, lampe, leute1, paar1, red_foot, fate, fate_hut, wall1, wall2, sm_rlyy).
  club.bmp, clubfoto.bmp and `d\zeitung.bmp` share one palette.
- **Input** seg7:2118: Escape, Space or Enter close the window once the first pass
  ran (DS:1198); a left click (WM_LBUTTONDOWN, seg7:25F6) closes it with DS:B5E4
  and DS:1198; other buttons go to DefWindowProc.
- **Loop** seg7:04D6, one pass per 20 ms: pass counter + 1 (cap 30,000),
  subtitle poll (see Subtitles), the state logic, then — while state < 11 or
  pass < 105 — draw the records (seg2:74E4), step them (seg2:6DCB), show the DIB.
  Records with a follow-up definition take it and clear +4 (seg2:6F01).
- **Voice helpers:** seg1:3E64 playing while elapsed ms × 11 ≤ length (TimerCount
  path); seg1:4071 the position lies in one of the ten talk intervals of sync.dat
  +0x18; seg1:414C the cue at +0xCC is non-zero and below the position.
- **Stamps:** seg7:1E3C keyed-blits every frame of a file into the backdrop (at
  start paar2 (0x165, 0x9E), bild (0x19D, 0x3A), leute1 (0x81, 0xB0), paar1
  (0x165, 0x9E)); seg7:1F52 one frame (tu00 at (0x22, 0x70) on entering state 7).
- **States** (definitions in the data segment, e.g. DS:0614 = tu00 × 9, tu09–tu17,
  delay 4): 0 door closed, after 250 passes the door opens; 90 passes later the
  head kop appears, 10 passes later it speaks RALLY01b (frame reset while in a
  talk interval); then the show master zooms in (zo00–zo17 moving from (0x41,
  0x69) by y = p · 50 / 84 + 0x69, x = 0x41 − p) to TUERAUF1 / TUERZU, the picture
  falls (pic23–29) and the lamp swings; state 7 RALLY02 with the speaker (rd),
  8 RALLY04, 9 RALLY05 (talking resets the speaker to frame 2), 10 RALLY07a with
  the wall pieces and the hat walking right to x 0x158, then left and up until
  Fate (fate1) falls behind wall2; at the cue the show master rises (sm_rlyy1–5,
  then 5/6). State 11 (all cheer): at pass 105 subtitles close, BLITZ01 plays,
  the palette goes to white (seg5:3483: +0x1E per channel and 15 ms, entries
  1–0xFE), the backdrop becomes `clubfoto.bmp`, all records are cleared, the
  palette returns (seg5:3623: −0x1E per 15 ms); at pass 305 `butt\<lang>\zeitung.bmp`
  is revealed by seg8:0E20 mode 4; at pass 555 the window closes.
- **Wipe seg8:0E20** (hwnd, mode): modes 0–5 (table seg8:160F), > 5 picks
  rand() % 5. Mode 4 reveals the 400 cells of a 20 × 20 grid: a random free cell
  (two rand() % 20) whose row is at most i / 20 + 2, rectangle (x·W/20, y·H/20,
  (x+1)·W/20, (y+1)·W/20) — the bottom uses the width; 20 ms before every 16th cell.
- Record field +0x36 is cleared for records 2 and 5 when Fate falls; the drawing
  code (seg2:74E4) does not read it (+0x2E..+0x35 is the dirty rectangle).
- Rebuilt in `src/intro_script.cpp` (no SDL, tested; the definitions compared
  with the data segment) and `src/intro_view.cpp` (8-bit scene composed in
  software, palette fades on it). The wipe draws from the session's generator,
  seeded before the intro (round 143, see "Random numbers"). **Assumptions:** the fade back steps each channel by 0x1E towards the bitmap colour; a
  close stops the voice; the window is centred like the victory.

### Subtitles (round 88)

- **sync.dat** (`dat\<lang>\sync.dat`, 29,120 bytes = 140 records of 0xD0, read to
  DS:9694): +0x00 WAV name (NUL-terminated within 0x14 bytes), +0x14 length in
  bytes (uint32), +0x18 ten talk intervals {start, end} (seg1:4071), +0x68 ten
  entries {uint16 string id, uint32 start, uint32 end}, +0xCC a cue (seg1:414C;
  the intro). The voices are 11,025 Hz
  8-bit mono (fmt of `zielsuch\part01a.wav`), so positions are bytes = samples.
  172 ids (40000–42306) are all present in rally_d.dat. Records cover the voice
  folders zielsuch (68), bilanz (58), intro (9) and sieg (1).
- **Position** seg1:3F46 for the current record DS:3E46: with voice output the
  waveOutGetPosition bytes (TIME_BYTES) minus the start DS:A12A; otherwise
  (TOOLHELP TimerCount − start DS:3E48) × 11, 0 once beyond the record's length.
- **Lookup** seg1:41EF: 0 without a record or at position 0; else the first of the
  ten entries with start ≤ position ≤ end (both inclusive), else 0.
- **Window** SUBTITLE (DS:3AE0): seg6:64A5(rect, font size) creates it only with
  `[Setup] Subtitle` (DS:B5DF) as a child of DS:42 at the rectangle; seg6:6596(hwnd,
  id) loads the string (LoadString, 0x200 bytes) or clears it for id 0; seg6:6554
  destroys it. Painting seg6:661F: `butt\unipaper.bmp` stretched over the window
  (seg2:913A), font seg6:04F9(14, weight 600), DrawText 0x110 in the client
  rectangle inflated by (−6, −6), PALETTEINDEX(0) without shadow.
- **Callers:** INTRO (seg7:04CA, window DS:9502), ZIEL (seg7:3B2B, DS:A04E), Sieg
  (seg7:7434, DS:A7B6) and BILANZ (seg8:8C3E, DS:A132), each with the owner's
  GetWindowRect: top = bottom − 0x10, bottom = top + 0x3C, size 14. Every pass of
  their loops (seg7:04F2) remembers the last two ids: a new non-zero id different
  from the last one set is shown; the window is cleared when the id has been 0
  for two passes after a non-zero one; the last id set stays remembered.
- **Forced on:** without a wave device (seg1:2BFA after WaveMixGetInfo, seg1:2C57
  after WaveMixConfigureInit fails) and for language letter E / e (seg5:21BA,
  which also turns the voice off).
- Rebuilt in `src/voice_sync.cpp` (no SDL, tested) and `src/subtitle_view.cpp`
  for ZIEL, BILANZ and Sieg (the intro since round 89); forced on when the game
  runs without audio. **Assumption:** the position uses the elapsed time (the
  TimerCount path) also with voice output.

### Sights (round 87)

- **Stop:** seg2:1DE2 sets a stop flag when the vehicle's cell has europe.pit bit
  0x10 (the 40 view point cells, tile 0x33) and `[Setup] Views` (DS:B5D8) is on;
  seg2:1E35 then halts like at a field unless a direction is held (DS:A77A). No
  view point lies on a field cell (field cells carry 0x01, 0x03 or 0x09 only).
- **Opening:** Enter on the stopped vehicle runs seg2:0BC3, which at seg2:1677 tests
  bit 0x10 again and calls seg6:4DCB(cell x, cell y). That loads
  `butt\views\%d_%d.bmp` (DS:18A1 / 18AB, 40 files of 180 × 180) and
  `butt\<lang>\views\%d_%d.txt` (DS:18B7 / 18C1) and creates the window class
  `Tourism` (DS:18D0) as a child of the canvas DS:40 at (0, 0), 0x200 × 0xE8.
- **Painting** (seg6:4F5E): `butt\unipaper.bmp` (DS:18E0) at (0, 0), the picture at
  (10, 10), the text with DrawText 0x110 (DT_WORDBREAK | DT_NOCLIP) in the client
  rectangle with left + 200 inflated by (−10, −5), font seg6:04F9(15, 600), colour
  0x45. The texts are code page 1252 (as shown by the umlauts).
- **Second window:** seg6:47BE opens a `Tourism` window from command 1003 of the
  menu TOWNINFO (Vorherige Seite 1000, Nächste Seite 1001, Text ausblenden 1002,
  Beenden 1003) and toggles it (DS:94EA); 1000 / 1001 step the page DS:9524
  (seg6:4951, seg6:48A6). *Round 88:* nothing sends 1003 — no `push 0x3EB`, the
  only accelerator of rally.exe maps Escape to 101, and the TOWNINFO menu exists
  only while the window is open — so this window is unreachable in this build
  and is not rebuilt.
- Rebuilt in `src/sight_view.cpp`; the stop is presentation in
  `GameScreen::advanceVehicles`. **Input** (round 90, seg6:518B): WM_KEYDOWN and
  WM_LBUTTONDOWN destroy the window, other messages go to DefWindowProc; the
  window is modal (seg6:0FB4 on DS:A04A).
- **Passing** (round 95, seg2:1E08..1E31): at a view point a human's vehicle drives
  on only when the cell ahead in the driving direction DS:A77A has the properties
  exactly 0x20 (one of the 15 "open" road cells); computers always drive on
  (seg2:5C02). Otherwise it stops like at a field (seg2:1E35); an arrow key starts
  it again. Replaces the assumption that a held arrow key passes the point.

### Player setup (round 36)

- **Neues Spiel** seg4:0DA9(0): DS:A104 = DS:A106 = 99 (seg4:0E23), seg2:0000
  with the window PLAYER (seg1:02BD, procedure seg1:0A60), then — only without the
  demo flag — the stage count window TEMP seg8:0747 (seg4:0E54, see "Stage count
  window"). PLAYER lies
  over the canvas: background `%s\%d\pl_menu\menu.bmp` (640 × 480 at 640);
  buttons (class SBBUTTON) with ids 0x1392 + slot showing
  `%s\%d\pl_menu\%c%d.bmp` (driver letter of seg2:2D43, status 0 free / 1 human /
  2 computer), star.bmp (id 0x139A, start) and stop.bmp (id 0x139B, cancel), and
  input fields (class SBINPUT) for the names.
- **Geometry** DS:1BD4: (x, y, w, h), ten per resolution (1024, 800, 640):
  slots 7 down to 0 (seg1:0696 counts the slot down while the table index rises),
  then star and stop; at 640 slot 0 is at (61, 73), slot 7 at (334, 317), star
  (491, 420), stop (26, 420). DS:1CC4: the name fields, eight per resolution in
  the same order (640: slot 0 at (148, 73, 160, 26)).
- **Input** seg1:0A6E: a driver button steps its status 0 → 1 → 2 → 0 and
  changes the picture; afterwards the star is enabled only when a slot is in
  play (seg2:0368). Star: the names are read with GetWindowText(…, 0x13), i.e.
  up to 18 characters, and the window closes; stop or 0xFFFF closes it (cancel,
  back to the title); Enter acts as star when a slot is in play (seg1:0C3B).
- **Rebuilt** (`src/setup_view.cpp`, geometry in `src/layout.cpp`): the window
  with these pictures and positions; the start creates a new game with the
  chosen slots and names and a time-based seed (the original seeds once at
  seg1:02A9 with the time) and hands it over into the running Game object
  through the save codec. The names are drawn as SBINPUT does (below; since
  round 109). **Assumptions:** a bar as caret; a disabled star is drawn
  unchanged; Escape cancels from a button (WM_COMMAND 0xFFFF, seg1:0F77) but does nothing in a name field (round R8); non-ASCII characters pass as letters.
- **[Player] P0–P7** (round 142): seg1:02CE ff. reads the slots with
  GetPrivateProfileInt (seg5:22C0) from `rally.ini`, defaults P0 = 1, P1 = 2, others
  0, into the player records +0x00 before PLAYER opens. The window cycles a record's
  status in place (seg1:0A8A `inc`, above 2 back to 0 at seg1:0AA9), so there is no
  copy to restore: after the modal loop seg1:041E writes all eight back with
  `P%d` (DS:1DE2) and `%d` (seg5:22FC → WritePrivateProfileString), whether the
  window was started or cancelled. Names are not stored there. Rebuilt in
  `Settings::players` ([Player] of the recomp's settings file) for the menu's
  Neues Spiel (1020); values outside 0–2 read as the default (rebuild decision).
- **SBINPUT** (round 108): the class lives in rally.exe (registered at seg1:0E57,
  procedure seg1:1C71). WM_CREATE: Times New Roman (DS:32D2), height −20 / −17 /
  −15 by seg6:12FA, weight 600, *italic*, family 0x20 (seg1:1D04); length limit
  0x13 (window word +0xC). WM_PAINT: TextOut at (0, 0), transparent, no text
  colour (black), caret after the text. WM_CHAR accepts IsCharAlphaNumeric or a
  space while below the limit (seg1:1F31). WM_KEYDOWN (dispatch seg1:21E5):
  Backspace deletes; Tab goes to the adjacent sibling, with Shift GW_HWNDNEXT, else
  GW_HWNDPREV (seg1:209E), Enter to GW_HWNDPREV (seg1:2144), each only to an id in
  0x1388–0x1770 with sound 8; *correction (round R8):* Left / Right / Up / Down call
  the SBBUTTON walks seg1:250A / 2474 / 267F / 25A0 (seg1:21AA..21D4), they do not
  move a caret; only Enter is posted on to the parent (seg1:2190); other keys,
  Escape too, do nothing. Rebuilt in `src/setup_view.cpp` with the fields as
  `SbButtonGroup` entries.

### Stage count window (round 108)

- seg8:0747 registers class TEMP (procedure seg8:08D6), loads
  `%s\%d\p1.bmp` (DS:180A, paper) and `%s\%d\sm.bmp` (DS:1817, portrait) and
  creates a child of the canvas DS:40 at (0, 0), 0x186 wide and sm height + 0x24
  high; it runs modally (seg6:0FB4).
- WM_CREATE: an SBINPUT field (id 0x139C) at seg6:1140(8) (640: (185, 60)),
  0xBE × 0x5A; its text is GetPrivateProfileInt("Default", "Rounds", 99), 99 when
  outside 0..300; it gets the focus.
- WM_PAINT: p1.bmp at (0, 0), sm.bmp at (10, 10) (client rect inflated by −10),
  text 810 "Wieviele Etappen wollt Ihr spielen (max. 300)?" right of it (left +
  sm width + 10) with seg6:04F9(18 / 16 / 14, weight 600), DT_WORDBREAK |
  DT_NOCLIP, colour 0x45.
- WM_KEYDOWN (only Enter arrives from SBINPUT) and WM_LBUTTONDOWN on the window
  (seg8:0A0D): GetWindowText(…, 5), atoi; ≤ 0 or > 300 plays Zaehler2 and the
  window stays, otherwise [Default] Rounds is written, DS:A104 = DS:A106 = the
  count and the window is destroyed. There is no cancel.
- **Rebuilt** `src/stages_view.cpp` (profile value in `[Default] Rounds` of
  settings.ini). **Recomp addition** (not in rally.exe, always shown, as the user
  asked): a slider 1–300 on a 40 pixel taller window cut from the same paper;
  field and slider follow each other.

### Title menu and autosave (round 35)

- **Title** seg8:03AB: window TITEL (DS:17EF) over the whole canvas with the
  bitmap `%s\%d\%c\t.bmp` (DS:17FB: butt directory, art directory, language
  letter); seg8:0DB0 opens the menu MAIN at (0, 0), i.e. centred, and reads the
  profile value `Save` / `AS` (seg5:22C0): 0 grays "Spiel fortsetzen" (position
  1, seg6:156D: EnableMenuItem MF_BYPOSITION | MF_GRAYED), otherwise it is
  enabled (seg6:15BC).
- **Commands** (seg4:0B76): 1020 Neues Spiel → seg8:045D (title off), menu off,
  seg4:0DA9(0) (game setup; 0 → back to the title); 1024 Demo → seg4:0DA9(1);
  1022 Spiel fortsetzen → seg8:A8DF(0) loads `%s\as.dat` (DS:7EDF), then the
  game starts (seg5:186A(1), seg8:6709, seg2:0407, seg1:275E, seg5:6BF2,
  ONFIELD); 1021 Spiel laden → the save dialog in load mode seg8:B157(0), same
  start on success; failures return to the title.
- **Autosave** seg2:0837 → seg8:A870 at every turn start while a human plays
  (seg2:5C5A returns 0 when a slot has status 1): `%s\as` and profile `Save` /
  `AS` = 1 (seg5:22FC).
- **Rebuilt** (`src/game_screen.cpp`, `src/save_slots.cpp`): `--game` starts with
  the title (`--no-title` skips it); Neues Spiel starts the game set up on the
  command line (round 36: through the player setup PLAYER);
  Spiel laden uses the save dialog with text 801; the autosave is `as.sav` in
  the save directory, written at each turn start (turn counter
  `Game::turnSerial`, not saved), and its presence enables Spiel fortsetzen.
  Optionen, Demo and Credits return to the title for now.

### Statistik (round 34)

- **Call** 1036 / 1042 → seg1:4DE5: when the current player stands on a field
  (+0x3C ≠ 0xFF), seg1:4EA8(1) opens the ESTATES window in its statistics
  variant (bit 0x10 in DS:3FC2), then seg5:607B(−1). Start: category DS:8C48 = 0,
  DS:8C4A = argument + 1, scroll 0, list seg1:4504.
- **List** seg1:4504(category, player, with rented): all towns × ten entries with
  a name, owner = player, landlord free unless the third argument is set, and,
  for a category ≠ 0, entry +0x1E = category. *Round 107:* the sort at seg1:4735
  walks the list and swaps two neighbours only when they are out of order, then
  steps back (a stable insertion sort): DS:8C4A 0 by value (entry +0x18)
  ascending, 1 by rate (+0x1C) descending, 2 by town name with lstrcmp
  (seg1:4886) ascending. The names go to lstrcmp in the OEM code page (town record
  +6, no OemToAnsi). Windows 3.x lstrcmp (Microsoft KB Q100366, Q96748) compares
  primary weights first — non-alphanumeric characters in ANSI order, then digits,
  then letters without case — and the case only on a tie. The OEM umlauts are no
  letters in ANSI (ö 0x94 a quotation mark, ü 0x81 undefined), so Köln sorts
  before Kattowitz and Mülheim, München before Madrid; ß (0xE1, ANSI a-acute) gives the same order for all 115 towns
  whether a language driver treats it as a letter or not.
- **Header** seg1:7506: text 375 (Ansehen; 360 Verkaufen when DS:3FC2 = 0x10) at
  x = 28; PFEIL1 (pressed PFEIL2) at (x − 15, 8) before the category text
  361 + DS:8C48 (Alle, Industrie, Handel, Agrar, Risiko, Luxus, Sonstige) and the
  sort text 368 + DS:8C4A (Sortieren: Wert / Rendite / Stadt); x advances by
  0x84 / 0x6A when DS:A0F0 = 'd' (x 160 / 266), otherwise by 0x7A / 0x92 (x 150 /
  296; seg1:766F / 77C4 compare with 0x64 only). The German `dat/lan.inf` holds
  `D` (0x44), so the supported copy takes 150 / 296 (rebuilt since the lan.inf
  letter is read; earlier rounds used 160 / 266). Not yet compared with a
  screenshot of rally.exe.
- **Eight rows always** (round 136): seg1:60D6 calls seg1:6119 for rows scroll …
  scroll + 7 (`cmp 8` at seg1:60F5); its strip seg1:65A4 (GR_Z / ZW_Z, the selected
  one GR_ZI / ZW_ZI) has no count check, while the owner icon (seg1:6160) and the
  texts (seg1:6348) skip slots past the list (seg1:6180 / 635F). An empty statistics
  therefore shows the header and eight empty strips (the rebuild drew only the
  header) and the pages are eight rows high (the rebuild used nine).
- **Rows** seg1:6E87: frames ZW_Z / ZW_ZI, owner icon at x = 0, Arial 17 weight
  400: entry name at x = 44, town name (record +6) at x = 200, value · 0.7
  (DS:868E) right-aligned in [339, 422], income in [430, 490], yield `%.1f%%`
  (DS:402E) in [496, 554]; no row is disabled. Page buttons step DS:8C48 by
  (x + 6) % 7 (seg1:59EF) or the scroll by seven (seg1:59C7 / 5B15).
- **Input** (round 104): the header items are 0 (Ansehen; Verkaufen in the forced sale),
  1 (category), 2 (sort order) and 3 (close); the focus starts on 0 (seg1:4FC9) and
  is drawn white. Left / Right move it with wrap-around; Enter or a click on
  0–0x81, 0x82–0xD1, 0xD2–0x1AE, 0x1AF–0x22E acts: 1 / 2 step category / sort
  forward ((x + 1) % 7, % 3; scroll 0; list rebuilt, seg1:56CA / 5710), 3 closes
  with Plopp02 (Zaehler2 while DS:3FB8 = 3). Up / Down change the focused item:
  0 pages by seven (seg1:59BE / 5B07: forward while count − 8 > scroll, else to the
  end), 1 / 2 step backward / forward (seg1:59EF / 5A40). Escape, Backspace and the
  right button close.
- **Forced sale** (round 105): after every arrival (seg2:1618, before nextPlayer)
  seg1:4E39 checks the driver's cash < 0.0 (DS:8696 = 0.0) and seg1:4302 > 0 — the
  sum of the values of his own entries that are not rented out and not of category
  3 (entry +0x1E). Then seg1:4EA8(0) opens the statistics with DS:3FC2 =
  (0 << 5) + 0x10 (the menu's statistics, argument 1, are 0x30), sort 0, without
  rented entries (seg1:4504's third argument), and DS:3FB8 = 3 while the cash is
  negative and something is sellable (seg1:4FEB ff.). Header item 0 then reads 360
  Verkaufen; Enter on it switches to the sale mode 0x12 when the list is not
  empty (seg1:5687; in 0x30 it does nothing). There Up / Down stop at the ends,
  Enter or a row click sells the selected entry at value × 0.7 (DS:868E) through
  seg1:8327 (seg1:57A4, 6081); the list is rebuilt, an empty one returns to the
  header, a selection past the end moves back (seg1:5816 / 5843); seg1:5857 lifts
  the lock once the cash is no longer negative or nothing is sellable. While
  locked, closing plays Zaehler2 (seg1:5748). Escape in 0x12 returns to the header.
  *Round 147:* when the lock lifts in the forced variant ((DS:3FC2 & 0x30) = 0x10),
  seg1:5896 sets DS:3FB8 = 0xFF, waits 750 ms (seg6:0F60(0x2EE)) and posts WM_KEYDOWN
  VK_BACK twice (seg1:58A7 / 58BB): the first leaves the sale mode (seg1:53BE,
  Plopp02), the second closes the dialog (seg1:5391), so the turn goes on by
  itself. Rebuilt: `EstatesView::unlock` and two Backspace events after 750 ms.
  Computers (seg2:5C02) get state 14 (seg5:71B3(0xE) → seg5:70B5): seg5:962A lists
  their own, not rented entries without category 3, scored by seg5:990A plus the
  money record's +0x32 (10) when a player owns the whole town, bubble-sorts them
  lowest first (swap on >) and takes them until cash + value × 0.7 (DS:8706) is no
  longer negative; seg1:50D1 sells them. Rebuilt: `Game::endTurn` /
  `forcedSell` / `sellableValue`, `aiForcedSale`, the statistics' forced mode in
  `src/estates_view.cpp` (rules revision 38).
- **Rebuilt** (`statisticsRows` in `src/estates.cpp`, `src/estates_view.cpp`):
  the list of the current player with rented entries, header, rows and input as
  above. Header arrows (round 110, seg1:752A / 7687 / 77DC): pfeil2 before the
  item with the focus, pfeil1 before the others, at (14, 8) before item 0 and
  15 pixels before items 1 and 2. They are the only frames of this window drawn with
  seg2:9497, whose last argument is the colour key: 0xFF (seg1:7540); both frames
  of `sbs/buy.sbs` (15 × 28) have a 0xFF background. The others are copied
  opaque (seg2:913A).
- **Loading:** the original loads from the title menu MAIN (1021, round 35);
  `--game --load <slot>` loads a slot directly.

### Save dialog (round 33)

- **Call:** MAIN1 "Spiel speichern" (1062, seg4:07B8) closes the menu, calls
  seg8:B157(1), then seg5:607B(−1) and reopens ONFIELD (seg2:068F).
- **Window** seg8:B157: WinG bitmap, sprites `%s\ls.sbs` (DS:8081; frames KO_Z,
  KO_ZI, 558 × 42), class TEMP, 0x22E × 0x1CE (558 × 462) at a computed position
  (assumed centred). DS:A052 = 1 save, 0 load.
- **Paint** seg8:BA8A: KO_Z header at (0, 0), title text 800 "Spielstand
  speichern" / 801 "Spielstand laden" in Arial 22 weight 600 at (10, 10), colour
  0xFF, shadow 1. Slots 0–8 (seg8:B746): KO_ZI for the chosen slot DS:A7B8, KO_Z
  otherwise, at (slot + 1) · 42; text = profile section `Save`, key `Slot%d`,
  default `leer` (GetPrivateProfileString, DS:80C0 / 80D0 / 80D7) in Arial 13
  weight 400 at (15, slot · 42 + 52), colour 0xFF chosen, 0x45 otherwise. Footer
  KO_Z at y = 420 with text 116 (Nein) from x = 30 and text 114 (Ok) right-aligned
  to x = 528 at y = 428; the highlighted button DS:8D64 (7 Nein, 6 Ok) in 0xFF.
- **Input** seg8:B578: Left → Nein, Right → Ok, Up / Down move the slot within
  0–8, Enter acts (Nein closes), Escape and Backspace close; a right click sends
  Backspace (seg8:B349).
- **Saving** seg8:B413: file `%s\slot%d` (DS:80B5) in the save directory DS:7E
  and the description `"%Fs (%d) - %Fs"` (DS:809F): text 301 + month % 12, year
  + 1, and the names of the players in play joined by " - " (seg8:BD58; at most
  79 characters, cut with "..."), written with seg5:2366 to section `Save`
  (DS:80C0), key `slot%d`.
- **Rebuilt** (`src/save_slots.cpp`, `src/save_view.cpp`): the same dialog and
  descriptions, but the slots are the native save format (docs/save-format.md)
  in `$XDG_DATA_HOME/dtvr-recomp/saves` (`slot<n>.sav`, descriptions in
  `saves.ini`); original save files are neither read nor written. Confirmed in
  round 95: the footer starts on Ok (DS:8D64 = 6, seg8:B253; Left sets 7 = Nein);
  WM_LBUTTONDOWN (seg8:B8F6) only chooses a slot inside its rectangle
  (seg8:BA3B), posts Escape for (0, 0x1A4)–(0x96, 0x1CE) and Enter for
  (0x1CA, 0x1A4)–(0x22E, 0x1CE), which acts with the current footer choice;
  WM_LBUTTONDBLCLK posts Enter (seg8:B335), WM_RBUTTONDOWN Backspace. The
  default word `leer` is a literal of rally.exe, not a language text.

### SBS: remaining hypotheses

- Palette: the four SBB palettes differ from `butt/1/map.bmp` in only 2 of 256
  entries; viewers use the SBB palette for the map and `map.bmp` for sprites.
  Colour cycling (`[Setup] Colorcycle`) is traced in "Colour cycling" (round 82).
- Frame names suggest animation sequences (`DDOWN1..4`, `V01..`); playback order,
  timing and placement come from code that is not yet analysed.
- `map/europe_d.tow`: contains readable city and property names, including
  London and Tower-Cafe. Record layout is not yet established.
- `dat/d/rally_d.dat` is itself an MZ/NE binary despite the `.dat` suffix.
- Tourist information in `butt/d/views/*.txt` is readable as Windows-1252.

## Open questions

The contents of `.car`, the field-list values, town entry fields and
`flagset` bits need to be derived from their consumers. File sizes
alone do not establish record layouts. Palette selection, colour cycling, bit 12
of map cells and the viewport (which map area is shown at which position) are
not yet traced. What the game's month counter is, and where DS:B5DC is set, is
open.

The world view composes the whole 256 × 256 map with the original rules. It
does not reconstruct the viewport, sprites on the map, routes or game rules.

## Next milestones

1. ~~Decode SBS sprite names, frame boundaries and pixels.~~ Done. Confirm
   transparency and palette selection from the executable.
2. ~~Locate original map-loader routines and derive tile mapping.~~ Done.
   Palette selection and colour cycling are still open.
3. ~~Decode cities and the road graph.~~ Field graph and town records decoded and
   validated against the composed map.
4. Movement: step counting, slot machine, random generator, tile properties and
   routes recovered (see "Movement").
5. Turn: direction input, arrival condition and the field dispatch recovered;
   money fields, inflation and destination reward rebuilt.
6. Player change, months, start, destinations and stages recovered; a playable
   hotseat game (`--play`) assembles all rules so far.
7. Event card deck, shuffle, hands, yellow field and movement cards rebuilt.
8. Property economy: starting cash, town state, purchases in eight modes, whole-
   town bonus and monthly income rebuilt.
9. Selling, credit at the year end, card targets and 7 more card effects (money
   and status cards) rebuilt.
10. Dr. Drago (holder, forms, meetings, six actions), Weihnachten and Hau ab
    rebuilt.
11. Event trigger, event tables, Eastern Europe (barriers, opening) and robbers
    rebuilt.
12. Road obstacles (rain, snow, rockfall) with Schneepflug, property damage
    (storm, drought, flood, Nessie, earthquake, volcano) with Versicherung and
    Nessie. Next: professor, the remaining cards, card fields and computer players.

Native rendering, input, timing and audio can use SDL and Linux libraries.
The choice between translating executable routines and manually reconstructing
individual systems remains open pending disassembly of the relevant routines.
