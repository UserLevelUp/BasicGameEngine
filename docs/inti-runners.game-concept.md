# Inti Runners Game Concept

Status: concept capture (modernized 2026-06-15)
Branch: inti_runners
Source: notebook sketch and user direction, 2026-05-31 to 2026-06-01; modernized 2026-06-15

## Product Framing (2026-06-15)

Inti Runners is one **witness game** for the BasicGameEngine (BGE)
composable platform: the engine stays domain-neutral, resets to a baseline
runtime, and loads this game as plugin Operations plus a recorded command
recipe. All Inca/Inti gameplay logic lives in the game module / plugin
Operations, never in engine core. (Companion eventness in the kicad_a
workspace: `bge.witness.inca-gods-aliens.chasqui-tourist-maze.level1`,
`bge.engine.warp.reset-baseline-plugin-load-game-downloader`, and
`bge.engine.authoring-grammar.redline-ten-candidate-lock-in`.)

The product is layered:

- **Level 1 — Chasqui Photo Run**: a fast, low-friction arcade maze-chase
  on-ramp. This is the tourist hook that gets people playing in seconds.
- **Strategy depth — Inti Runners 4X**: the turn-based empire / tech /
  chasqui game below, for players who want more.

Both layers funnel toward the book *Inca Gods and Aliens*: play the free
Level 1, get hooked, go deeper, buy the book.

The originality boundary stands — the arcade layer is maze-chase *style*
with original Inca fiction, screens, names, and assets. Do not copy another
game's names, rules, UI, fiction, or assets.

## Core Frame

Inti Runners is an original Inca-inspired, rain-forest/highland strategy game about growing an empire, developing knowledge, establishing trails, and using chasqui runners to move messages between cities.

The game should be command-driven and turn-based. The player issues commands during a planning phase, then resolves the turn with `inti end-turn`. The influence is the broad 4X rhythm of plan, commit, resolve, and adapt; do not copy another game's names, rules, UI, fiction, or assets.

The game does not begin on a separate title screen. The first screen when the game runs is the main game screen. From there, the player selects one of three sections:

- Tech Tree.
- Empire.
- Chasqui Runners.

The sections are parts of the same game surface. The visual language should be Inti, rain forest, mountain trails, cities, terraces, quipu strings/knots, and runner relays, not asteroids or arcade space debris.

## Level 1 — Chasqui Photo Run (Arcade On-Ramp)

Level 1 is the fast hook: an original maze-chase reskin where you run as a
Chasqui messenger reframed as a **modern tourist with a camera**, moving
through the trail network around a central Machu Picchu hub.

- **Maze** = the trail/route network between tourist locales.
- **Dots = tourist checkpoints**: each dot is a spot to photograph. Their
  core job is *information* — visited checkpoints stay visibly checked off,
  so what remains is obvious at a glance.
- **Power node = major landmark / temple**: big points and a brief
  advantage.
- **Rivals = ghost tourists**: rival couriers race you to photograph each
  scene first; beat them to claim the shot.
- **Camera mechanic**: snap the scene to claim it; if a rival snaps first,
  you lose that claim. An optional light tussle with rivals is the fun
  factor.
- **Speed-run + leaderboard**: the headline mode is finishing the whole
  map (all spots photographed) fastest, scored onto a leaderboard.

Level 1 reuses the existing runner primitives: the ten runner modes and
the ten-position running cycle (below) are the same building blocks, gated
into a real-time arcade shell instead of the turn-based surface. Authoring
UI (selection ring, vector arrow, redline honing) is gated off in the
player-runtime shell. Can ship as a board game or a quick digital game.

## Turn Model

Each turn has two phases:

- Planning: commands select tech focus and queue empire/chasqui orders.
- Resolution: `inti end-turn` advances research, yields resources, resolves queued orders, and publishes the turn report.

Initial command slice:

- `inti main` shows the state of the turn.
- `inti tech list` shows the available tech tree.
- `inti tech research terraces|quipu|rainforest|trailworks|bridges|tambos` chooses a research focus.
- `inti empire goods` queues goods production.
- `inti empire found-city` queues founding a city.
- `inti empire path` queues establishing an Inca running trail between cities.
- `inti chasqui send` queues a message delivery after Chasqui is unlocked.
- `inti chasqui run 120,240 300,240 420,320` starts a looping runner animation at the first x,y coordinate and follows each subsequent path point.
- `inti chasqui run 120,240 300,240 420,320 speed 220 reverse-krebs` previews a boosted courier state with the 10-frame motion trail visible.
- `inti end-turn` resolves queued orders and advances the game.

The game should feel like strategic command, not twitch movement. Animated runners can exist as presentation or honing, but state changes happen through turns.

## Main Screen

The main screen is the first strong visual identity for the game.

Main elements:

- Inti, the sun, centered near the top of the scene.
- The moon off to one side as a quieter counterpoint.
- Rain forest and highland tones around the empire map.
- The current city/path state below.
- Selectable section signals for Tech Tree, Empire, and Chasqui Runners.
- A visible indication that Chasqui Runners are locked until the empire has enough cities and trails.

The screen should feel mythic and readable rather than crowded. The sketch's big triangle/ray composition remains useful: rays or mountain edges can pull the eye from Inti down into the empire and the three player choices.

