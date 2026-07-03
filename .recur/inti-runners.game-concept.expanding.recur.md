# inti-runners.game-concept.expanding

Date: 2026-06-01
State: expanding
Priority: high
Focus: turn Inti Runners into a command-driven, turn-based strategy lane with Inca/rain-forest identity.

## Interesting Eventness

The branch has shifted from Space Rocks/asteroid honing into a new original game concept: Inti Runners, centered on Inti, empire growth, cities, Inca running trails, rain forest/highland tone, chasqui runners, and command-driven turn resolution.

The corrected completed reality is a main game screen with three selectable sections: Tech Tree, Empire, and Chasqui Runners. The stronger direction is now turn-based: the player issues commands, queues orders, picks research, then resolves the turn. The game should feel like original Inca/rain-forest strategy, not an asteroid arcade reskin.

## Concept Signals

- Main screen: Inti above the empire, moon to the side, current city/path state below.
- Turn model: plan commands, then `inti end-turn` to resolve yields, research, and orders.
- Tech Tree section: quipu strings/knots with prerequisites, progress, and effects.
- Empire section: queue goods, city founding, and Inca running trail orders.
- Chasqui Runners section: locked until two cities and one trail path exist; then queue message delivery.
- Chasqui route animation: command supplies a start x,y and path points; slots `0`-`9` loop as the running sequence.
- Visual direction: green/gold rain forest, highland trail, terrace, quipu strings/knots, and relay markers; avoid asteroid-shaped city markers.

## Completed Reality

The branch should eventually open into a playable Inti Runners prototype with:

- A visible main game screen with game identity.
- Three selectable sections on that main screen.
- Turn counter, order capacity, pending orders, and turn reports.
- Empire state anchored by cities and paths.
- Chasqui runner play gated by `cities >= 2` and `paths >= 1`.
- A serious tech tree shown as a quipu: hanging strings for technology lanes and knots that form as technologies progress/complete.
- A first playable message-delivery loop that changes empire state.
- Original assets, text, rules, and screen design.
- Command/replay affordances that fit BGE's existing testing workflow.

## First Implementation Slice

- `IntiGameModule` is a peer of `AsteroidGameModule`, not an asteroid rewrite.
- `game inti` / `inti main` activates the Inti main screen; Asteroids remains switchable through its own commands.
- `inti tech list` shows tech node state.
- `inti tech research terraces|quipu|rainforest|trailworks|bridges|tambos` selects research focus.
- `inti empire goods`, `inti empire found-city`, and `inti empire path` queue turn orders.
- `inti chasqui send` queues a message only after city/path requirements are met.
- `inti chasqui run 120,240 300,240 420,320` previews a looping 10-frame Chasqui runner along coordinate path points.
- `inti chasqui run ... speed 220 reverse-krebs` previews a boosted route where the visible 10-frame trail becomes a speed/drug effect instead of the normal running baseline.
- `inti end-turn` resolves resources, research progress, completed tech, and queued orders.
- Main-map city markers use non-asteroid shapes and greener/golder colors.

## 2026-05-31 Module Boundary Slice

Implemented first playable module boundary for Inti Runners:

- Active-game routing gates key input and tick updates so Inti and Asteroids do not mutate the same object slots at the same time.
- Shared scene primitives include a `runner` object shape rendered by both DirectX 11 and DirectX 12.

This slice was later corrected: the initial surface is the main game screen, not a title screen, and the game is now steering toward turn-based command strategy.

## 2026-06-01 Turn-Based Strategy Correction

User clarified the next design pressure:

- Keep the command-driven approach.
- Make the game turn-based with a strategy planning/resolve cadence.
- Preserve the path-between-cities idea.
- Push the identity toward Inca/rain forest, not asteroid.
- Give the Tech Tree serious work.

Implementation update:

- Added turn counter, order capacity, pending-order state, and `inti end-turn` resolution.
- Empire and Chasqui commands now queue orders instead of acting like real-time arcade inputs.
- Added six tech nodes with prerequisites/progress/effects: terraces, quipu, rainforest, trailworks, bridges, tambos.
- Replaced the generic Tech Tree node graph with a quipu visualization: a main cord, hanging technology strings, and knots that form from research progress.
- Removed asteroid-shaped city markers from the Inti map presentation.

## 2026-06-01 Quipu Tech Tree Pivot

User clarified the desired tech-tree metaphor:

- The tech tree should look like a quipu, or at least evoke quipu strings and knots.
- Increasing technology state should be visible as knots forming on quipu strings.
- The visual should be something people can potentially relate to, not an abstract sci-fi graph.

Implementation update:

- Added a shared `quipu` scene shape/kind and renderer support in both DirectX paths.
- The Inti Tech Tree now renders a main cord plus six hanging technology cords.
- Each technology cord uses knot count to show research progress and fuller knots to show completed technologies.

## 2026-06-01 Chasqui Route Animation Slice

User clarified the Chasqui animation shape:

- It needs an animated Chasqui runner that loops through a running sequence.
- It should use all ten object positions as the animation sequence.
- The command should include the x,y coordinate where the runner starts.
- The command should support a sequence of path points to run.

Implementation update:

- `inti chasqui run <x,y> <x,y> ...` parses up to eight route points.
- The Chasqui scene stores the route and samples it by distance each tick.
- Slots `0`-`9` are configured as runner animation frames with phase offsets and a fading motion trail.
- Runner glyph rendering now honors explicit heading and frame phase so the route animator controls the run cycle.
- The fading multi-runner trail is now treated as a boost/readability effect for high-speed or reverse Krebs Cycle drug states; normal Chasqui route previews keep the courier visually clean.

## Verification Ideas

- `inti main` shows turn, orders, cities, trails, resources, and current tech.
- `inti tech list` prints node progress/locked/done states.
- `inti tech research quipu` selects Quipu Records when available.
- `inti tech research tambos` reports locked until prerequisites are complete.
- `inti empire found-city` queues a city order; city count changes only after `inti end-turn`.
- `inti empire path` queues trail work only when city requirements are met or planned.
- `inti chasqui send` is blocked until two cities and one path exist.
- `inti end-turn` resolves queued orders and advances research.

## Boundary

The referenced influence is directional only. Do not copy names, text, art, map layouts, rules, or assets from another game. Build Inti Runners as original work using the user's sketch and BGE's own mechanics.

## Linked Capture

See docs/inti-runners.game-concept.md for the human-readable concept capture.