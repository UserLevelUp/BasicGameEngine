# BGE Save, Project, And Export Roadmap

Status: research and architecture plan
Scope: generic BasicGameEngine capability, no domain-specific plugin content

## Why This Matters

A game engine becomes useful when a user can close it, reopen it, keep working,
and eventually ship what they made. For BGE, that means save/load should not be
an afterthought bolted onto the renderer. It should become the stable contract
between the controller, workers, editor UI, command history, and future exported
games.

BGE already has the right early seams:

- runtime scene state in object slots, renderer selection, background path, edit
  mode, edit rate, animation state, and sound slot state;
- command routing through controller command text and worker `WM_COPYDATA`;
- controller History that records command equivalents;
- OpNode attributes and placeholder serialization concepts;
- CLI arguments that can reconstruct parts of a running scene.

The next step is to promote those pieces into an explicit project/scene model.

## Ten-Item Working Set Rule

BGE should keep editor complexity bounded by making every active editing surface
care about at most 10 things at a time.

```text
10 object slots per active object group
10 actor slots per active actor group
10 animation items per active sequence group
10 sound slots per active sound group
10 visible controls before the user switches groups
```

Large projects can still exist. They scale by adding named groups and switching
the active group, not by showing 50 or 500 editable things in one panel. This is
the simplicity rule: the user is never asked to reason about more than 10 active
items in the current mode.

The save format should preserve many groups, but the regular editing UI should
always expose one bounded working set. Export sees the whole project; editing
sees the active 10. The final game scene, animation, or movie export may include
many groups of 10 and can therefore be internally complex. That complexity is
acceptable at the composition/export layer. The editor stays humane by making
the user work on one named 10-item group at a time.

Full-animation preview is the deliberate exception to the active-group editing
rule. A preview window may render multiple selected groups together so the user
can see the assembled result without making the regular editing surface more
complex.

Ghost and ping references are the editing exception. While editing one 10-item
group, the user may request read-only ghosts of one or more other groups. Those
ghosts help align timing, placement, direction, spacing, or collisions with
items outside the active group. They do not become editable items and do not
count against the active 10.

## Save Scopes

BGE should support several save scopes rather than one overloaded save file.

```text
User settings      engine/editor preferences for this machine
Project file       game identity, scenes, assets, export profiles, startup scene
Scene file         objects, layers, renderer state, camera/view, commands metadata
Runtime save       player progress/state produced by an exported game
Command journal    optional replay/debug/undo stream, not the only source of truth
Build manifest     exact files and settings used for an exported executable
```

Important rule: snapshot files are authoritative. Command journals are useful for
history, replay, debugging, and smoke tests, but loading a game project should
not require replaying every command from the beginning.

## File Types

Suggested extensions:

```text
*.bgeproj        project manifest
*.bgescene       scene snapshot
*.bgeanim        authored animation clip or timeline
*.bgesave        runtime/player save
*.bgejournal     command/history journal
*.bgebuild       export/build profile and manifest
```

Movie files are export artifacts, not editable source files:

```text
*.png sequence    deterministic frame output for testing and later encoding
*.mp4             packaged movie export for sharing or publishing
```

The first implementation can store JSON. JSON is easy to inspect, test, diff,
and generate from smoke scripts. Later, BGE can add a compact binary asset cache
without changing the editable project format.

## Project Model

A project should describe the game as a product, not just the currently open
window.

```json
{
  "schema": "bge.project.v1",
  "engineVersion": "0.1.0",
  "game": {
    "id": "sample.game",
    "title": "Sample Game",
    "company": "",
    "startupScene": "scenes/main.bgescene"
  },
  "assets": [
    {
      "id": "asset.background.main",
      "kind": "image",
      "path": "assets/background.png",
      "hash": ""
    }
  ],
  "scenes": [
    {
      "id": "scene.main",
      "path": "scenes/main.bgescene"
    }
  ],
  "exportProfiles": [
    {
      "id": "windows.debug",
      "target": "windows-x64",
      "configuration": "Debug"
    }
  ]
}
```

## Scene Model

The current bouncing-ball state can map directly into a first scene schema.
This lets save/load ship before BGE has a full entity/component system.

