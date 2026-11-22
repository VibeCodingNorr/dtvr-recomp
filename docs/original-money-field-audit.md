# Blue/red field arrival: missing message window

Original `rally.exe` SHA-256:
`aa3dde360f4d53501933be3648da4f8a019f220cf8dfda59e9e9d6a601786b83`.
Audited native baseline: `78892d79e304f68f765026f88b047e323d9dffe0`.
The original hash and segment 2 were verified against the private disassembly.

## Confirmed behavior and missing native call

Both money-field handlers call the message window on arrival, after adjusting
cash and playing the field sound, and before running Dr. Drago's field action:

| Field | Text selection | Sound | Message call | Dr. Drago afterward |
| --- | --- | --- | --- | --- |
| Blue, kind 1 | seg2:0E95–0EAE: 4001 + random(14) | seg2:0EF1: channel 3, index 14, loops 0 (`moneywin.wav`) | seg2:0F05 calls seg6:51EA | seg2:0F0D |
| Red, kind 2 | seg2:1025–103E: 4101 + random(13) | seg2:1081: channel 3, index 1, loops 0 (`Fate03.wav`) | seg2:1095 calls seg6:51EA | seg2:109D |

Formatting at seg2:0EBD–0EE8 and 104D–1078 supplies the player name followed
by the formatted, nonnegative gain/loss amount. All 29 matching language texts
were checked to have two `%Fs` placeholders. The red handler subtracts a positive
amount from cash but displays that positive amount in its loss message.

The existing native `Game::arrive()` already computes the correct `textId`,
updates cash and queues these field sounds. It writes the text ID into the CLI
log but does not queue a `Notice`. The SDL game loop consumes `takeNotices()` for
the message window, so the log entry alone cannot display the original dialog.

This refers to stopping after the movement allowance is exhausted. Passing over
a blue or red field during the move should not produce its arrival dialog.

The red field's direct sound is confirmed as `Fate03.wav`. This audit did not
audition that recording and does not identify its audible content as laughter.
The separately named `Lachen01.wav` and `Lachen02.wav` are explicitly used by
Dr. Drago's spending action (seg2:BB5E–BB84), which may follow a money-field
message when he rides with that player. No additional laugh sound is proposed
without evidence.

## Tested integration change for the parallel engine work

`src/game.cpp`, `src/game.hpp`, the save revision and the shared game test were
under active concurrent modification during this audit. They were not edited.
The fix below was applied only to an isolated export of the audited commit.

In the money-field branch of `Game::arrive()`, after the existing CLI message
and **before** `dragoAct(current_)`, add:

```cpp
// seg2:0F05 / 1095: message with player and unsigned gain/loss amount.
notice(e.textId, {playerArg(current_), moneyArg(f.kind == 1 ? e.amount : -e.amount)});
```

This uses the already selected text and amount. It consumes no additional random
draws and changes no authoritative state or save layout. The existing field
sounds remain in place. Keeping the Notice before `dragoAct()` preserves message
order when he also acts at the stop.

## Regression test and results

[money_field_notice_test.cpp](../tests/money_field_notice_test.cpp) uses only
synthetic boards and in-memory games. It checks 128 seeds for each field kind,
256 arrivals in total. Coverage includes no field dialog while passing, exactly
one arrival Notice, original text ID and player/positive-amount arguments,
formatting, the existing driving and field sound cues, unchanged cash calculation
and RNG progression, and no duplicate Notice after a rejected extra step.

The unpatched baseline fails 256 assertions, all for the missing arrival Notice.
With the isolated fix the new test and existing `rules`, `game`, `game-cards` and
`save` CTest suites all pass. These are simulation/queue tests; a desktop dialog
and an audible comparison with the Windows original were not exercised here.

Register the test after incorporating the arrival fix:

```cmake
add_executable(money-field-notice-test tests/money_field_notice_test.cpp)
target_compile_options(money-field-notice-test PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(money-field-notice-test PRIVATE dtvr-formats)
add_test(NAME money-field-notice COMMAND money-field-notice-test)
```

The same CMake target was built and tested in the isolated export. No original
media or analysis output was added to the repository.
