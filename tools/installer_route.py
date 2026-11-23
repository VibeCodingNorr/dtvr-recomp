# SPDX-License-Identifier: GPL-3.0-only
"""Small route/waypoint artwork; SVG is rasterized with antialiasing, not by Tk."""

from installer_palette import NAVY, MINT, AMBER

WIDTH, HEIGHT = 56, 224
LEFT, TOP = 20, 194


def route_svg(active):
    if type(active) is not int or active not in range(3):
        raise ValueError("Installer phase must be 0, 1 or 2")
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{HEIGHT}" '
             f'viewBox="0 0 {WIDTH} {HEIGHT}">',
             f'<rect width="{WIDTH}" height="{HEIGHT}" fill="{NAVY}"/>']
    for index in range(2):
        y = 24 + index * 88
        bend = 44 if index == 0 else 12
        path = (f'M28 {y} C28 {y+26} {bend} {y+26} {bend} {y+44} '
                f'S28 {y+62} 28 {y+88}')
        color = MINT if index < active else "#7d938c"
        parts.append(f'<path d="{path}" fill="none" stroke="#294351" stroke-width="14"/>')
        parts.append(f'<path d="{path}" fill="none" stroke="{color}" '
                     f'stroke-width="2" stroke-dasharray="4 6"/>')
    for index in range(3):
        y = 24 + index * 88
        color = MINT if index <= active else AMBER
        fill = color if index == active else NAVY
        parts.append(f'<circle cx="28" cy="{y}" r="19" fill="{fill}" '
                     f'stroke="{color}" stroke-width="2"/>')
        if index < active:
            parts.append(f'<path d="M20 {y} L26 {y+6} L37 {y-7}" fill="none" '
                         f'stroke="{color}" stroke-width="3" '
                         f'stroke-linecap="round" stroke-linejoin="round"/>')
    return "".join(parts) + "</svg>"