```json
{
  "schema": "bge.scene.v1",
  "renderer": "dx11",
  "animationRunning": false,
  "sceneTimeSeconds": 0.0,
  "selectedObject": 1,
  "editMode": "translate",
  "editRate": "1x",
  "background": {
    "asset": "asset.background.main",
    "path": "assets/background.png"
  },
  "objects": [
    {
      "slot": 1,
      "visible": true,
      "x": 180.0,
      "y": 180.0,
      "velocityX": 180.0,
      "velocityY": 135.0,
      "radius": 32.0,
      "color": [0.96, 0.34, 0.22],
      "motion": {
        "direction": [0.8, 0.6],
        "magnitude": 225.0,
        "velocity": [180.0, 135.0],
        "algorithm": "movement.bounce2d"
      },
      "actor": {
        "id": "actor.object.1",
        "kind": "prop",
        "controller": "movement.bounce2d"
      }
    }
  ],
  "sound": {
    "selectedSlot": 1
  }
}
```

## Motion Vectors, Actors, And Movement Algorithms

The current arrow should become more than a visual edit handle. It is the visible
representation of an object's initial motion vector:

```text
direction  normalized heading, such as [0.8, 0.6]
magnitude  speed or force scalar
velocity   derived direction * magnitude cache for simple runtimes
```

That model lets the same arrow grow into actor movement without changing the
editor gesture. A selected object can start as a simple bouncing ball, then later
become a player actor, NPC actor, projectile, camera target, or simulation agent.

Movement behavior should be attached as a generic algorithm/controller:

```text
movement.static        no automatic movement
movement.bounce2d      current bouncing-ball behavior
movement.input.wasd    player-controlled actor using input intents
movement.seek          actor seeks a target point or actor
movement.patrol        actor follows named waypoints
movement.orbit         actor orbits a center point
movement.scripted      actor delegates movement to a script/plugin
```

The save file should preserve both the current physical state and the movement
intent. Position and velocity answer "where is it now?" The arrow direction,
magnitude, actor id, and movement algorithm answer "how should it move next?"

This keeps BGE's current vector-arrow UI useful for future game logic. The same
control can edit a prop's velocity today, a player actor's initial facing/speed
tomorrow, and an NPC's movement controller later.

## Animation Sequencing Window

BGE should have a dedicated animation sequencing window even though the actual
animation plays in the renderer window. The renderer shows the result; the
sequencing window controls what is selected, ordered, timed, grouped, saved, and
exported.

The first sequencing window should stay intentionally small:

```text
Group dropdown      select the active animation/object/actor group by name
+ button            create a new named group
10 item slots       edit only 10 items in the active group at one time
Timeline controls   play, pause, stop, scrub, frame step, loop
Item controls       select item, enable/disable, set actor, set movement algorithm
Vector controls     edit direction, magnitude, rate, and selected WASD action mode
Save/export         save scene, save animation clip, export frames/movie
```

Each group must have a unique project-local name. The UI should prevent duplicate
names or resolve them predictably with suffixes such as `Group 2`, `Group 3`, and
so on. The group dropdown is the active working set selector; the plus button is
the simple creation path.

The 10-item limit is deliberate. It matches the current numbered-slot mental
model and keeps the editor simple. Larger scenes can have many groups, but the
user works with one 10-item group at a time. Nothing in the editor should become
overly complicated because the active problem is always capped at 10 items.

Final playback is allowed to combine every group. Sequencing, saving, and export
can assemble the full messy result; editing remains local to the active group.

Example sequencing state:

```json
{
  "schema": "bge.sequence.v1",
  "activeGroup": "group.player-and-npcs",
  "groups": [
    {
      "id": "group.player-and-npcs",
      "name": "Player And NPCs",
      "maxItems": 10,
      "items": [
        {
          "slot": 1,
          "actor": "actor.player",
          "animation": "animations/player-idle.bgeanim",
          "movement": "movement.input.wasd",
          "enabled": true
        },
        {
          "slot": 2,
          "actor": "actor.guard.1",
          "animation": "animations/guard-patrol.bgeanim",
          "movement": "movement.patrol",
          "enabled": true
        }
      ]
    }
  ]
}
```

The sequencing window should use the same input contract as the renderer:

