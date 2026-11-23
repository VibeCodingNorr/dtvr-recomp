# Proposed fullscreen and upscaling options

> Update: fullscreen, integer scaling and an optional widescreen extension are
> implemented (README "Widescreen", `widenLayout` in src/layout.cpp). Widescreen
> is off by default: the original 4:3 layout with side bars stays the reference.

Status: recorded idea, not implemented by this document. Renderer integration
belongs to the engine work; installer validation must not modify that work.

Offer fullscreen later, including 1440p and 4K output, while preserving the
original 1024 × 768 logical game canvas and its 4:3 aspect ratio. Scale the whole
canvas uniformly and centre it. Show side bars on wider displays rather than
stretching the map, vehicles or interface.

| Display | Largest proportional 4:3 image | Scale | Side bars |
| --- | --- | --- | --- |
| 2560 × 1440 | 1920 × 1440 | 1.875× | 320 pixels each |
| 3840 × 2160 | 2880 × 2160 | 2.8125× | 480 pixels each |

Proposed settings are Window / Fullscreen, aspect-preserving fit, and an optional
integer-scale mode for crisp pixels with larger borders. A sharp or smooth
scaling choice can make the fractional-scale tradeoff explicit. Upscaling changes
output size; it cannot recreate detail absent from the original artwork. Shaders
and other graphics effects remain separate optional ideas.

Keep gameplay coordinates, movement and original layout independent of display
pixels. Transform mouse positions through the inverse scale and image offset;
ignore clicks in the borders. Verify menus, dialogs, text and pointer alignment
at each scale, together with fullscreen toggling and focus recovery. No original
assets or new render code are needed to record this proposal.
