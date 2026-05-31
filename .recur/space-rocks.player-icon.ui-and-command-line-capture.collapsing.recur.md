# space-rocks.player-icon.ui-and-command-line-capture.collapsing

Date: 2026-05-31
State: collapsing
Focus: only the Asteroids player icon, and only the command capture/test loop needed to prove it works during gameplay from both the UI version and the command-driven/exported game.

## Interesting Eventness

The interesting thing is not "Asteroids is broken". The interesting thing is that the player icon is the visible, controllable witness for the whole game loop. If the user presses or records commands and the icon does not visibly exist, rotate, thrust, and fire in both surfaces, the rest of the Asteroids work is noise.

This lane stays narrow: capture user commands against the player icon, replay them, and assert the icon state/rendering from both the UI path and the command-line recipe path.

Related visibility tuning lane: use `space-rocks.player-icon.visibility-trials` when the question is whether the user can actually see the icon during play, including quick 1-to-10 visual setting trials.

## Completed Reality

The same player-icon command sequence can be captured from the UI version and replayed through the command-driven asteroid game. Both paths show a visible classic vector player icon at gameplay start, then prove that rotate/thrust/fire commands affect that same icon during play.

The completed behavior is recipe-backed, not hand-tuned: `exports/space-rocks/space-rocks.commands` creates `player_ship` with `--shape vector-ship --style outline`, and the UI/runtime path renders the same player-icon contract.

## Collapse Record

Collapsed on 2026-05-31.

Saved outcome:

- The UI Asteroids path and exported Space Rocks recipe both have a visible `player_ship` gameplay contract.
- Runtime digit handling targets active gameplay trial groups before editor object-slot selection.
- The player-icon state/render contract is covered by the BGE Julia behavior/source-contract suite.
- The current implementation keeps command-driven and hosted behavior aligned through shared scene primitives and player-icon mode helpers.

Verification saved with the collapse:

- Full BGE Julia suite passed after the player, UFO, and grouped test-control changes.
- Exported runtime smoke reached gameplay from the embedded recipe and verified the active test group path.

Follow-up boundary: a more formal command-capture fixture can be opened later as a hardening lane; it is no longer blocking the player-icon visibility repair.

## Current Evidence

- User reports that after Gemini's fixes the player icon cannot be seen.
- The exported command recipe creates `player_ship` using `player-ship create --id player_ship --shape vector-ship ... --style outline`.
- The command-driven plugin path in `BasicGameEngine/src/BasicGameEngine.cpp` configures slot 0 as a visible `BgeObjectKind::Player` with `BgeObjectShape::VectorShip`, heading, render style, and dirty renderer state.
- The UI/built-in asteroid module path in `BasicGameEngine/src/AsteroidGameModule.cpp` now configures the player as `BgeObjectShape::VectorShip` and decouples heading from velocity.
- DX11 and DX12 both contain `appendVectorShipSlot`; outline vector ships route through `BgeAppendStrokedPolygon`.
- Therefore the first test question is not "are rocks, score, title, UFO, audio correct?" It is: after gameplay starts, does a player-icon state exist, and does it emit visible player-icon pixels/vertices?

## Two Surfaces To Capture

UI surface:
- Start the normal BasicGameEngine/UI Asteroids path.
- Capture user gameplay inputs that target the player icon: start/play, rotate left/right, thrust/reverse, fire, hyperspace if active.
- Assert after each captured input that the player icon still exists, remains visible, and changes heading/velocity/projectile state as expected.

Command-line / command-driven surface:
- Start from the exported recipe with the normal `--game-file` replay path for `exports/space-rocks/space-rocks.commands`.
- Capture or provide the equivalent command/input sequence against `player_ship` during gameplay.
- Assert the same player-icon contract as the UI surface: visible slot, visible render output, heading changes, thrust changes velocity, fire creates a projectile from the icon nose.

## Capture And Test Shape

The useful test artifact is a small command-capture fixture, not a screenshot-only smoke:

1. Start gameplay from UI and command-line recipe paths.
2. Capture/replay this minimal sequence: enter gameplay, idle one tick, rotate, idle one tick, thrust, idle one tick, fire.
3. Probe player-icon state after each step: active group equals main player group, player slot index valid, `visible=true`, `isDeleted=false`, `kind=Player`, `shape=VectorShip`, radius greater than zero, alpha greater than zero, heading nonzero, position inside viewport.
4. Probe rendering after each step: renderer receives the player slot, `appendVectorShipSlot` emits vertices for `VectorShip`, and at least one visible pixel/vertex lands near the expected player icon bounds.
5. Run this through the Julia BGE behavior tests in `C:\src\kicad_a\usability\bge\julia-tests\runtests.jl`, because that is the project lane for BGE behavior validation.

## Fix Probes

- Add a player-icon probe command or log event if missing, e.g. state that can expose `player_ship` slot, shape, heading, alpha, render style, and last vertex count without relying on human eyeballs.
- Make the UI capture and command-line capture produce the same normalized event list: start/play, rotate, thrust, fire, plus resulting player-icon state snapshots.
- If state is valid but icon is invisible, fix the renderer lane: `appendVectorShipSlot`, `BgeAppendStrokedPolygon`, winding/culling, alpha blending, and renderer state synchronization.
- If the UI path passes but command-line path fails, inspect recipe/plugin command parsing and `player-ship create` setup.
- If command-line path passes but UI path fails, inspect `AsteroidGameModule` start/input/selected-player behavior and its player slot visual setup.

## Explicit Non-Focus

- Do not chase rocks, score, UFO, audio, title-screen polish, or general Asteroids gameplay until the player icon command-capture loop passes.
- Do not accept heading/velocity behavior as fixed while the icon is invisible.
- Do not treat a manual visual check as completion; the lane needs replayable command capture plus state/render assertions.

## Collapse Criteria

- `recur reveal space-rocks.player-icon.ui-and-command-line-capture` reveals this lane.
- A repeatable UI command capture proves the player icon is visible and responds during gameplay.
- A repeatable command-line/recipe capture proves the same player icon contract from `exports/space-rocks/space-rocks.commands`.
- Julia BGE behavior tests cover the player icon visibility and command-response regression.
- The visible player icon survives spawn, initial idle, rotate, thrust, and fire in both surfaces.