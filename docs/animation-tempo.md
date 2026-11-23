# Optional animation tempo

Status: the user endorsed adjustable animation tempo on 2026-11-23. **First
increment implemented (round 125):** `src/presentation_clock.{hpp,cpp}` (no SDL,
tests in `tests/presentation_clock_test.cpp`), `[Extensions] AnimationTempo` (1, 2,
4; others read as 1) and **Animationstempo** on the Spiel page of the recomb menu.
The clock drives the event animations (`GameScreen::startEventScene` /
`eventSceneActive` and their camera) and the vehicles' drive including their frame
animation; it stops while the recomb menu is open. Real time stays with input,
the time limit, computer-message reading time, the slot machine, Risiko, the
ambient map animations, palette cycling, music, speech, subtitles and films.
Not yet: finish / victory / balance sequences (voiced), "Computerzüge schneller".
The proposed levels and first implementation scope below remain design choices,
not recovered original behavior.

## Player experience

Add **Animationstempo** under **Spiel** in the native Escape menu, with
**Original (1×)**, **Schnell (2×)** and **Sehr schnell (4×)**. Default to original
speed and preserve the local preference across restarts. Apply a change without
requiring a new game or restarting an animation. A later **Computerzüge schneller**
option can restrict acceleration to computer presentation once turn transitions
are verified.

Keep the existing **Wartezeit der Meldungen** setting separate. It reproduces
`[Setup] Pause`, which multiplies computer-message waits by 1, 1.5, 2 or 3;
it is not an animation-speed setting. Avoid making faster scenes also dismiss
their explanatory messages sooner.

## First implementation scope

Start with presentation-only event and finish/victory animations. Extend to
vehicle travel and other scenes after their timing dependencies have been
audited with the current game-window owner. Preserve every movement step,
field effect and event in the simulation even when fewer frames are displayed.

Leave music, speech, subtitle synchronization and voiced films on their media
timeline. Keep human input windows, turn deadlines and the timed Risiko phase
on their authoritative time source. The slot machine also needs a separate
review because pressing a key can affect when it resolves. Skipping whole
sequences requires its own handling of pending effects and notices and is
deferred from the first increment.

## Integration for Claude

The active game-window work currently owns `src/game_window.cpp`,
`src/game_screen.*`, `src/settings.*`, `src/recomp_menu.*` and the vehicle
animation modules. Coordinate changes there; this handoff does not change them.

Use a presentation clock with accumulated elapsed time, retaining the original
20 ms scene steps. Do not replace every `SDL_GetTicks()` call with a multiplied
timestamp: the loop also uses time for input, random seeding, messages and game
deadlines. At a speed change, first account for elapsed time at the old rate,
then use the new rate for subsequent intervals. Preserve fractional elapsed
time so many short updates equal one longer update. Opening and closing the
pause menu must not cause an animation jump.

Store the preference as a local extension setting, separate from the original
`Animations` switch and `Pause` setting. Validate loaded values and fall back to
original speed for unsupported values. It is presentation state and does not
belong in the shared ruleset or game save format. In future multiplayer each
client may choose its own presentation tempo; readiness and game progression
must remain authoritative.

## Acceptance targets

- At default speed, verified original scene timing remains unchanged.
- A two-second eligible scene takes one second at 2× and half a second at 4×.
- Changing speed mid-scene preserves its current frame position and pending
  events; pauses resume without adding the time spent in the menu.
- A fixed initial simulation state, seed and accepted command sequence produce
  the same settled game state at every presentation speed.
- Timed player decisions retain their duration. Speech and subtitles stay in
  sync; computer messages retain their separately selected reading time.
- Invalid settings recover to original speed; choosing a valid speed persists
  across restart. Verify scene timing with an injected clock and inspect
  externally stored rendering captures before claiming native integration.
