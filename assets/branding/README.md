# Project branding

Original project vector artwork under **GPL-3.0-only**; see the root `LICENSE`.
The current identity recreates the user-selected second logo concept as editable
SVG geometry. No original game artwork, font files or raster images are embedded.
All lettering and shapes are SVG paths or simple geometry.

- `logo.svg`: 1200 × 360 banner with the lowercase **dtvr-recomp** wordmark.
- `icon.svg`: 256 × 256 application/installer mark, transparent outside its tile.

The road forms a lowercase **r**, mint code brackets represent the native
reconstruction, and the amber pin marks a destination. Palette: navy `#0c2c48`,
mint `#16c993`, ivory `#fff8eb` and amber `#f5ad16`. The ivory panel keeps the
navy mark legible on the installer's dark sidebar and in light or dark READMEs.

Keep these editable SVG sources in Git. Raster exports are generated outside the
checkout, never sourced from an original disc:

```sh
python3 tools/export_branding.py --output /path/to/branding-exports
```

This optional packaging tool requires `rsvg-convert` (librsvg). It generates a
banner PNG, PNG icons from 16 to 512 pixels and a multi-resolution Windows ICO.
The native installation ships the SVG icon in the standard hicolor scalable
icon directory. The graphical installer uses the banner and icon directly when
`rsvg-convert` is available; setup packaging embeds generated PNG versions for
hosts without a converter. Desktop launcher integration remains separate work.