## Section: Tech Tree

Purpose: choose long-term knowledge paths that change the turn economy, map, trail system, and chasqui network.

The tech tree needs to be a real system, not a single science counter. It should have prerequisites, visible progress, and practical effects on the empire.

The visual metaphor should be a quipu. Technologies live on hanging strings; research progress appears as knots forming on those strings; completed technologies read as fuller knot clusters. This gives the tech tree a culturally legible object people can relate to instead of a generic sci-fi node graph.

Current first-pass nodes:

- Terrace Farming: improves goods yield and production orders.
- Quipu Records: improves command capacity and research output.
- Rainforest Scouts: improves money/exploration yield and establishes the jungle tone.
- Stone Trailworks: improves trail building and reduces path cost.
- Rope Bridges: expands possible path links across difficult terrain.
- Relay Tambos: improves chasqui message rewards and relay efficiency.

Core verbs:

- List the tech tree.
- Choose a research focus.
- Advance research at turn resolution.
- Complete tech and apply its effect to future turns.

## Section: Empire

Purpose: grow the empire state that makes runners meaningful.

Core verbs:

- Queue goods production.
- Found or add cities.
- Establish Inca running trails between cities.
- Prepare the network that Chasqui Runners will use.

Candidate resources:

- Cities.
- Goods.
- Money or labor.
- Trail paths.
- Order capacity.

The important rule: chasqui runners are not useful until there are at least two cities. They also need established paths/trails between those cities.

## Section: Chasqui Runners

Purpose: send messages between cities once the empire has enough cities and running trails.

Unlock requirements:

- At least two cities.
- At least one established Inca running trail/path between cities.

Core verbs after unlock:

- Select a chasqui runner style.
- Queue a message delivery order.
- Animate a Chasqui runner from a start x,y coordinate through a sequence of path points.
- Resolve message delivery during turn resolution.
- Bring back rewards, information, or consequences.

The runner animation should use the engine's ten object positions as a 10-frame running cycle. At runtime the active frame loops through slots `0`-`9`, while the command supplies the route: start coordinate first, then each path point the runner follows. Normal runs should show a single clear courier; boosted runs, high speed, or reverse Krebs Cycle drug effects can reveal the fading 10-frame motion trail.

The ten runner **modes** (`inti runner|candidate|mode 0-9`) are the game's
first use of the general **redline → 10 candidates → lock-in** honing loop:
any redlined object/material/feature can surface ten variants (`0`-`9`) to
compare and lock one in. Current modes are `relay sprint`, `highland
lean`, `quipu carry`, `ridge dash`, `night courier`, `sun runner`, `canyon
step`, `moon relay`, `royal stride`, and `terrace flash`. Locking a mode in
clears its redline; the same loop should hone runner style, checkpoint
icons, landmark art, and rival behavior quickly across complex UI.

Before unlock, this section should explain the missing requirement through state, not pretend the player needs runners when there is nowhere meaningful for them to run.

## First Play Loop

1. Start at the main game screen.
2. Select a tech focus with `inti tech research ...`.
3. Queue Empire work such as goods, city founding, and path building.
4. Use `inti end-turn` to resolve the turn.
5. Repeat until the empire has at least two cities and one trail.
6. Queue a Chasqui message and resolve it on a later turn.

The earliest playable prototype should make this loop real before adding story depth.

## Prototype Slice

Minimum useful version:

- Main game screen for Inti Runners.
- Three selectable sections: Tech Tree, Empire, Chasqui Runners.
- Turn counter, order capacity, pending-order status, and turn report.
- Tech tree shown as a quipu: strings for technology lanes, knots for progress/completion, plus prerequisites and completed effects.
- Empire orders that found cities, produce goods, and establish paths at turn resolution.
- Chasqui section locked until two cities and one path exist.
- Chasqui route animation command that accepts coordinate paths and loops through ten runner frames.
- Text/status output sufficient for replay and command testing.

The visual style can start simple and symbolic: sun, moon, city nodes, green/gold rain-forest map colors, path lines, runner marker, and resource counters. Avoid asteroid-shaped city markers and arcade-space presentation unless explicitly switching back to the Asteroids game.

## Open Decisions

- Final title: Inti Runners versus another name.
- Whether tech should branch into ritual, food, logistics, diplomacy, and jungle exploration lanes.
- How much of the tech tree should be visible on the main surface versus the Tech Tree section.
- Whether runner visual honing stays in the Chasqui section only or also appears in a debug/honing mode.
- How explicitly god events appear in the first slice.
- Whether word-game/story challenges are core from the start or a second pass.
- Ordering: ship the Level 1 Chasqui Photo Run arcade hook first, the 4X
  strategy depth first, or interleave them.
- Board-game versus digital-first for the Level 1 hook.
- Leaderboard scope: local speed-run only, or shared/online.
- Whether the rival tussle is a real battle mechanic or a pure photo-race.
- How hard Level 1 funnels toward the *Inca Gods and Aliens* book.

## Originality Boundary

The user referenced an existing game influence. Treat that as inspiration for turn cadence and strategic feel, not as a source to copy. Build original screens, text, rules, assets, and characters for Inti Runners.