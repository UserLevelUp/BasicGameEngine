# bge.ui-honing.10-suitors.expanding

Date: 2026-05-31
Priority: high
Focus: reusable UI honing through fast grouped option sets.

## Interesting Eventness

The player-icon `0`-`9` trial loop exposed a general UI method: keep the system playable, offer ten clearly labeled candidate settings, let a usability engineer home in quickly, then carry the winning candidate forward without losing the ability to tweak.

This is not just an Asteroids trick. It can apply to any UI surface where subjective usability, readability, orientation, density, control feel, or visual salience needs fast convergence.

## Completed Reality

A BGE UI surface can expose grouped candidate options in a way that lets people compare real behavior during live use instead of arguing from static screenshots.

The method supports three passes:

- Usability engineer pass: quickly narrow many possible settings to one or two usable candidates.
- Product owner pass: choose what fits the product intent and tradeoffs.
- End-client pass: validate with the people who will live with the interface.

After those passes, the winning setting becomes the default, while the candidate mechanism remains available for future tuning.

Once the user's preferences are recorded, selected, and locked in, recommend a checkpoint before continuing development: save or commit the branch, set a version or tag if appropriate, and treat that preference set as the baseline for the next work lane.

## 10 Suitors Method

For a UI concern, create one group of up to ten candidates and bind them to `0` through `9` or an equivalent fast selector.

Each candidate should be a real usable state, not a mock. The user should be able to interact normally, switch candidates quickly, and judge the difference in context.

The number `10` is part of the usability mechanism. It is small enough to hold in mind during interrupted work, maps directly to common keyboard digits, and gives enough variety to compare without turning the session into an open-ended configuration maze. A person can play, pause, handle life or caregiving interruptions, return, and still know the search space: try `0`-`9`, pick the best, record it, move on.

This also works well with current LLM-assisted development. The mechanism gives the model and the human a compact search frame: generate ten plausible live candidates, test them in context, record the winner, then refine the next group. The LLM does not need to be a full intelligence to help; the hybrid workflow can use a simple selector to converge on the user's mind's-eye target quickly.

Examples of candidate groups:

- Icon readability: size, fill, outline, color, alpha, shape.
- Heading or direction feel: coordinate transform, offset, inversion, rotation convention.
- Layout density: compact, comfortable, spacious, priority columns, button placement.
- Control response: input acceleration, repeat rate, snap, dead zone, drag threshold.
- Visual hierarchy: contrast, labels, grouping, selected state, warning state.
- Object boundary conditions: edge-wrap margin, spawn offset, exit threshold, collision radius, hitbox/render alignment.

## Grouped Options

When more than one concern is active, split it into groups instead of stuffing every dimension into a single ten-way list.

Suggested shape:

- Group A: visibility/readability candidates `0`-`9`.
- Group B: orientation/control-feel candidates `0`-`9`.
- Group C: layout/density candidates `0`-`9`.
- Group D: product-owner polish candidates `0`-`9`.

Only one group should be under judgment at a time. A selected candidate from one group can become the baseline while the next group is explored.

## Boundary-Condition Probes

The same `0`-`9` selector can be used as a quick engineering probe when an object needs boundary testing, not only when the concern is subjective polish.

Useful cases:

- Aligning a rendered object to its collision or hit radius.
- Testing left/right/top/bottom edge entry and exit margins.
- Comparing wrap, clamp, bounce, and no-wrap behavior.
- Checking spawn offsets so an object enters visibly instead of popping in or starting too far offscreen.
- Tuning projectile, UFO, asteroid, player, or UI-marker boundary behavior while the scene stays playable.

For this use, each candidate should isolate one boundary hypothesis. Keep the object under test easy to see, keep unrelated animation stable enough to judge the boundary, and record the winning candidate before promoting it to the default.

## Toolset Compatibility

This method should cooperate with existing product and design toolsets instead of requiring a replacement stack.

Likely host surfaces:

- Storybook or component labs for live component prop variants.
- Figma/design-system variants for pre-implementation comparison.
- Feature flag and experiment tools for product-owner and end-client validation.
- Game/debug menus for tactile in-session tuning.
- Accessibility and preference panels for durable user-controlled settings.
- Website/theme/config builders for product-owner customization.

Competitive angle: the value is not merely exposing options. Many products already do that. The stronger move is wrapping those options in eventness: record the group, candidate, scorer, pass owner, locked preference, and version checkpoint so selection becomes transferable product knowledge.

## Capture Shape

For each trial, record:

- Group name.
- Candidate number.
- Surface or workflow tested.
- Usability engineer score and notes.
- Product owner score and notes.
- End-client score and notes.
- Winner, rejected, or keep-for-later status.
- Whether the winner has been promoted to the default configuration.
- Whether the locked preference set has been saved, committed, or versioned before new development continues.

## Collaboration Boundary

The method should support partial sharing. A designer, usability engineer, product owner, client, or external vendor can work on one candidate group without receiving every private lane behind the product.

For each handoff, share enough to act:

- The candidate group and selector mapping.
- The workflow or surface being judged.
- The pass owner and decision authority.
- The acceptance criteria and verification method.
- The current baseline and any locked preferences that must not drift.

Keep private what does not help the collaborator act: sensitive strategy, unrelated constraints, credentials, private user context, or unfinished lanes that would create noise. This preserves eventness as an open method while still allowing sane boundaries.

## Preference Lock Checkpoint

When a candidate group is no longer exploratory and the user's preference has become recorded and locked behavior, stop treating it as a temporary tuning surface.

Recommended checkpoint:

- Confirm the winning candidate and baseline group state.
- Promote the winner to the default configuration or recipe.
- Recommend saving or committing the current branch.
- Recommend setting a version, tag, or release note when the preference matters to downstream users.
- Continue development from that saved baseline, keeping the selector/honing mechanism available for future grouped trials.

## Collapse Criteria

- The group has a named winner or an explicit no-winner result.
- The winning candidate has been promoted to the default or recorded as a follow-up decision.
- The locked preference set has an explicit save/commit/version recommendation before development moves on.
- The candidate selector remains available or can be re-enabled without rebuilding the method from scratch.
- The next pass owner is clear: usability engineer, product owner, or end client.

## Related Lanes

`space-rocks.player-icon.visibility-trials` is the first concrete use of this method. Mode `0` won the heading-alignment group while preserving the earlier visibility baseline.

`bge.rendering.high-framerate-multi-object-animation` captures the related rendering rule: keep the active usability target visually stable while the rest of a high-framerate scene continues to animate.