# Project branding

Original, project-authored vector artwork under **GPL-3.0-only**; see the root
`LICENSE`. No original game art, fonts, traced graphics or external images were
used. All lettering and shapes are SVG paths or simple geometry.

- `logo.svg`: 1200 × 360 repository banner, with a geometric DTVR Recomb wordmark.
- `icon.svg`: 256 × 256 application/installer mark, transparent outside its tile.

The winding road represents routes, the die represents board-game decisions, and
the amber marker represents a destination. Palette: midnight `#101c2e`, slate
`#142338`, mint `#6ce5c2`, ivory `#f5edda` and amber `#f3b64c`.

Keep these editable SVG sources in Git. Raster exports are generated outside the
checkout, never sourced from an original disc:

```sh
python3 tools/export_branding.py --output /path/to/branding-exports
```

This optional packaging tool requires `rsvg-convert` (librsvg). It generates a
banner PNG, PNG icons from 16 to 512 pixels and a multi-resolution Windows ICO.
The native installation ships the SVG icon in the standard hicolor scalable
icon directory. A graphical installer and desktop launcher integration remain
future work; the icon is prepared for them.
