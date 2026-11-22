# Original save/load dialog: input audit and integration handoff

Reference: `rally.exe` SHA-256
`aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83`.
Native baseline: `fde95e0` (the relevant presentation sources are unchanged
from `1e73942`). The findings below come from static executable analysis and
synthetic SDL input tests, not interactive execution of the Windows game.

## Confirmed behavior

| Operation | Original evidence | Audited native behavior | Correction |
| --- | --- | --- | --- |
| Open either dialog | seg8:B24E selects slot 0; B253 selects button 6, Ok | Retains the last slot; selects Ok | Reset slot on every opening |
| Select a slot | seg8:BA3B: rectangle x `[0,558)`, y `[(slot+1)*42, (slot+1)*42+41)`; hit test at B944 | Every pixel of each 42-pixel strip selects a slot | Preserve the one-pixel noninteractive separator |
| Click Nein | seg8:B974–B9AF: x `[0,150)`, y `[420,462)` posts Escape | Entire left half of footer cancels | Use the traced rectangle |
| Click right footer | seg8:B9B4–B9EF: x `[458,558)`, y `[420,462)` posts Enter | Entire right half selects Ok and confirms | Use the traced rectangle and retain the selected keyboard button |
| Click footer middle | Neither footer rectangle matches | Cancels or confirms | No action for x `[150,458)` |
| Double-click left button | WM_LBUTTONDBLCLK `0x203` dispatches to seg8:B335; posts Enter | Treated as another ordinary selection/click | Enter on the second click inside the dialog |
| Move pointer | WM_MOUSEMOVE `0x200` has no explicit handler in seg8:B658–B69D | Changes highlighted footer button | Leave keyboard selection unchanged |
| Middle mouse button | No explicit middle-button handler in that dispatcher | Treated as a left click | Ignore it |

The window class helper seg6:1484 sets its style to `0x8` at seg6:1498,
enabling double-click messages. The dialog registers its procedure seg8:B301
through that helper at seg8:B212–B218.

The keyboard implementation already matches the inspected paths: Left selects
Nein (7), Right selects Ok (6), Up/Down clamp the slot to 0–8, Escape and Backspace
close. Enter evaluates the selected button at seg8:B39A. Right-button down
`0x204` dispatches to seg8:B349 and sends Backspace.

The right footer's behavior is intentionally unusual: after pressing Left,
clicking that right region posts Enter and therefore cancels. Double-clicking
a slot also uses the selected footer button. Do not replace these paths with
unconditional confirmation when reproducing the original.

## Prepared implementation

The corrections are isolated to [save_view.hpp](../src/save_view.hpp) and
[save_view.cpp](../src/save_view.cpp). They change selection and hit testing;
the rendering, save format and save-file operations are unchanged.

[save_view_test.cpp](../tests/save_view_test.cpp) exercises the dialog at all
three canvas widths (640, 800 and 1024), without a renderer, original assets or
save writes. Coverage includes reopening in both modes, row boundaries and
separators, footer boundaries, keyboard selection across mouse movement,
double-click, unsupported mouse buttons and inactive-dialog handling.

The same test was compiled against both the baseline presentation sources and
the corrected sources, with the isolated baseline `dtvr-formats` library.
The baseline fails 72 assertions; the corrected implementation passes all of
them. Source-only repository checks also pass. No visual rendering comparison
or interactive Windows run is claimed by these input tests.

## CMake handoff

The shared root CMake file is under active concurrent work, so it was not edited.
Add the following alongside the other test targets when integrating:

```cmake
add_executable(save-view-test tests/save_view_test.cpp src/save_view.cpp src/text.cpp)
target_compile_options(save-view-test PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(save-view-test PRIVATE dtvr-formats PkgConfig::SDL2)
add_test(NAME save-view COMMAND save-view-test)
```

This exact integration was also built in a separate exported source tree;
the registered `save-view` CTest passed there.

The test intentionally uses the existing no-TTF text implementation: it only
constructs the dialog and submits events. `dtvr-formats` already contains
`save_slots.cpp`, providing the symbols referenced by the unused draw path.

Then build the target and run:

```sh
cmake --build /path/to/build --target save-view-test
ctest --test-dir /path/to/build -R '^save-view$' --output-on-failure
```

When reconciling the shared `docs/formats.md` save-dialog section, replace the
assumptions about the initial Ok button and half-width footer hit regions with
the confirmed rules above. The previously claimed retained slot selection in
`SaveView` is also incorrect. Dialog placement remains outside this audit.