```text
Up / Down      choose mode, item, actor, or movement algorithm context
Left / Right   choose rate, magnitude, frame step, or timeline sensitivity
WASD / ASDW    apply the selected action to the selected item or actor
Escape         stop playback first, then close/recover context
```

The Mapping window should include the sequencing window, and Controller History
should record command equivalents for every sequencing action.

## Preview Window Group Selection

The preview window is for seeing the full assembled animation. It can include one
group, several selected groups, or every group in the project.

The regular renderer/editor window keeps the active 10-item group. The preview
window composes selected groups:

```text
Regular editing window   active group only, max 10 items
Preview window           selected groups, many groups of 10 allowed
Export/movie render      selected export groups or all export-enabled groups
```

The preview controls should include:

```text
group checklist       include/exclude groups from preview
select all / none     fast preview set changes
preview profile       current group, selected groups, all groups, export set
playback controls     play, pause, stop, scrub, frame step, loop
fit/zoom controls     view the assembled result without editing every item
```

Preview selection should not change the active editing group. If the user wants
to edit an item seen in preview, they switch the regular editor to that item's
group. This keeps preview composition powerful while preserving the 10-item
editing rule.

Suggested commands:

```text
preview group add <name>
preview group remove <name>
preview group only <name>
preview groups all
preview groups none
preview play|pause|stop
```

## Ghost And Ping Reference Groups

Ghost groups are read-only overlays from other groups shown while editing the
active 10-item group. Pings are temporary highlights of a ghost group, item, or
path so the user can align the current group against it.

```text
Active group      editable, max 10 items
Ghost groups      read-only context, do not count against the active 10
Ping highlight    temporary flash/focus marker for a ghost group or item
Preview groups    full composed playback, not direct editing
```

Ghosts should support simple visual states:

```text
faint ghost        visible reference, no selection handles
pinged ghost       sonar-like outline pulse that fades fully to nothing
path ghost         read-only trail or future/past motion path
collision ghost    optional warning when active items overlap ghost items
```

Ping should start with a hard black/white outline around the selected ghost
items, then fall through a subtle tonal glow or ring before fading to zero. It
should only re-ping according to the ping rate for the ghost groups the user has
chosen.

The editor rule is strict: WASD/ASDW and arrow actions apply only to the active
group unless the user explicitly switches active groups. Ghosts can be pinged,
hidden, shown, or used for snapping/alignment hints, but not mutated.

Suggested commands:

```text
ghost group add <name>
ghost group remove <name>
ghost groups clear
ghost opacity <0-100>
ghost ping-rate <seconds>
ping group <name>
ping item <group> <slot>
ping path <group>
ping selected-ghosts
ping stop
```

This gives the user context from the larger messy animation while preserving the
small working surface. The active task remains 10 editable items; ghost/ping is
just alignment help.

See `docs/bge.animation-assist-techniques.md` for the deeper analysis of onion
skins, ghost groups, pings, motion paths, markers, reset states, and preview
profiles.

## Input Usability Gates For Motion Editing

The motion-vector UI should go through the same kind of usability testing as the
renderer edit controls. Keyboard behavior needs to be sufficient for working
with both simple objects and future actor/controller logic.

The shared input grammar should remain:

```text
Up / Down      choose mode, actor, or movement algorithm context
Left / Right   choose rate, speed, magnitude, or sensitivity
WASD / ASDW    perform the selected movement-vector or actor action
Escape         stop active playback first, then close/recover context
```

Usability tests should prove:

- Arrow keys change setup state only: mode, selected movement algorithm, rate,
  magnitude, or sensitivity.
- Arrow keys do not accidentally move, retarget, or rewrite an actor before an
  explicit WASD action.
- WASD actions visibly update the selected vector, actor, target, or controller
  behavior according to the current mode.
- Status text names the selected object/actor, movement algorithm, direction,
  magnitude, and rate after each setup or action step.
- The Mapping window lists keyboard controls, command equivalents, and whether
  a key changes setup state or mutates scene state.
- Controller History records reusable command forms such as `motion vector`,
  `motion magnitude`, `actor set`, and `movement set`.
- A new user can complete basic tasks without guessing: select actor, choose
  movement algorithm, adjust arrow magnitude, apply WASD movement, save scene,
  reload scene, and confirm the vector/actor state survived.

