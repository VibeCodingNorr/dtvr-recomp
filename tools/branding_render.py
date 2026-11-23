# SPDX-License-Identifier: GPL-3.0-only
"""Antialiased branding PNGs through librsvg, with no extra imaging dependency."""

import base64
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET


def render_png(source, width=None, height=None):
    """Render at 4x, then halve twice so fine artwork and alpha edges are averaged.

    Directly sampling the embedded high-resolution artwork at icon size can
    alias fine contours, especially the dark logo's lettering mask. The two
    2:1 image passes let Cairo filter each intermediate image, including its
    premultiplied alpha. Packaged and source execution use the same renderer.
    """
    source = Path(source)
    if width is None or height is None:
        root = ET.parse(source).getroot()
        width = int(root.attrib["width"]) if width is None else width
        height = int(root.attrib["height"]) if height is None else height
    return _render(width, height, path=source)


def render_svg(svg, width, height):
    """Render generated project SVG without intermediate files."""
    return _render(width, height, data=svg.encode("utf-8"))


def _render(width, height, path=None, data=None):
    renderer = shutil.which("rsvg-convert")
    if renderer is None:
        raise ValueError("Branding requires librsvg's rsvg-convert")
    if type(width) is not int or type(height) is not int or not (1 <= width <= 2048 and 1 <= height <= 2048):
        raise ValueError("Branding dimensions must be integers from 1 to 2048")

    def run(w, h, path=None, data=None):
        return subprocess.run([renderer, "-w", str(w), "-h", str(h),
                               *([str(path)] if path is not None else [])],
                              input=data, capture_output=True, check=True, timeout=10).stdout

    png = run(width * 4, height * 4, path=path, data=data)
    for scale in (2, 1):
        previous_w, previous_h = width * scale * 2, height * scale * 2
        encoded = base64.b64encode(png).decode("ascii")
        image = (f'<svg xmlns="http://www.w3.org/2000/svg" '
                 f'xmlns:xlink="http://www.w3.org/1999/xlink" '
                 f'width="{previous_w}" height="{previous_h}" '
                 f'viewBox="0 0 {previous_w} {previous_h}">'
                 f'<image width="{previous_w}" height="{previous_h}" '
                 f'image-rendering="optimizeQuality" '
                 f'xlink:href="data:image/png;base64,{encoded}"/></svg>')
        png = run(width * scale, height * scale, data=image.encode("ascii"))
    return png
