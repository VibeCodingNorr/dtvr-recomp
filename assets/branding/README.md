# Project branding

Original project artwork under **GPL-3.0-only**; see the root `LICENSE`.
The user-approved generated logo with road, tower and die is embedded losslessly
as PNG data in self-contained SVG files. This deliberately preserves the smooth
die, lettering, contours, colors and subtle background of the approved image.
These are raster-backed SVG containers, not resolution-independent vector paths.
No original game artwork or external font files are included.

- `logo.svg`: 1200 × 400 banner containing the complete 2172 × 724 master artwork
  and lowercase **dtvr-recomp** wordmark, with rounded outer corners.
- `icon.svg`: 256 × 256 application/installer mark, cropped from the same original
  master at coordinates (0, 70), size 600 × 600, with rounded outer corners.
- `logo-dark.svg`: transparent installer variant. The emblem retains the smooth
  die and gains a subtle light edge. The original wordmark shapes are recovered
  through an SVG alpha mask and filled with ivory, avoiding generated cutout
  speckles. There is no background panel. Embedded project images and the mask
  make this asset self-contained too.

The winding road leads toward a town tower against an amber sun; the tilted
five-pip die represents the board-game rally. The lowercase wordmark leans
forward. The standard banner retains the original colors and cream backdrop for
light surfaces. The installer uses the transparent dark variant with ivory text
on its navy sidebar, rendered at 264 × 88 without stretching.

Keep these self-contained project SVG assets in Git. Standalone PNG/ICO exports
are generated outside the checkout, never sourced from an original disc:

```sh
python3 tools/export_branding.py --output /path/to/branding-exports
```

This optional packaging tool requires `rsvg-convert` (librsvg). It generates a
standard and transparent dark banner PNGs, PNG icons from 16 to 512 pixels and a
multi-resolution Windows ICO.
`tools/branding_render.py` renders at four times the target dimensions and then
halves the image twice through librsvg/Cairo, filtering color and alpha together.
This reduces aliasing in the fine emblem contours, rounded icon corners and
lettering mask. Source GUI rendering, standalone exports and AppImage packaging
share this path; no additional imaging library is required on the user's host.
The native installation ships the SVG icon in the standard hicolor scalable
icon directory. The graphical installer prefers `<name>-dark.svg` when present
and otherwise uses the standard SVG. Setup packaging makes the same selection
and embeds generated PNG versions for hosts without a converter. Desktop
launcher integration remains separate work.
