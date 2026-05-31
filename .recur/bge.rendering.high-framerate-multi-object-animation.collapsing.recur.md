# bge.rendering.high-framerate-multi-object-animation.collapsing

Date: 2026-05-31
State: collapsing
Priority: high
Focus: keep multi-object animation readable at high framerate while preserving stable usability targets.

## Interesting Eventness

The player-icon blink fix exposed a broader rendering technique: in a high-framerate view with many animated objects, not every animation channel should be treated equally.

Asteroids, bullets, particles, score changes, invulnerability timers, and environmental motion can all animate across many frames. But when a specific object is being judged for usability, orientation, or readability, its visual identity should stay stable enough for the user to compare candidates.

## Completed Reality

The renderer can animate many objects across frames while the object under active usability judgment remains readable. Animation remains available for game feel and feedback, but the target object does not disappear, pulse away, or shift presentation rules while the user is trying to judge it.

The concrete example is the Asteroids player icon: the ship can still have invulnerability gameplay state, and the rest of the scene can continue updating at high framerate, but player-icon trial modes use steady alpha so Marc can judge shape, heading, thrust, and bullet alignment.

## Collapse Record

Collapsed on 2026-05-31.

Saved outcome:

- Active usability targets stay visually stable while the rest of the scene can keep animating.
- Player-icon trial modes use steady presentation during judgment instead of invulnerability blink noise.
- The UFO can animate as a live moving object without disturbing the currently focused trial group.
- Shared helpers keep the command-driven and hosted paths aligned.

Verification saved with the collapse:

- Full BGE Julia suite passed after the stable-target and grouped-object changes.
- Runtime smoke verified live group switching while the game was running.

Follow-up boundary: reopen only when a new animation channel interferes with human judgment, or when a target-specific animation is itself the candidate being tested.

## Technique

Separate these concerns:

- Gameplay state: invulnerable, alive, respawning, moving, firing, colliding.
- Animation phase: blink phase, pulse phase, sprite frame, particle lifetime, rotation, scale, opacity curve.
- Usability target state: the object or property currently being judged by a human.
- Presentation baseline: the stable visual form used for comparison.

The mistake to avoid is letting an animation phase override the usability target. A blink can be useful feedback, but it becomes noise when the user is trying to decide whether a player icon is visible, pointing correctly, or firing in the right direction.

## High-Framerate Rules

- Animate non-target objects freely when it supports game feel or feedback.
- Keep the active target object visually stable during a honing pass unless the animation itself is the candidate being tested.
- Let gameplay state continue internally even when presentation is stabilized.
- Apply stable presentation through a shared helper so UI and command-driven paths behave the same way.
- Keep per-frame updates deterministic enough that a user can compare candidate `0` with candidate `1` without unrelated flicker changing the judgment.

## Multi-Object View Shape

For each object in the view, track whether it is:

- Background context: can animate normally.
- Gameplay feedback: can animate according to state.
- Active usability target: should use the current stable comparison baseline.
- Candidate under test: may vary only the candidate property being judged.

This lets a scene remain alive at high framerate without stealing attention from the thing being honed.

## Capture Checks

For high-framerate multi-object testing, record:

- Target object and candidate group.
- Which properties are allowed to vary.
- Which properties must remain stable.
- Whether non-target animations continue normally.
- Whether UI and command-line paths share the same presentation helper.
- Whether the user can judge the target after several seconds of live animation.

## Collapse Criteria

- The target object remains visible/readable during live high-framerate animation.
- Non-target object animation still works and does not mask the target.
- The candidate selector varies only the intended property group.
- The stable presentation rule is shared across UI and command-driven paths.
- The winning candidate can be recorded, locked, and versioned without removing the animation system.

## Related Lanes

- `space-rocks.player-icon.visibility-trials`: concrete proof; steady player alpha stopped blinking while the rest of the game kept animating.
- `bge.ui-honing.10-suitors`: grouped candidate selection method that benefits from stable target presentation.