# space-rocks.ufo.random-saucer.expanding

Date: 2026-05-31
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
- Export recipe enables `ufo create --id saucer --score score:200 --weapon enemy-shot --arrival 7..18 --aim player_ship --edge-spawn --style outline`.
- First saucer after pressing Enter is accelerated to a 3..8 second window so the player can actually notice the feature quickly; later arrivals use the configured 7..18 second window.
- Hosted UI Asteroid module also randomly spawns a moving UFO.
- Player bullets can destroy the UFO and award 200 points.
- UFO contact can cost the player a life.

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

`ufo create --id saucer --score score:200 --weapon enemy-shot --arrival 7..18 --aim player_ship --edge-spawn --style outline`

Quick test command:

`ufo show --id saucer --score score:200 --weapon enemy-shot --arrival 7..18 --aim player_ship --edge-spawn --style outline`

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

This confirms the UFO does actually spawn in the normal exported runtime, not only through source inspection.

## Follow-Up Eventness

Enemy firing is intentionally left as the next arcade-fidelity pass: use the existing `enemy-shot` projectile definition and `--weapon enemy-shot` config to let the saucer fire with imperfect aim at the player.

## Related Lanes

- `bge.rendering.high-framerate-multi-object-animation`: UFO is another live animated object in the high-framerate scene.
- `bge.ui-honing.10-suitors`: UFO timing, size, color, speed, and aim could each be tuned with grouped candidates if needed.