These gates matter because player actors, NPC actors, and algorithmic movement
raise the cost of confusing controls. A mistaken arrow press should be harmless;
the intentional action key should be the moment the scene changes.

## Animation Save Levels

BGE should treat animation save support as three related capabilities.

```text
Animation state     save whether motion is running plus current object position, velocity, and scene time
Animation clip      save authored keyframes, curves, duration, loop mode, and target object/component IDs
Animation export    bake or package clips so an exported game can play them without the editor
```

The first implementation only needs animation state. The current bouncing-ball
runtime already has enough state to resume simple procedural motion: object
positions, velocities, radius, color, selected renderer, and whether animation is
running. A saved `.bgescene` should restore those values so loading the scene can
continue from the saved moment.

That is not the same as saving a full animation timeline. A future `.bgeanim`
file should handle authored animation:

```json
{
  "schema": "bge.animation.v1",
  "id": "anim.ball.pulse",
  "durationSeconds": 2.0,
  "loop": true,
  "tracks": [
    {
      "target": "object.slot.1",
      "property": "radius",
      "keys": [
        { "time": 0.0, "value": 32.0 },
        { "time": 1.0, "value": 48.0 },
        { "time": 2.0, "value": 32.0 }
      ]
    }
  ]
}
```

This keeps procedural runtime state, editor-authored animation, and exported game
playback cleanly separated.

## Movie And MP4 Export

Saving an animation as a movie is a third concern: it records rendered output,
not just game state or animation curves.

BGE should support this in layers:

```text
Frame capture       save viewport frames as numbered PNG files
Deterministic render render a scene/animation range at fixed timestep and FPS
Movie encoding      encode the frame sequence and audio track into MP4
```

The first version should export an image sequence before MP4. A PNG sequence is
simple, testable, and debuggable: every frame can be inspected when timing,
camera, or rendering is wrong.

MP4 export can come after frame export. On Windows, BGE can use Media Foundation
for an in-engine encoder path. An optional external encoder profile can also call
`ffmpeg` when present, but that should be an explicit tool dependency rather
than hidden engine magic.

Example movie export profile:

```json
{
  "schema": "bge.movie-export.v1",
  "sourceScene": "scenes/main.bgescene",
  "sourceAnimation": "animations/intro.bgeanim",
  "output": "exports/intro.mp4",
  "width": 1920,
  "height": 1080,
  "fps": 60,
  "durationSeconds": 10.0,
  "fixedTimestep": true,
  "encoder": "media-foundation",
  "bitrateMbps": 12
}
```

Movie export should work even when the editor is not interactive. The exporter
should be able to load a project, load a startup scene, advance time at a fixed
rate, render frames, and write either a frame sequence or an MP4 file.

Later this can evolve into an entity/component model without breaking old files:

```text
entity.id
component.transform2d
component.shape.circle
component.motionVector
component.actor
component.movementController
component.sequenceGroup
component.timelineTrack
component.sprite
component.physics2d
component.audio
component.inputBinding
component.animationPlayer
component.script
```

## Asset Rules

Assets need stable IDs independent of file paths. Paths move; IDs should not.

Each asset should have:

```text
id          stable project-local identifier
kind        image, sound, script, scene, font, material, shader, prefab
path        path relative to project root
hash        optional content hash for build reproducibility
importer    optional importer/version metadata
```

The save file stores asset IDs and relative paths, not absolute developer paths.
Export copies assets into a build folder and writes a build manifest.

## Command Journal

BGE already records command equivalents in controller History. That should grow
into an optional command journal:

```json
{
  "schema": "bge.journal.v1",
  "project": "sample.game",
  "entries": [
    {
      "time": "2026-05-20T12:00:00Z",
      "target": "bge.game-loop",
      "command": "add object 1",
      "result": "ok"
    }
  ]
}
```

The journal should not replace scene saves. It should support undo/redo, replay,
smoke tests, auditability, and crash recovery.

## Export Model

Exporting a game is different from saving a project.

Save means:

```text
write editable project/scene files
```

Export means:

```text
copy the engine runtime
copy selected scenes and assets
write a runtime config pointing at the startup scene
optionally render frame sequences or MP4 movie exports
optionally rename/icon/sign/package the executable
write a build manifest
```

A first Windows export can be folder-based:

