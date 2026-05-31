# space-rocks.ufo.random-saucer.collapsing

Date: 2026-05-31
State: collapsing
Priority: high
Focus: original-Asteroids-style UFO randomly showing up during Space Rocks gameplay.

## Interesting Eventness

The exported Space Rocks recipe already promised `bge.piece.ufo`, `enemy-shot`, and a `200_SAUCER` title legend. This lane makes that promise visible in play: a saucer randomly enters from a screen edge, moves across the high-framerate scene, can be shot for points, and can threaten the player by contact.

## Completed Reality

The UFO is animated as a live moving object, not as a static decoration. It uses normal per-frame slot motion and a dedicated saucer shape in both DirectX renderers.

Current implemented behavior:

- Shared `BgeObjectShape::Ufo` and `BgeObjectKind::Ufo` exist.
- DirectX 11 and DirectX 12 render a saucer outline/filled shape.
- UFO slots are exempt from edge wrap so they can fly offscreen and retire.
- Command-composed Space Rocks supports `ufo create` / `ufo show`.
- Export recipe enables `ufo create --id saucer --score score:200 --weapon enemy-shot --arrival 7..18 --aim player_ship --edge-spawn --style outline --view 0`.
- UFO view `0` is forced on-screen immediately after pressing Enter so the player can inspect the saucer right when the game starts; later random arrivals use the configured 7..18 second window.
- Page Up/Page Down changes the active `0`-`9` test group between ship and UFO.
- The status/status-bar text shows the active test group, such as `Test group: ufo` or `Test group: ship`.
- When UFO is the active group, `0`-`9` applies ten UFO view candidates on-screen.
- Hosted UI Asteroid module also randomly spawns a moving UFO.
- Player bullets can destroy the UFO and award 200 points.
- UFO contact can cost the player a life.

## Collapse Record

Collapsed on 2026-05-31.

Saved outcome:

- The saucer is no longer just promised by the recipe; it appears in normal play.
- UFO view `0` is forced on-screen immediately after starting gameplay so the user can inspect it without waiting for the random timer.
- `0`-`9` applies ten UFO view candidates while the active test group is `ufo`.
- Page Up/Page Down switches between UFO and ship test groups.
- Status/HUD text shows the active group so digit keys are unambiguous.
- The exported recipe is refreshed with `--view 0`.

Verification saved with the collapse:

- Build/export refresh passed.
- Full BGE Julia suite passed.
- Exported runtime smoke logged `ufo-view-applied mode=0`, `ufo-view-applied mode=8`, and Page Down focus on `ship`.

Follow-up boundary: enemy firing, audio, aim jitter, and small/large saucer variants are separate arcade-fidelity lanes, not blockers for this UFO visibility/motion collapse.

## Animation Shape

The first UFO animation pass is motion animation:

- Random arrival timer.
- Random left/right edge entry.
- Horizontal crossing velocity.
- Slight vertical drift.
- Offscreen exit instead of wrap-around.

This is enough to feel like the original saucer without requiring sprite-frame animation. A later pass can add audio, enemy shots, aim jitter, or different small/large saucer behavior.

## Command Surface

Current command shape:

`ufo create --id saucer --score score:200 --weapon enemy-shot --arrival 7..18 --aim player_ship --edge-spawn --style outline --view 0`

Quick test command:

`ufo show --id saucer --score score:200 --weapon enemy-shot --arrival 7..18 --aim player_ship --edge-spawn --style outline --view 0`

## UFO View Candidates

The `0`-`9` view candidates are intended for fast visual/boundary comparison:

- `0`: left entry high.
- `1`: right entry high.
- `2`: top center hold.
- `3`: left middle drift.
- `4`: right middle drift.
- `5`: lower center hold.
- `6`: left edge clip.
- `7`: right edge clip.
- `8`: large filled.
- `9`: small bright.

## Collapse Criteria

- UFO appears randomly during normal play without user command intervention.
- UFO is visibly animated as it crosses the field.
- UFO exits instead of wrapping forever.
- Player shots can destroy it and add 200 points.
- UFO contact is a player hazard.
- UI Asteroid module and exported command-driven Space Rocks both support the behavior.
- Full BGE Julia test suite passes after the implementation.

## Runtime Verification

Verified the exported `space-rocks.exe` path by launching the embedded recipe, sending Enter to start play, and reading `BasicGameEngine.log`.

Observed log evidence:

- `[BgePlayerRuntime] embedded-script status="Player runtime loaded 16 commands from embedded resource"`
- `[SpaceRocks] StartOrRestartSpaceRocksGameLocked count=1`
- `[SpaceRocks] SpawnSpaceRocksWaveLocked wave=1 call#1`
- `[BgeUfoPlugin] spawned`
- `[BgeTrialGroup] ufo-view-applied mode=0 slot=6`
- `[BgeTrialGroup] ufo-view-applied mode=8 slot=6`
- `[BgeTrialGroup] focus="Test group: ship | PageUp/PageDown switch | 0-9 ship view 0 heading current"`

This confirms the UFO does actually spawn in the normal exported runtime, not only through source inspection.

The newer `ufo-view-applied mode=0` verification confirms the UFO is visible immediately at game start. The `mode=8` and `Test group: ship` lines confirm the digit candidate and Page Down group-focus controls are active.

## Follow-Up Eventness

Enemy firing is intentionally left as the next arcade-fidelity pass: use the existing `enemy-shot` projectile definition and `--weapon enemy-shot` config to let the saucer fire with imperfect aim at the player.

## Related Lanes

- `bge.rendering.high-framerate-multi-object-animation`: UFO is another live animated object in the high-framerate scene.
- `bge.ui-honing.10-suitors`: UFO timing, size, color, speed, and aim could each be tuned with grouped candidates if needed.