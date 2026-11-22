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
- **Field distances** seg2:43A8(unused, start): breadth-first search over the
  field links up to 80 levels into a word array at DS:22; a field with
  `flagset & DS:A10F` ≠ 0 is not expanded (seg4:0E0B sets the mask to 0x4000).
  **Route** seg2:9996(from, to) clears the mask, computes distances from `to`
  and from `from` repeatedly takes the first neighbour (up, right, down, left)
  with a smaller distance, recording each field via seg2:4D3F into a 1,000-entry
  ring buffer at DS:2C. seg1:A4E4 draws such a route with GDI `MoveTo`/`LineTo`.

### Turn and field actions (round 5)

- **Direction input** (main window procedure, seg4:0001): key-down of VK_UP,
  VK_RIGHT, VK_DOWN, VK_LEFT sets DS:A77A to 1, 2, 3, 4 (seg4:01F2–020A); a
  second message releases them to 0 (jump table seg4:00FC). The vehicle
  therefore drives while an arrow key is held. Other keys: Space centres the
  view on the player (seg2:3216 → seg6:3064, then redraw seg2:05A8), Enter →
  seg2:0BC3, Backspace/Escape → a dialog via seg5:5214, `1`–`4` → seg6:0DA0,
  Insert/Delete → seg6:2E54(3).
- **Cells with pit bit 0x20**: entering one calls seg2:2404(x, y); a zero result
  stops the vehicle (seg2:21D0). Meaning open.
- **Arrival** (seg2:0D38): the field action runs when the player is on a field
  (DS:9592 ≠ −1), steps taken = allowance or allowance = −1, the vehicle is
  aligned to the cell grid (seg2:2C0F: `(x | y) & 0xF == 0`) and DS:9EDE = 0. A
  move therefore only triggers a field when the full allowance is used.
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
- **Stages**: DS:A104 = remaining stages, read with key `Rounds` (DS:184D,
  seg8:0A6A; new game default 99 at seg4:0E23), DS:A106 = total. Reaching the
  destination with A104 ≠ 0 pays 100,000 × inflation, decrements A104 (at 0:
  "last stage" message seg7:372F(3)), sets the origin to the reached field and
  draws a new destination; then seg2:9C97 gives **Dr. Drago** to the active
  player with the largest value in the distance array DS:22 (first active on a
  tie or with one player). *Correction (round 10): this was first read as the
  player who starts the stage; seg2:9C97 places Dr. Drago (DS:9CFC).* With A104 = 0 the reward is 300,000 × inflation and WM_COMMAND
  0x424 ends the game (seg4:075B tears down the game windows).
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
  | 8 | Abkaufen | another player's entry; cash ≥ value; owner receives value, buyer pays |
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

Rebuilt in `src/economy.cpp` and wired into the game: starting cash, purchases
in all modes above (the game exposes the normal purchase after stopping in a
town), whole-town bonus, monthly settlement, rent expiry.

Open: how a burden arises, the use of player +0x4A, the entry category and
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
- **Money cards** (each handler calls nextPlayer seg2:06FC afterwards, so the
  card replaces the turn):
  - Fiskus (seg3:14AD): target pays (cash + property value +0x5A) × 0.1 (DS:8636).
  - Konto (seg3:15B0): with more than one active player every cash balance is set
    to the average of all active players.
  - Schulden (seg3:1790): only if the player's cash is negative; the target
    takes the negative amount, the player's cash becomes 0.
  - Zero (seg3:18EA): the player's own cash becomes 0.
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
  random(13) (seg2:1127).

Rebuilt: Dr. Drago state, placement, form countdown and changes, meetings, all
six actions, Weihnachten, Hau ab, the block flag in nextPlayer and these text
draws; nextPlayer now runs the credit charge, settlement and credit clearing
at the original points through hooks. Assumptions: Dr. Drago changes hands
when a move ends (the original checks while the vehicle stops on fields), and
the stage placement uses distances to the new destination. Open: Konferenz,
the event system with the passive cards, Tschüss, Punch, Erdbeben, Nägel,
Risiko, Flugzeug, Steuerlos, the card field, property cards in the town dialog.

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
  and nextPlayer.
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
- **Verdoppeln** (card 40, seg3:3FBB): sets DS:2EB0 = 1; the card handler at
  seg3:0E83 then skips removing the next card (seg2:3C62) and clears the flag.
  seg2:0CE8 clears it at every player change.