```text
build/windows-x64/SampleGame/
  SampleGame.exe
  game.bgeproj
  scenes/main.bgescene
  assets/...
  BgeRuntime.json
  build.bgebuild
```

A later release export can add installer packaging or app container formats, but
the folder export should come first because it is easier to test.

## Executable Game Strategy

There are three plausible executable strategies. BGE should support them in this
order.

### 1. Runtime Wrapper

Ship the same BGE executable with a runtime config.

```text
BasicGameEngine.exe --run-project game.bgeproj
```

Pros: fastest path, lowest code duplication, easy smoke tests.
Cons: executable name/icon still look like BGE unless renamed or wrapped.

### 2. Renamed Runtime Export

Copy `BasicGameEngine.exe` to `SampleGame.exe` and include project files beside
it.

```text
SampleGame.exe
game.bgeproj
scenes/main.bgescene
```

Pros: feels like an exported game.
Cons: icons/version resources need a later customization step.

### 3. Generated Game Project

Generate a small Visual Studio project that links/references the BGE runtime and
embeds startup project metadata.

Pros: best long-term productization and custom code.
Cons: more build-system work; should wait until save/load and folder export are
stable.

## Implementation Sequence

### Phase 1: Snapshot Save/Load

Add a `BgeSceneState` model that captures only current generic runtime state:

```text
renderer api
animation running
scene time
selected object slot
active sequence group
sequence group items, up to 10 per group
motion direction and magnitude
actor id and movement algorithm
edit mode
edit rate
background asset/path
object slot array
sound slot
```

Add functions:

```text
CaptureSceneState()
ApplySceneState(state)
SaveScene(path, state)
LoadScene(path, state)
```

Add commands:

```text
save scene <path>
load scene <path>
```

This phase saves animation state only. It should restore moving objects at their
saved positions and velocities and decide from `animationRunning` whether motion
continues after load.

### Phase 2: Project Manifest

Add `.bgeproj` support with project root, scene list, assets, and startup scene.

Add commands:

```text
new project <folder>
open project <path>
save project
```

### Phase 3: Actor Motion Components

Promote the current velocity arrow into a reusable motion vector component and
attach optional actor/controller metadata. The first implementation can keep
using the existing object slots while saving additional fields for direction,
magnitude, actor id, and movement algorithm.

Add commands:

```text
motion vector <slot> <dx> <dy>
motion magnitude <slot> <speed>
actor set <slot> <actor-id> <kind>
movement set <slot> movement.bounce2d|movement.input.wasd|movement.seek
```

### Phase 4: Animation Sequencing Window

Add a controller-owned or worker-owned sequencing window that edits named groups
of up to 10 items. The animation continues to play in the renderer, but timing,
group selection, active item selection, actor assignment, movement algorithm,
clip selection, and save/export commands are controlled from the sequencing
window.

Add commands:

```text
sequence group add <name>
sequence group select <name>
sequence item set <slot> <actor-id>
sequence item movement <slot> <movement-id>
sequence play|pause|stop
sequence step frame|back
```

### Phase 5: Preview Group Composition

Add a preview window/profile that composes selected groups without changing the
active editing group. Start with include/exclude group selection and playback
controls. Editing remains in the regular 10-item group view.

Add commands:

```text
preview group add <name>
preview group remove <name>
preview group only <name>
preview groups all|none
preview play|pause|stop
```

### Phase 6: Ghost And Ping References

Add read-only ghost overlays and ping highlights for non-active groups. The user
can choose one or more groups as alignment context while continuing to edit only
the active 10-item group.

Add commands:

```text
ghost group add <name>
ghost group remove <name>
ghost groups clear
ghost opacity <0-100>
ghost ping-rate <seconds>
ping group <name>
ping item <group> <slot>
ping path <group>
ping selected-ghosts
ping stop
```

### Phase 7: Authored Animation Clips

Add `.bgeanim` files and an animation-player component that can target object or
component properties. Start with transform/radius/color tracks before supporting
scripted or skeletal animation.

Add commands:

```text
animation save <path>
animation load <path>
animation play <id>
animation stop <id>
```

### Phase 8: Frame Capture And Movie Export

Add deterministic frame capture before MP4. This should render at fixed timestep
and write numbered frames:

```text
exports/movie/intro_000001.png
exports/movie/intro_000002.png
exports/movie/intro_000003.png
```

Add commands:

```text
capture frame <path>
export frames <scene> <output-folder> --fps 60 --seconds 10
export movie <scene> <output.mp4> --fps 60 --seconds 10
```

MP4 can use Windows Media Foundation first, with optional external `ffmpeg`
profiles later.

### Phase 9: Autosave And Recovery

Autosave the current scene to a `.autosave` folder and keep a small journal of
recent commands. Recovery should offer the newest autosave if BGE starts after a
crash.

### Phase 10: Folder Export

Add an export command that creates a runnable folder:

```text
export windows-folder <output-folder>
```

The first export can copy the Debug/Release BGE executable, project manifest,
scene files, and assets. It should produce `build.bgebuild` with timestamps,
engine version, source paths, and copied files.

### Phase 11: Executable Packaging

Add export profiles for custom executable name, icon, version metadata, and
release/debug selection.

## Code Placement

Keep this generic and small:

```text
BasicGameEngine/include/BgeSceneState.h
BasicGameEngine/src/BgeSceneState.cpp
BasicGameEngine/include/BgeProject.h
BasicGameEngine/src/BgeProject.cpp
BasicGameEngine/include/BgeExport.h
BasicGameEngine/src/BgeExport.cpp
```

The existing renderer slot type should remain in `BgeScenePrimitives.h`, but
save/load should not serialize directly from UI controls. It should serialize
from captured state.

## Acceptance Gates

Phase 1 is complete when:

- adding objects, setting colors/vectors, choosing renderer, choosing mode/rate,
  selecting background, and stopping/starting animation can be saved;
- restarting BGE and loading the scene restores the same visible state;
- loading a scene with `animationRunning=true` resumes procedural motion from the
  saved positions and velocities;
- saved vector arrows round-trip as direction plus magnitude, not only as a
  drawn UI affordance;
- WASD/arrow-key usability gates pass for the current motion-vector editor;
- save output uses relative asset paths when inside the project root;
- malformed files fail with clear status text and do not crash;
- command bar and controller History show reusable `save scene` and `load scene`
  commands.

Phase 4 is complete when:

- the animation sequencing window can create uniquely named groups;
- the group dropdown switches the active working set;
- the plus button creates a new group without disrupting the current scene;
- each group enforces a maximum of 10 active items;
- sequencing actions have command equivalents and appear in Controller History;
- save/load restores group names, active group, item assignments, movement
  algorithms, and timeline/playback state.
- UI complexity remains bounded: the active sequencing surface never requires
  the user to manage more than 10 items before switching groups.

Phase 5 is complete when:

- preview can include one group, selected groups, all groups, or no groups;
- preview group selection does not change the active editing group;
- playback controls work against the selected preview group set;
- preview commands are recorded in Controller History;
- export/movie profiles can reuse the same group selection model.

Phase 6 is complete when:

- one or more non-active groups can be shown as read-only ghosts;
- ghost groups do not count against the active 10 editable items;
- pinging a group, item, or path gives clear temporary visual focus;
- ping starts as a hard outline, falls through subtle tones, fades to nothing,
  and repeats only according to the selected ghost groups' ping rate;
- WASD/ASDW and arrow actions mutate only the active group, never ghost groups;
- ghost and ping commands are recorded in Controller History.

Phase 8 is complete when:

- BGE can capture the current renderer output to a still image;
- BGE can render a deterministic fixed-timestep frame sequence for a scene or
  animation range;
- MP4 export either succeeds through the configured encoder or reports a clear
  missing-encoder/status message;
- frame/movie exports are listed in a manifest with resolution, FPS, duration,
  encoder, and source scene/animation.

Phase 10 is complete when:

- export creates a folder with an executable, project file, startup scene, and
  copied assets;
- running the exported executable opens the startup scene;
- missing assets produce a useful error;
- export writes a manifest listing every copied file.

## Recommendation

Start with scene snapshot save/load, not export. Export depends on having a
stable project and scene file. The shortest strong path is:

```text
BgeSceneState -> .bgescene save/load -> .bgeproj -> folder export -> renamed executable
```

That sequence keeps BGE useful as a small game engine today and leaves a clean
route toward exported executable games later.
