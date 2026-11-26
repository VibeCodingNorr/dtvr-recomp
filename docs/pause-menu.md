# Native start and Escape menus

Status: the in-game menu is implemented (`src/recomp_menu.{hpp,cpp}` model,
no SDL; `src/recomp_menu_view.{hpp,cpp}` drawing and input; tests in
`tests/recomp_menu_test.cpp`). Escape opens it on the map, where rally.exe's Escape
opens RIGHTBTN like Backspace (seg4:01D1; deliberate deviation), and in a menu without
a checked item such as ONFIELD, where rally.exe's Escape does nothing. Backspace and
the entry "Originales Spielmenü" keep RIGHTBTN. In a game without humans and with no menu open, Escape (and
a right click) still end the game and return to the title first, as seg1:021D does.
Deliberate deviation: in the title menu MAIN, rally.exe's Escape posts the checked item (seg5:543D:
GETMENUSTATE & MF_CHECKED → WM_COMMAND), which is "Neues Spiel"; the recomp opens its
menu there instead. MAIN1 (Weiter spielen) and RIGHTBTN (Zurück) keep seg5:543D. Pages: Pause, Grafik,
Audio, Spiel, Steuerung. While it is open the game loop treats it as modal and the
time limit stops. The Audio page holds the master volume and the sound set (F8
opens it; the former F8 overlay is gone); the options window opens the menu through
its recomp entry "Weitere Einstellungen". Separate effect / speech / music levels are
deliberately not offered: rally.exe only switches them on and off at full volume
(round 132). The controller's Start (Menu) button opens and closes it on the Pause page
(src/game_window_pad.cpp); Tab / Shift+Tab (the pad's RB / LB) go to the next /
previous page, staying in the entries when the page has any. The page Steuerung
(`controlsPage(PadMap)`, pad rounds 3 and 4) is a table page with the columns
Aktion / Tastatur / Gamepad, drawn compactly in one font size that fits every cell,
with its own footer hint (the [Setup] Joystick gate, merged pads). It shows twelve
rows at once and scrolls with the focus (▲ / ▼ in the heading row; the mouse wheel
moves the focus). Rows: one **Binding** row per pad action (four directions,
Bestätigen, Abbrechen, Recomp-Menü, Originales Spielmenü, Schließen/Löschen,
Karte/F9, Seite zurück / vor), the stick choice "Fahren / Umfeld (Sticks)" (Links /
rechts or Rechts / links), four info rows (names, the on-screen keyboard's
shortcuts, stage count, display keys) and
the command **Standard wiederherstellen**. Enter / A on a Binding row waits for a
pad button ("Taste drücken …", the footer counts down 8 s); the pressed button is
bound, and when another action had it the two swap and the footer says so
("Bestätigen jetzt auf B. Getauscht: Abbrechen jetzt auf A."). Escape / Backspace
or any click cancel the wait; without a pad it ends at once. Every action always
keeps a button of its own, so the menu stays reachable with the pad; keyboard and
mouse always work on the page. Changes are saved at once in `[Gamepad]` of the
recomp settings (docs/formats.md "Recomp pad layer", round 4). The texts are recomp
texts and must follow `src/pad_input.cpp`, `src/pad_map.cpp` and the game window's
keys; `tests/recomp_menu_test.cpp` checks every action's row, capture, swap,
timeout and reset. The footer's key line and the on-screen keyboard's help line name
the buttons in use (Tab/RB, Enter/A, Esc/B by default).
Pause commands (`planCommand`, `src/game_window_input.cpp`): Weiterspielen,
"Originales Spielmenü" (replaces the menu with RIGHTBTN) and "Rallye beenden" (the
recomp menu hides for the original quit question; a No brings it back on the same
entry). The Pause page has no save or load entries (user decision, 2026-11-26):
saving and loading stay with the original menus as in rally.exe, MAIN1's "Spiel
speichern" (1062, seg4:07B8 → seg8:B157(1)) and the title menu's "Spiel laden"
(1021). The pad's A is exactly the keyboard's Enter here (one Return per press,
src/pad_input.cpp).
Not yet: help pages and multiplayer entries. Keyboard keys are not remappable
(user decision). The sections below are the earlier design draft; where they differ
from the implemented menu above, the implemented menu and the original windows win.

## Before a game

The game starts with rally.exe's own sequence (logo, intro, title menu MAIN from
the language file); Escape in MAIN opens the recomp menu (see above). A native
start screen with our own logo is not implemented, and the agreed direction is the
original look (the draft below is kept for reference only).