Rebuilt in `src/events.cpp` (`investmentPayout`), `src/game_cards.cpp` and
`src/game_events.cpp`. Simplifications: Punch uses the direction of the
player's last step as facing (right before the first move); the viewer invests
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
  in both directions (flag 0, used by the route planner, always for rockfall) or
  counts as clearable (flag 1, used by the card choice). Directions come from
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
  the card is not skipped.
- **Card field** state 12: seg5:7D85 sells every hand card without a worth
  (adding half its value), seg5:7E4F takes spare money (seg5:9A34) plus the
  sales, collects affordable offers with a worth, scores them worth +
  random(word +0xC), +10 for the card that reaches the destination exactly,
  sorts highest first and buys in order until one is too dear or the hand is
  full. seg3:4266 executes sales first.
- **Investment offer** seg8:6FBF has no computer branch: the modal dialog waits
  for input on any turn. Assumption here: computers decline.

Rebuilt in `src/ai_cards.cpp` and `src/game_ai.cpp`. Approximation: DS:22 is
taken as the distances from the destination with the route mask (the array
holds whatever seg2:43A8 computed last). Not rebuilt: Risiko.

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
(`dtvr-recomb --game`): canvas background, map window with the black margin,
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
  menu (seg5:5214, MENU 192 × 48 at (0, 0)), Enter seg2:0BC3, Space centres the
  map on the current player (seg2:3216, 05A8), arrows set the steering direction
  DS:A77A (1 up, 2 right, 3 down, 4 left), Insert / Delete seg6:2E54(3), keys 1–4
  seg6:0DA0(key − 1), C seg2:7F72, Q seg4:0EDF.

Rebuilt in `src/panel.cpp` (SDL-free contents) and `src/text.cpp` (SDL2_ttf with
Arial, else Liberation Sans; bold face for weight 600). Simplification: slot 6
shows while the player stands on the reached field (phase Arrived).

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
  window (DS:A6F0) centred at its bottom: x = client width / 2 − bitmap width / 2,
  y = client height − bitmap height (seg2:6528). The paint (seg2:6C2B) only blits
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
- **Timers**: the game timer is 50 ms (seg5:01EB → seg5:02CF); others run every
  1000 ms (seg2:3AF0) and 80 ms (seg2:8018). The slot loop (seg2:65C4) advances
  the animations once per pass and synchronises with the game loop (seg2:1C37).

Rebuilt in `src/slot_view.cpp`: housing, spinning and stopping reels and the sum
display, started for every spin of the game (`Game::lastReels`). Assumptions:
one animation tick is the 50 ms game timer; the window stays one second after the
last reel. Not yet: the lever animation (`SLTHAD*`), the gear animations of 3–5
reels and pulling the lever with the mouse.

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
  y = +0x18 − height (+0x2E) + 2; seg2:1A91 draws a frame with its top-left corner
  at that point minus the map origin (DS:956A / 9568 · 16), and the map tiles use
  the same mapping (seg6:2857, seg6:2C0A). **Open:** taken literally, a car stands
  one cell right of and half a cell below its field (vertical frames are opaque in
  columns 16–31, horizontal ones in rows 7–25), although europe.pit marks the road
  and the field (bit 0x01) on the field's own cell (e.g. London (86, 96)). A
  further offset in the path from the sprite list to the window has not been found.

Rebuilt in `src/game_screen.cpp`: the vehicles of all drivers in play with the
facing of their last step; the current driver's car animates while driving.
Assumption until the offset is traced: frames are centred on the field cell
(−16, −8), which puts the cars on the road in all four facings.

### Messages (round 25)

- **Message window** seg6:51EA (about 70 callers) creates `INFO` (procedure
  seg6:5492) with the paper `%s\%d\p1.bmp` (DS:3ABC, butt directory) and the
  showmaster `%s\%d\sm.bmp` (DS:3AC9, bmp directory). Size: width a of element 5
  of seg6:1140 (400 / 500 / 600), height = showmaster height + 0x24; it is
  centred on the canvas (seg6:5852 → seg6:0000) and modal (seg6:0FB4). DS:8C36
  selects the buttons (0 none, 1 Yes / No with texts 114 ff., result DS:8C38);
  a click or key closes it.
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
showmaster, word-wrapped text). Not yet: the remaining callers (cards, Dr.
Drago, town and card fields, investments), the button variants and the
showmaster's other portraits.

### SBS: remaining hypotheses

- Palette: the four SBB palettes differ from `butt/1/map.bmp` in only 2 of 256
  entries; viewers use the SBB palette for the map and `map.bmp` for sprites.
  Palette selection and colour cycling (`[Setup] Colorcycle`) remain to be traced.
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
