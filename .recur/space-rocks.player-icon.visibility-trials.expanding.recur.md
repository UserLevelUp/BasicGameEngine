# space-rocks.player-icon.visibility-trials.expanding

Date: 2026-05-31
Priority: critical
Focus: whether the user can actually see the Asteroids player icon while playing the game.

## Interesting Eventness

The player icon is the first human witness for the game. If Marc cannot see it during play, the player system is not fixed, even if the slot state, heading math, or command parser looks technically correct.

This lane exists so we can quickly try 10 visibility settings by pressing `0` through `9` during play and compare results without turning the work into general Asteroids debugging.

Implementation status: the UI asteroid module and exported `player-ship` gameplay path now route active gameplay digit keys to player-icon visibility modes before falling back to editor object-slot selection.

## Completed Reality

The user starts gameplay and immediately sees the player icon. It remains visible during idle, rotate, thrust, fire, respawn/invulnerability, and normal command replay. The chosen settings work in both surfaces:

- UI Asteroids path.
- Command-line / exported recipe path using `exports/space-rocks/space-rocks.commands` and the normal `--game-file` replay route.

## Fast Trial Loop

One trial should be cheap enough to run in minutes:

1. Start gameplay in the UI path or exported command-line recipe path.
2. Press one key from `0` through `9` to apply a player-icon visibility mode.
3. Check: can the user see the player icon immediately?
4. Run the minimal command/input sequence: idle, rotate, thrust, fire.
5. Capture state and visual result for both UI and command-line surfaces.
6. Score it, keep or discard, then move to the next profile.

## Trial Keys 0-9

- `0` baseline outline: `VectorShip`, `Outline`, radius 14, outline thickness 2, white.
- `1` larger outline: `VectorShip`, `Outline`, radius 18, outline thickness 2, white.
- `2` big outline: `VectorShip`, `Outline`, radius 22, outline thickness 2, white.
- `3` thick outline: `VectorShip`, `Outline`, radius 18, outline thickness 4, white.
- `4` max outline: `VectorShip`, `Outline`, radius 20, outline thickness 6, white.
- `5` filled white: `VectorShip`, `Filled`, radius 18, white. Use this to separate outline-stroke failure from ship-state failure.
- `6` cyan outline: `VectorShip`, `Outline`, radius 18, outline thickness 4, cyan/blue-white.
- `7` yellow outline: `VectorShip`, `Outline`, radius 18, outline thickness 4, yellow-white.
- `8` huge cyan outline: `VectorShip`, `Outline`, radius 26, outline thickness 6, cyan/blue-white.
- `9` debug magenta marker: `Ball`, `Filled`, radius 26, magenta. This is not final game art; it answers whether the slot is present but too subtle or the vector path is failing.

## Score Each Trial

Use the same score for UI and command-line separately:

- `0 invisible`: user cannot find the player icon while playing.
- `1 barely visible`: visible only after hunting or only in a still frame.
- `2 usable`: visible during idle and movement, but could be clearer.
- `3 good`: obvious at spawn and readable during rotate, thrust, and fire.

Record:

- Profile number.
- Surface: UI or command-line.
- Spawn visible score.
- Rotate/thrust/fire visible score.
- Invulnerable/respawn visible score.
- Notes: too small, too thin, wrong color, hidden by title/state, renderer missing, or command did not reach player.

## State Checks During Each Trial

Every trial should still prove the player-icon state, because good visuals with broken state are a trap:

- Player slot exists and is the main player slot.
- Active object group equals main player group.
- `visible=true`.
- `isDeleted=false`.
- `kind=Player`.
- `shape=VectorShip` unless intentionally testing a fallback.
- `radius > 0`.
- `colorA > 0` even during invulnerability.
- Heading is nonzero and changes on rotate.
- Velocity changes on thrust.
- Fire creates a projectile from the player icon direction.

## Render Checks During Each Trial

If a trial scores `0 invisible` while state checks pass, inspect rendering before changing gameplay logic:

- Does `appendVectorShipSlot` run for the player slot?
- Does `BgeAppendStrokedPolygon` emit vertices for outline mode?
- Does filled mode emit vertices for profile 6?
- Does the renderer receive updated slot state after gameplay starts?
- Does alpha blending or culling erase the icon?
- Does a screenshot or pixel/vertex probe show any bright pixels near expected player bounds?

## UI Surface

The UI path must allow quick setting swaps for player icon visibility. If those settings are not currently configurable, add the smallest probe/control needed to try radius, outline thickness, style, color, alpha, and debug halo without rebuilding the whole game design each time.

The UI result matters because Marc is judging playability there: if the icon cannot be seen by a person during live play, the lane stays expanded.

## Command-Line Surface

The command-line recipe path must support equivalent trials through recipe settings or replay setup. The preferred recipe knobs are:

- `--style outline|filled`
- `--outline-thickness N`
- radius or size if exposed
- color/alpha if exposed
- temporary debug halo if needed

If a knob is not exposed, that absence becomes part of the fix: command-driven Asteroids needs enough player-icon settings to test visibility quickly.

## Related Lane

Use `space-rocks.player-icon.ui-and-command-line-capture` for command capture and replay correctness. Use this lane for human visibility and fast visual trial selection.

## Collapse Criteria

- A profile scores at least `2 usable` on both UI and command-line surfaces.
- Preferred collapse target is `3 good` at spawn and during rotate/thrust/fire.
- Pressing `0` through `9` during play applies the matching visibility profile without selecting editor object slots.
- The winning setting is recorded in the recipe/UI configuration path, not only remembered in a screenshot.
- A replayable behavior test or visual probe prevents regressing to an invisible player icon.
- `recur reveal space-rocks.player-icon.visibility-trials` reveals this lane.