### Draft: native start screen

The start screen uses our own project logo and offers New game, Continue/Load,
Multiplayer, Settings, Help and Quit. Show New game, save or multiplayer entries
as unavailable with a short explanation until those systems are implemented.
Explain missing imported data and provide a path to installer guidance; do not
execute the original Windows installer or download game assets.

Settings share the same layout before and during a game. Show imported content
and installed sound sets through a readable status page, not private filesystem
details in the main player flow. A diagnostic view may offer technical details.

## During a game

Escape first dismisses the current modal dialog or cancels a pending UI selection
where that is allowed. Otherwise it opens the pause overlay; pressing Escape
again returns to the game. Do not let the held key immediately close and reopen it.
The controller's Start (Menu) button reaches it; View/Back opens the original
RIGHTBTN menu on the map instead (docs/formats.md "Recomp pad layer").

Draft entries (implemented: Resume as Weiterspielen, Audio, Graphics as Grafik,
Controls as Steuerung, Quit as Rallye beenden; the others are not):

| Entry | Purpose / behavior |
| --- | --- |
| Resume | Close the overlay and restore the previous UI focus. |
| Audio | Master volume (no separate music / speech / effect levels, see above), music and effect switches and installed Ogg sound-set selection. |
| Graphics | Window/fullscreen, scaling, interface scale and optional installed effects. |
| Controls | Controller bindings (keyboard keys stay fixed), reset defaults and visible focus navigation. |
| Accessibility | Reduced motion, color-independent field markers and readable text options. |
| Help | Controls, contextual rules explanations and the financial/event history. |
| Save / Load | Not in the recomp menu (user decision): the original MAIN1 and title menus save and load. |
| Multiplayer | Connection state and a pause request/resume action when networking exists. |
| Return to menu/lobby | Show the impact on the current session and protect unsaved progress. |
| Quit | Clear exit action; warn about unsaved progress when there is any. |

Keep renderer APIs, source addresses, decoder settings and developer tools out of
the normal menu. Put a developer overlay behind a separate explicit switch.

## Pause and network behavior

In single-player, an agreed simulation pause stops rule deadlines and AI progress.
The player can navigate settings and choose a separate audio pause policy without
the music clock driving game rules. Reproduce original activation behavior where
appropriate, while allowing an explicit local "music while paused" preference.

In multiplayer, opening a client's settings must not pause every player or
silently suspend the authoritative turn deadline. Offer a separate pause request
whose approval and state are handled by the host according to the agreed session
policy. Show clearly whether only the local menu is open or the session is paused.
Changing local volume, sound set, scaling or effects is not a shared rule change.
Rule settings and loading a saved game belong to an authoritative session flow.

## Music-pack choice

Offer an "Audio / Soundtrack" choice for installed complete packs: CD recordings
with documented rendered gaps, a GM rendering, or another compatible supplied
sound set. See [music research](music-research.md). A menu label must describe the
actual rendering; do not label a generic bank as authentic Roland hardware.

The game consumes local Ogg files for all packs. Missing cues, a partial extracted
intro or an absent render bank must be visible as incomplete; do not silently
enable a pack that leaves original music events without assets. Changing packs
should happen at a cue boundary until timeline mappings have been verified.

## Verification targets when implemented

- Open/close with keyboard and controller; preserve focus and pending selections.
- Verify single-player pause and multiplayer local-menu behavior separately.
- Distinguish successful cue completion from stop, pause and pack changes.
- Persist local preferences without changing rule state or network ownership.
- Keep unavailable entries understandable and confirm actions that discard progress.
