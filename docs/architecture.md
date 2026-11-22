# Native game architecture and modernization

Status: agreed direction and implementation requirements. These systems are not
implemented by the current data viewer. Original game behavior remains the
reference; preserve evidence and label deliberate changes.

## Simulation boundary

Keep simulation independent of SDL, rendering, audio, input devices and network
transport. Parsing original formats produces validated data consumed by the game.
Local input, AI and remote input submit the same explicit game commands. A single
authority validates commands, advances state and emits ordered game events.

Model commands around recovered rules, such as choosing a route or buying a
property. Do not define guessed gameplay semantics before inspecting the original.
Presentation consumes state and events; animation completion must not determine
simulation results. Use stable entity and player identifiers rather than pointers
or renderer objects in saved state or messages.

The authority owns random outcomes and gameplay deadlines. Persist random state
and its algorithm/version along with the game state. Use simulation time or
explicit authority events for deadlines, never client animation timing. Log
resolved outcomes so replay need not depend on an unspecified platform RNG.

## Multiplayer

One host owns the authoritative state. Clients send commands, not replacement
states, random outcomes or account balances. The host checks sender ownership,
current turn, phase and action legality before applying a command.

Commands carry a session identifier, unique command identifier and expected state
revision. Responses acknowledge acceptance or rejection; accepted events have
monotonically increasing revisions. Duplicate commands must not apply twice.
Clients can request a fresh snapshot if revisions are missing or inconsistent.
Network messages need explicit size limits and validated fields.

Player slots and connections are separate: several local hotseat players can
share one client, alongside remote players and AI slots. Local play uses the same
authority boundary without requiring a network socket.

### Delivery order

1. Recover a complete human turn and make its state serializable.
2. Add a host/join lobby over LAN with direct address entry and explicit player
   slot assignment. Later add discovery and local-machine naming.
3. Add reconnect using session identity, a reconnect credential and an authoritative
   snapshot. Pause on player disconnect by default; AI replacement is an explicit
   host decision. Agree a deadline policy before timed multiplayer is enabled.
4. Support saved multiplayer sessions, preserving slot ownership and rules.
5. Extend transport to Internet play. Direct host connections are an initial path;
   convenient connectivity may require rendezvous and relay services. Service
   hosting, credentials and maintenance require a separate implementation decision.

Match protocol version, ruleset version and a manifest of gameplay-relevant data
before starting. Allow local cosmetic settings to differ. Keep transport separate
so Internet support does not require rewriting the recovered game rules.
Host migration, matchmaking, accounts and a public server browser are deferred.
Host disconnection initially ends the connection; a persisted session enables
manual recovery, not automatic host migration.

## Save files and event history

Use a versioned native save format, independent of whether original saves are
eventually decoded. Include game/rules versions, original-data identity, revision,
players, current phase, rule settings and random state. Write snapshots atomically
and rotate autosaves. Reconnect credentials are private session data and must not
be exposed in a shared replay or public diagnostic log.

Record ordered accepted actions and resolved game events for a readable financial
and event history. Replays and diagnostics can build on this log; a full replay
feature is deferred until state transitions and version compatibility are tested.

## Usability scope

- Scalable interface, window/fullscreen support and readable text on modern displays.
- Adjustable animation speed, fast AI presentation and reduced motion.
- Autosave history and restore points.
- A readable record of money changes and events with their causes.
- Symbols or patterns in addition to field colors.
- Separate music, speech and effect volume controls.
- Preserve hotseat alongside LAN and eventual Internet play.
- Remappable keyboard and controller bindings, with reset to defaults and visible
  prompts for the active input device. Save bindings locally per device/profile.
- Full controller navigation through menus, route choices and dialogs, with clear
  focus indicators. Route every input device through the same game commands.

Original rules are the baseline. Balance changes, new events and changed turn
limits belong to an explicit ruleset selected by the host and stored in saves.
Visual and audio preferences belong to each local client, not the shared ruleset.

### Feature backlog

All entries below are planned and remain unimplemented. Start them once the
recovered gameplay or rendering layer they depend on exists.

