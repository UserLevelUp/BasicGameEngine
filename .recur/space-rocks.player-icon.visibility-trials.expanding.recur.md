# space-rocks.player-icon.visibility-trials.expanding

Date: 2026-05-31
Priority: critical
Focus: whether the user can see the Asteroids player icon and whether its rendered nose, thrust, and bullets agree during play.

## Interesting Eventness

The player icon is the first human witness for the game. If Marc cannot see it during play, the player system is not fixed, even if the slot state, heading math, or command parser looks technically correct.

This lane exists so we can quickly try 10 player-icon settings by pressing `0` through `9` during play and compare results without turning the work into general Asteroids debugging.

Implementation status: the UI asteroid module and exported `player-ship` gameplay path now route active gameplay digit keys to player-icon trial modes before falling back to editor object-slot selection. Current user feedback: mode `1` was the best visible candidate for raw visibility, so all heading trials keep that filled-white-small look. The invulnerability alpha path stays visually steady for these modes. Heading feedback: visual heading and bullet direction can disagree, especially near 0 degrees / -90 degrees / about -10 degrees, so `0`-`9` test heading transforms instead of color/size. Latest result: mode `0` works best, meaning the current screen-coordinate heading transform is the winning candidate.

Winning candidate: `0` heading current. Because `0` is already the default player-icon mode, no gameplay default change is needed unless a later replay or UI/command-line check contradicts this result.

## Completed Reality

The user starts gameplay and immediately sees the player icon. It remains visible during idle, rotate, thrust, fire, respawn/invulnerability, and normal command replay. The rendered nose, thrust direction, and bullet direction agree. The chosen settings work in both surfaces:

- UI Asteroids path.
- Command-line / exported recipe path using `exports/space-rocks/space-rocks.commands` and the normal `--game-file` replay route.

## Fast Trial Loop

One trial should be cheap enough to run in minutes:

1. Start gameplay in the UI path or exported command-line recipe path.
2. Press one key from `0` through `9` to apply a player-icon heading trial mode.
3. Check: can the user see the player icon immediately?
4. Run the minimal command/input sequence: idle, rotate left/right, thrust, fire.
5. Capture whether the icon nose, movement/thrust, and bullet direction agree for both UI and command-line surfaces.
6. Score it, keep or discard, then move to the next profile.

## Trial Keys 0-9

- `0` heading current: current screen-coordinate heading transform. Current winning candidate.
- `1` heading invert y: flips Y after the logical heading calculation. Try this first for the "-90 looks up/down wrong" symptom.
- `2` heading plus 90: current heading plus 90 degrees.
- `3` heading minus 90: current heading minus 90 degrees.
- `4` heading 180: current heading plus 180 degrees.
- `5` invert y plus 90: Y-inverted heading plus 90 degrees.
- `6` invert y minus 90: Y-inverted heading minus 90 degrees.
- `7` invert y 180: Y-inverted heading plus 180 degrees.
- `8` heading fine minus 10: current heading minus 10 degrees.
- `9` heading fine plus 10: current heading plus 10 degrees.

All ten modes use the readable filled-white-small player icon: `VectorShip`, `Filled`, radius 15, white, steady alpha.

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
- Logical heading is nonzero and changes on rotate.
- Rendered/gameplay heading updates from the selected `0`-`9` heading transform.
- Velocity changes on thrust along the same transformed heading used by the visible nose.
- Fire creates a projectile from the same transformed heading used by the visible nose.

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
- Preferred collapse target is `3 good` at spawn and during rotate/thrust/fire, with nose/thrust/bullets all agreeing.
- Pressing `0` through `9` during play applies the matching heading trial profile without selecting editor object slots.
- Selected visibility mode should not blink during invulnerability unless the user explicitly asks for blinking back.
- The winning setting is recorded in the recipe/UI configuration path, not only remembered in a screenshot.
- Current winning setting recorded here: `0` heading current, already the default runtime mode.
- A replayable behavior test or visual probe prevents regressing to an invisible player icon.
- `recur reveal space-rocks.player-icon.visibility-trials` reveals this lane.
