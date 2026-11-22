# Original behavior audit: game start and first player change

Audited native revision: `1e73942ffe4363381a2ebec3704a0e059f0b21e2`.
Original `rally.exe` SHA-256:
`aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83`.

This is a static executable comparison with a synthetic native simulation probe,
not an interactive test of the original Windows game. All original code segments
and the data segment were compared byte for byte with the private disassembly
before using its annotations. No original data or disassembly is included here.

## Confirmed difference: restarting does not reproduce the initial event delay

The original initializes the event countdown using the player slots that exist
**before** the new game's player setup. The native new-game path always begins
with a zero countdown, without accepting the previous slot count.

Evidence, in execution order:

1. **seg4:0DEE–0DFA:** clears `0x25` bytes starting at DS:A0FE. This includes the
   calendar year at DS:A102 and event countdown at DS:A108.
2. **seg4:0E2F:** calls seg8:1B91 before seg4:0E37 calls the player reset/setup
   routine seg2:0000.
3. **seg8:1BA5–1BD1:** computes
   `(10 - (year >> 5)) * activeSlots + random(activeSlots)` and stores it in
   DS:A108. Both slot-count calls use seg2:0368, which counts nonzero statuses
   in the existing player records. At this point the cleared year is zero.
4. **seg2:00C4–00CD:** only now clears the player statuses; seg2:020D reloads
   the requested configuration through seg1:02BA. A lone player's computer
   opponent is also added later, at seg2:021F ff.
5. **seg8:16AD–16C6:** on a player change without a season reload, a positive
   countdown is decremented and the handler returns. Only an already-zero
   countdown schedules a new delay and enters event processing.

Consequently, if an existing two-player game is followed by another new game,
the initial delay is **20–22 event ticks**, calculated from the old two slots.
The first event-processing attempt is on the next eligible tick after the
countdown reaches zero. Season reloads return before decrementing this clock.
Changing the requested player count does not change which count this initial
calculation used.

Native counterparts:

- `EventClock` in [events.hpp](../src/events.hpp) defaults `countdown` to zero.
- The [Game constructor](../src/game.cpp) never initializes it.
- [PlayView::newGame](../src/play_view.cpp) constructs a fresh `Game` and loads
  that state into the existing instance; it does not transfer the old slot count.
- [Game::eventTick](../src/game_events.cpp) therefore enters event processing on
  the first eligible player change, instead of initially decrementing a delay.

For a matching two-player restart this is a confirmed control-flow difference.
Depending on the selected event and its preconditions, an event-processing
attempt may have no visible effect; it should not be described as a guaranteed
visible event on turn one.

**Do not fix this by unconditionally using the new player count.** When the old
slot count is zero, the traced original calculation yields zero too. seg6:1357–
1362 returns zero for `random(0)` without consuming `rand()`. The initial player
allocation at seg5:1794 ff. uses `GlobalAlloc(0x42, 0x2910)`, including zero
initialization. A fresh process and a restart must therefore be tested separately,
including any intervening title/demo behavior that populates those slots.

For a positive old count, the original countdown initialization also consumes
one RNG draw before setup and card shuffling. The native constructor omits this
draw. This affects subsequent random state if both paths enter that operation
with the same RNG state; a full seed-to-screen equivalence test remains open.

## Confirmed matches in the inspected startup path

- **Card data:** all 41 category, copy-count and double-valued price records in
  native `cardTypes()` match DS:2EB2, including the 0xFF terminator and a total
  of 199 card copies.
- **Shuffling:** seg4:0E67 calls seg3:01AD; seg3:0206 performs `count * 10`
  swaps using two `rand() % count` draws each. This matches `Deck::shuffle`.
- **Dealing:** seg4:0E6C calls seg3:0286 after shuffling. Active slots receive
  their whole initial hand in ascending player-slot order. Inactive slots are
  skipped. This matches the constructor's nested loops.
- **Starting-card option:** seg2:02C8–02FC reads the default of one, maps zero
  to three and caps values above eight at eight. This matches the supported
  nonnegative input behavior of `startingCards()`.
- **First destination:** seg4:0E92–0E94 calls seg7:372F after dealing;
  seg7:374D–3755 invokes seg5:1E2C to select the destination. The native
  relative ordering of shuffle, deal and destination is correct. This does not
  establish that every intervening random draw has been reconstructed.
- **Card categories:** seg4:094F–0951 opens category 2 for the main card command;
  seg1:549E–54C1 opens category 1 in the town dialog only when no property-card
  mode is active. The category mask is tested at seg3:0701–0715. These paths
  support the corresponding restrictions in `Game::playCard`; this audit does
  not establish every possible UI entry point or timing condition.

## Native verification and handoff

An isolated export of the audited commit was built outside the checkout.
The existing `rules`, `game`, `cards` and `events` CTest suites all passed.

A private synthetic probe used an 80-field ring, London at field 0, another
destination at field 79, and blue fields between them. It exercised two through
eight human players with eight fixed seeds, 56 cases in total. Every fresh native
game had a zero initial countdown. After one spin, movement and player change,
every case had already rescheduled its countdown. Starting hands, destination
and player progression passed the probe's assertions. The reference restart
calculation was positive in all 56 cases; its zero-old-slots control consumed no
RNG draw. The reference calculation comes from the static trace, not execution
of the Windows binary.

Suggested integration work, separate from this audit:

1. Model the pre-setup startup context explicitly, including the previous active
   slot count and the intended handling of RNG continuity. Keep it independent
   of SDL and window construction.
2. Cover old/new slot counts `0/2`, `2/2`, `2/4`, `8/2`, and the lone-player
   computer insertion. Distinguish process startup, demo exit, menu restart and
   loading a save. Do not reinitialize the saved countdown on load.
3. Verify decrement-to-zero timing and the season-reload bypass. Add a regression
   that would fail with the audited constructor's zero-only initialization.
4. Update the rules revision if behavior changes. The countdown already exists
   in the save schema; whether a layout version changes depends on any new state.

The simulation and shared integration files were not changed by this audit.