| Priority | Feature | First acceptance target |
| --- | --- | --- |
| Early | Key rebinding | Rebind an action, persist it across restart, detect conflicts and restore defaults. |
| Early | Financial and event history | Show turn, player, amount, cause and resulting balance from authoritative events. |
| After input actions exist | Controller support | Complete a turn and navigate every required dialog without a mouse; handle disconnect without submitting actions. |
| After scene/UI separation | Optional shader presets | Toggle effects at runtime, keep text readable and support an effects-free fallback. |
| After suitable layers exist | Subtle parallax | Move background layers independently while board positions and hit targets remain aligned; honor reduced-motion settings. |
| After save/event stability | Replays | Replay a saved initial state and recorded outcomes to the same final state; reject incompatible versions clearly. |
| After road graph and rules are verified | Optional route planner | Highlight legal routes, distances and known costs using recovered rules; allow the assistance to be disabled. |
| After action semantics are verified | Context help | Explain fields, cards and available actions, including known financial consequences; distinguish certain effects from random outcomes. |
| After complete state snapshots | Decision checkpoints and undo | Offer optional single-player undo at defined decision boundaries; multiplayer undo requires unanimous consent and an authoritative state revision. |
| After network turn ownership | Turn notification | Provide an optional visual or audible notification when a local player becomes active, respecting local sound preferences. |
| After multiplayer saves | Resume together in lobby | Load a shared saved session, explicitly reassign player slots and confirm ownership before resuming. |
| After authoritative event history | End-of-game statistics | Display wealth over time, stage wins, property income and largest gains/losses from recorded events. |
| After original rules are validated | Optional rule variants | Offer clearly labelled shorter games or changed starting conditions; agree settings in the lobby and persist the ruleset in saves. |

These entries capture the discussed ideas, not a commitment to implement all of
them before the original game is playable. Context help and route planning are
preferred next usability candidates once their underlying data is verified.
An undo policy must address already revealed random outcomes and must not become
an unrestricted reroll mechanism. Treat assistance and undo settings that affect
competitive play as shared session options agreed by the players.

For implementation, split these into focused issues with the relevant acceptance
target and dependencies. This document is the current backlog; creating issues
must not imply that any feature is already implemented.

## Optional graphics

Keep a crisp, unfiltered presentation as the default. Preserve indexed palettes
when decoding original data; reproducing original water and seasonal effects is
part of reconstruction, with details still to be verified.

Candidate enhancements, in order of usefulness:

1. Pixel-aligned rendering and optional integer scaling with letterboxing. Scale
   text independently so it stays legible. Translate pointer coordinates through
   the same camera transform used for rendering. *Implemented:* `[Display] TextSmoothing` (F12)
   0 Original (TTF solid, no anti-aliasing, as GDI text on 256 colours), 1
   anti-aliased at canvas size (default), 2 anti-aliased at the output resolution:
   `TextRenderer` rasterises at the renderer's logical-size scale and draws into
   the canvas rectangle, positioned by the canvas-size extent so layout and line
   breaks do not change. The options window shows it as a labelled line between
   stop and star (a click cycles it). Neither F11 nor F12 is used by rally.exe:
   the main window handles the keys of seg4:0259 and SBBUTTON forwards only
   F1–F10.
2. Optional subtle water highlights, weather overlays and a CRT/scanline preset.
   Isolate scene effects from interface text. Bloom, strong smoothing and distortion
   should never be required to read the original pixel art.
3. Restrained parallax on newly separated background clouds or menu layers, if
   suitable assets and layer boundaries exist. Do not shift roads, board fields,
   towns or input targets independently: their alignment conveys game rules.

Parallax cannot be recovered from a flat overview picture without additional
layers or masks. Additional art is a separate task. Do not assume those layers
exist in the original files.

Render the world and interface separately when adding effects. A future GPU
backend may implement shaders behind a presentation interface; the current SDL
software renderer has no programmable shader pipeline. Retain an effects-free
fallback and headless simulation tests. No GPU API is chosen at this stage.
Allow all added motion, flashes and camera movement to be disabled. Cosmetic
animation may vary across clients without changing gameplay or network traffic.

## Acceptance criteria as systems land

- The same accepted command sequence produces the same authoritative state across
  save/load; invalid or duplicate commands leave state unchanged.
- Simulation can run without a desktop, GPU or audio device.
- Test host/client revision agreement, reconnect snapshot recovery, incompatible
  data rejection, and several player slots sharing one connection.
- Rendering speed, effects and input scaling never alter random results, finances,
  turn ownership or route selection.
- Describe features as implemented only after their respective paths are verified.
