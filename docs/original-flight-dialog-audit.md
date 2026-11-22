# Original Flugzeug booking: input audit and integration handoff

Reference: `rally.exe` SHA-256
`aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83`.
Native baseline: `ce7e9dd21a402716e46c942e160f57385f4f75ed`.
The original hash and raw code segments 3 and 6 were checked against the private
disassembly before tracing. This is static analysis with native synthetic input
tests, not interactive execution of the Windows game.

## Confirmed input rules

| Operation | Original evidence | Audited native behavior | Correction |
| --- | --- | --- | --- |
| Open booking | seg3:362E selects row 0 | Selects row 0 | Already correct |
| Up | seg3:346D–3476 decrements only above zero | Wraps to last row | Clamp at first row |
| Down | seg3:34B9–34C4 increments only below DS:8C1E | Wraps to first row | Clamp at last row |
| Pointer motion | No `WM_MOUSEMOVE` handler in seg3:3971–39B9 | Changes selection | Retain selection |
| Left click | seg3:339B–3453 hit-tests each row, then sets DS:A2A2 and sends Enter | Returns a town without updating the selected row | Update selection for an accepted choice |
| Escape / right click | Key dispatcher seg3:3565 handles only Enter, Up, Down; main dispatcher has no right-button handler | Cancels booking | No cancellation from these inputs |
| Brussels on final stage | seg3:33F2–343E rejects the click before assigning DS:A2A2; seg3:3507–3554 also rejects Enter | Returns `closed` for the caller's text 8300 | Preserve refusal and retain previous selection after a refused click |

The main procedure dispatches `WM_KEYDOWN` at seg3:39A1 and
`WM_LBUTTONDOWN` at seg3:39A8. Other mouse-button messages go through the default
window procedure. No Escape-to-close or right-button-to-close path was found;
the explicit key handler consumes unrecognized keys without closing the window.
The non-client helper seg6:1A68 is invoked only for non-client messages `0x83`
and `0x85`, not for these input messages.

The row hit rectangles are confirmed at seg3:33A7–33E6: x `[0,240)`,
y `[50 + 24 * row, 74 + 24 * row)`. seg3:3633–3644 sets the highest available
row to 7 when Eastern Europe is open, otherwise 6. The native seven/eight-row
choice and rectangle boundaries already match. The selected row is painted white
(`0xFF`), the others brown (`0x45`), at seg3:3862–3877.

The final-stage restriction uses DS:A104 == 0, as already represented by the
native `lastStage` flag. It applies to Brussels, not every flight destination.
The `Result::closed` field indicates the refusal message, not that the booking
window should close; the existing caller keeps it open.

## Prepared implementation and verification

Changes are isolated to [flight_view.cpp](../src/flight_view.cpp), its header
comments in [flight_view.hpp](../src/flight_view.hpp), and the new
[flight_view_test.cpp](../tests/flight_view_test.cpp). `Result::cancel` remains
in the interface for compatibility with the concurrently edited game-window
caller, but the traced input paths no longer produce cancellation.

The test covers 640/800/1024 canvas widths, both east-open states and both
final-stage states: twelve combinations. It checks clamped keyboard navigation,
selection across pointer motion, ignored cancellation and middle-button inputs,
row edges, Moscow availability, Brussels refusal through both mouse and keyboard,
selection after accepted/refused clicks, reopening and inactive-dialog behavior.
An owned empty directory outside the checkout supplies the asset root; no game
art, renderer, save files or original language text is needed.

The same test fails 162 assertions against the baseline presentation sources
and passes against the corrected sources. The test uses synthetic town labels.
Its reference rules come from the static trace, not a running original binary.

## Integration handoff

The shared CMake and game-window files are under active concurrent development
and were not edited. Add this target alongside the existing tests:

```cmake
add_executable(flight-view-test tests/flight_view_test.cpp
    src/flight_view.cpp src/sdl_assets.cpp src/text.cpp)
target_compile_options(flight-view-test PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(flight-view-test PRIVATE dtvr-formats PkgConfig::SDL2)
add_test(NAME flight-view COMMAND flight-view-test)
```

This exact target was built in an isolated source export; its registered
`flight-view` CTest passed.

The test uses the no-TTF text implementation and never calls `draw()`. Build and
run it with:

```sh
cmake --build /path/to/build --target flight-view-test
ctest --test-dir /path/to/build -R '^flight-view$' --output-on-failure
```

When reconciling the shared `docs/formats.md` Flugzeug section, replace the
assumed hover, wrapping and cancellation behavior with the traced input rules.
Selected-row white text is confirmed. Window placement remains outside this
audit, and the current presentation's text/background geometry was not changed.

One separate presentation gap remains: successful Up/Down selection changes
call seg1:2C9B with channel 2, sound 2 and loop count 0 (seg3:34A8–34B3 and
34F6–3501). Boundary presses do not play that sound. The current `FlightView`
does not expose a sound cue; adding it requires coordinating the shared caller.
No simulation state or save-format change is required for these input fixes.
