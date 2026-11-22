# Original card shop: input audit and integration handoff

Reference: `rally.exe` SHA-256
`aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83`.
Native baseline: `ce7e9dd21a402716e46c942e160f57385f4f75ed`.
The hash and segment 3 were verified against the private disassembly. These
findings combine static executable analysis with synthetic native input tests;
they do not claim interactive testing of the Windows game.

## Confirmed rules and isolated corrections

| Operation | Original evidence | Audited native behavior | Correction |
| --- | --- | --- | --- |
| Single click on a card | seg3:5148–5166 assigns DS:A0EA and redraws selection/info | Buys/sells if the same card was already selected | Every ordinary click only selects |
| Left double-click | Message `0x203` dispatches through seg3:4CDE to 4674, posting Enter | No explicit double-click distinction | Use SDL's second-click event as Enter |
| Header Left at Kaufen | seg3:4A4A–4A53 returns immediately when choice is zero | Wraps to Beenden | Remain on Kaufen |
| Header Right at Beenden | seg3:4ACC–4ADA sets choice to zero | Wraps to Kaufen | Already correct; preserve the asymmetry |
| Up in buy/sell mode | seg3:4B42–4B71 subtracts nine if selected index is at least nine | No action | Move one grid row up |
| Down in buy/sell mode | seg3:4B79–4BB3 adds nine only if that index exists | No action | Move one grid row down without changing column |
| Escape/Backspace from buy/sell mode | seg3:46D6–46F9 restores header choice, with no sound call | Always emits sound 2 | Return silently |
| Escape/Backspace from header | seg3:46AB–46BC plays sound 2 and closes | Plays sound 2 and closes | Already correct |
| Header hit bounds | seg3:5051–5101 uses `PtInRect`, requiring nonnegative client coordinates | Negative x can activate Kaufen | Ignore pointer coordinates outside the dialog |

The same window-class helper as the save dialog is used: seg3:44F1–44FC calls
seg6:1484. Its class style is `0x8` (double-click messages). Two separate single
clicks on the same card must therefore remain distinct from a double-click.
The double-click handler posts Enter regardless of the card/header hit regions,
so a double-click on blank space inside the dialog acts on the current selection.

Other inspected behavior is already correct:

- seg3:453A–4544 initializes header mode, no selected card and Kaufen.
- seg3:5051–5101 uses header x ranges `[0,110)`, `[180,320)` and `[460,558)`,
  all at y `[0,42)`; the gaps have no action.
- seg3:51B8–5224 constructs card hit rectangles from `(15,51)`, stride `(60,74)`,
  nine columns, width 44 and height 63, with exclusive right/bottom bounds.
- seg3:4A96–4ABB and 4B0C–4B3A clamp horizontal card selection.
- seg3:4CE3 routes right-button down (`0x204`) to seg3:4688, which sends Backspace.
- seg3:475E rejects entering sell mode when the empty-hand flag DS:3FBA is set.
- seg3:4737–473D and 478F–4795 save the header choice before entering buy/sell;
  seg3:46E0–46E3 restores it when returning. Retaining native `choice_` across
  these modes gives the same input behavior.

## Prepared implementation and tests

Only [card_shop_view.cpp](../src/card_shop_view.cpp) and the new
[card_shop_view_test.cpp](../tests/card_shop_view_test.cpp) implement this patch.
It changes event handling; rendering and simulation transaction rules are not
changed. The existing `Action` interface needs no caller modification.

The regression test checks all three canvas widths (640, 800, 1024), ordinary
clicks versus double-clicks, vertical and horizontal selection boundaries,
card/header hit edges, return sounds and restored header choice, disabled empty
hand selling, and mapping a sell-list entry to a non-contiguous hand slot.
It uses a synthetic board and in-memory games; one owned external temporary
directory holds a generated 1-pixel palette BMP. No original media, renderer,
save files or installed game data are used.

The baseline fails 183 assertions; the corrected implementation passes the
same tests. The test emits buy/sell actions without applying each emitted action;
it is an input regression test, not an end-to-end transaction test.

## CMake handoff

The root CMake and game-window files remain under concurrent development and
were not edited. Register the test with:

```cmake
add_executable(card-shop-view-test tests/card_shop_view_test.cpp
    src/card_shop_view.cpp src/sdl_assets.cpp src/scene.cpp src/text.cpp)
target_compile_options(card-shop-view-test PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(card-shop-view-test PRIVATE dtvr-formats PkgConfig::SDL2)
add_test(NAME card-shop-view COMMAND card-shop-view-test)
```

This exact target was built in an isolated source export, and the registered
`card-shop-view` CTest passed there.

The target uses the no-TTF text implementation and does not draw. To verify:

```sh
cmake --build /path/to/build --target card-shop-view-test
ctest --test-dir /path/to/build -R '^card-shop-view$' --output-on-failure
```

When reconciling shared `docs/formats.md`, replace the assumptions about a second
click and right-button cancellation with the confirmed rules. Window placement
remains outside this audit.

## Separate gaps requiring presentation/transaction coordination

- **After selling the last card:** seg3:49EB–49FB adjusts the selected index
  after rebuilding the hand list. seg3:4A1B–4A39 then posts Backspace if the hand
  is empty, returning to the header rather than closing the shop. The current
  caller applies `Game::sellCard` but does not notify the view to make that
  transition. Coordinate a successful-transaction refresh with the shared
  game-window caller, including selection after shrinking lists.
- **Header preview:** changing header choice calls seg3:5813 from seg3:4A75–4A7E
  or 4AEB–4AF4 to rebuild the displayed list. Current `items()` always shows
  offers in header mode. Audit the preview's full choice semantics and verify
  screenshots before changing its rendering.

These gaps are recorded for follow-up; the input test does not cover them.
