# Original overview: zoom, nine-position panning and input

Reference: `rally.exe` SHA-256
`aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83`.
Native baseline: `343e688fd72425419643e4f1432a70edddc8fd11`.
The hash and code segments 1, 4 and 6 were compared with the private disassembly
before tracing. This audit uses static original-code analysis, synthetic native
rendering tests and a native smoke run with supplied game data. The original
Windows executable was not run interactively.

## Confirmed original behavior

The OUTLINE overview is interactive. It does not close on every key or mouse
button, as the audited native implementation assumed.

- **Initialization, seg1:A30C–A363:** the source rectangle starts at `(0,0)` with
  the full map-window width and height. These dimensions are retained at
  DS:8C5A/8C5C. Zoom DS:8C5E and cell index DS:8C60 start at zero.
- **Enter, seg1:AB95–ABE2:** toggles zoom. Enlarging resets the cell index and
  selects the top-left half-width/half-height source rectangle. Returning to
  normal restores the full source rectangle at `(0,0)`.
- **Arrow keys, seg1:ABE8–AC9D:** while enlarged, move among a 3-by-3 grid of
  overlapping source rectangles. Up/Down change the index by three, Left/Right
  by one, and movement stops at each grid edge. Outside zoom they do nothing.
- **Crop offset:** each horizontal step is `(fullWidth >> 2) - 4`; each vertical
  step is `(fullHeight >> 2) - 4`. The source width/height remain half of the
  original map-window dimensions. Use the window dimensions, not the smaller
  `map.bmp` width. For positive dimensions these shifts are integer division.
- **Painting, seg1:AA7D–AAA0:** `WinGStretchBlt` stretches that source rectangle
  to the full overview window. The map, route, markers and flags have already
  been composed into the source bitmap, so they are enlarged together.
- **Escape/Backspace, seg1:AA00–AA18:** destroy the overview window. Other
  ordinary keys such as Space or letters have no closing action.
- **Right button, seg1:AB50–AB53:** dispatches to seg1:A9A7, closing the overview.
- **Left button, seg1:A9B2–A9D3:** when not enlarged, acts as Enter. While
  enlarged it calls the existing MCTRL mouse-steering window through seg4:0F52.
  Its returned key is delivered to OUTLINE, allowing arrow panning or Enter.
- **Double-click, seg1:AB34–AB37:** dispatches directly to the Enter path at
  seg1:A9D3. Middle-button and mouse-motion messages have no explicit action.

This is presentation state. The camera crop does not change any player field,
route selection, movement allowance, random state or save-file content.

## Isolated implementation

[overview_view.hpp](../src/overview_view.hpp) and
[overview_view.cpp](../src/overview_view.cpp) now reproduce these input paths.
The view owns its zoom/cell state and reuses the existing `MouseControlView` for
the steering popup, without changing the shared game-window dispatcher.
Opening the overview resets zoom and closes any previous popup.

Rendering now composes the existing map/route/markers/flags into an owned SDL
target texture at the original window dimensions. A single nearest-neighbor
copy applies the original crop and enlargement. An RAII target guard preserves
the caller's render target, logical canvas, scale, viewport and clip; the mouse
popup is drawn afterward at its normal canvas size. Drawing the marks after
enlargement would misalign them with roads and fields.

Existing approximation of Dr. Drago's flag position, route reconstruction and
base marker shapes are unchanged. This patch does not claim a full pixel-level
match with the original renderer. Physical desktop pointer capture/clipping is
also not validated by these headless tests; the existing steering view clamps
received pointer positions to its control area.

## Verification

[overview_view_test.cpp](../tests/overview_view_test.cpp) uses a generated color
pattern BMP, synthetic flags and a two-town synthetic board. All fixtures and
optional screenshots are outside the source checkout. No original game content
is required by the test.

Coverage includes all nine crops at 640/800/1024, grid boundaries, reset on
reopening/re-enlarging, ignored keys/buttons, closing keys, ordinary/double left
clicks, mouse-steering release into pan/zoom actions, popup-only right-click
closure, and enlargement of flags and underlying markers together. Pixel checks
also verify that the overview does not overwrite the surrounding canvas.
A separate check preserves a nondefault logical canvas, scale, viewport, clip
and existing render target through the draw operation.

The baseline fails 120 assertions. The corrected test passes on SDL's software
renderer without a window or video device. Synthetic full/zoom screenshots were
inspected externally. An isolated source export also built the full native
executable and passed a 640-pixel `--game --no-title --no-intro --overview`
dummy-driver smoke run with the supplied original data; its external screenshot
was inspected. That run validates native loading/rendering, not desktop input
or an interactive comparison with the original Windows program.

## Test integration handoff

The existing application already links `mouse_control_view.cpp`; no runtime
source-list change is needed. Leave the shared integration files to the parallel
engine work. Add the test target when reconciling CMake:

```cmake
add_executable(overview-view-test tests/overview_view_test.cpp
    src/overview_view.cpp src/sdl_assets.cpp src/mouse_control_view.cpp)
target_compile_options(overview-view-test PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(overview-view-test PRIVATE dtvr-formats PkgConfig::SDL2)
add_test(NAME overview-view COMMAND overview-view-test)
```

This target was built and its registered `overview-view` CTest passed in the
isolated export. The test optionally accepts an external screenshot directory:

```sh
ctest --test-dir /path/to/build -R '^overview-view$' --output-on-failure
/path/to/build/overview-view-test /path/to/external-artifacts
```

When reconciling shared `docs/formats.md` and user controls, replace the
any-key/any-click dismissal assumption with Enter/left-click zoom, arrow panning
while enlarged, and Escape/Backspace/right-click exit. The README status remains
partial; this change does not complete the remaining original game interface.
