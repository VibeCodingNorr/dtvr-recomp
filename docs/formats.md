# Original data: observed structure

These observations come from the local German CD at
`/media/steam1/Games/Dtvr/ISOINHALT`. They describe this copy, not every release.
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
python3 tools/ne_disasm.py PROGRAM/rally.exe local/disasm --use32 9 \
    --dll-dir PROGRAM --dll-dir CDROOT/wing --dll-dir /usr/lib/wine/i386-windows
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
  files. `europe.pit` (→ DS:46) and `europe.car` (→ DS:4A) are loaded but not
  yet analysed.

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

The contents of `.pit`, `.car`, the field-list values, town entry fields and
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
   validated against the composed map. Movement rules, field effects and the
   meaning of marker colours are next.
4. Reconstruct one human player's complete turn before implementing more rules.

Native rendering, input, timing and audio can use SDL and Linux libraries.
The choice between translating executable routines and manually reconstructing
individual systems remains open pending disassembly of the relevant routines.
