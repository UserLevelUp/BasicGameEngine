# BGE Animation Assist Techniques

Status: research and architecture analysis
Scope: generic BasicGameEngine animation/editor guidance

## Purpose

Analyze animation-assist techniques from established tools and translate the
useful parts into BGE's simpler model: named groups, 10 editable items at a time,
preview composition, ghost/ping references, save/load, and export.

The target is not to clone a full animation package. The target is a small,
understandable animation workflow that helps a user place motion correctly while
keeping the working surface bounded.

## What Other Tools Teach

### Onion Skinning

Traditional animation used translucent paper and light tables so animators could
see nearby frames while drawing the current one. Digital onion skinning keeps the
same idea: show previous and/or future frames as translucent references.

Useful patterns:

```text
before frames       show earlier positions/poses
after frames        show future positions/poses
opacity             make references faint enough not to compete with the current frame
fade by distance    older/farther frames get more transparent
before/after colors distinguish past from future
selected mode       show only selected frames/keyframes when the timeline is busy
layer toggle        enable/disable onion skins per layer/group
```

BGE lesson: onion skins should be read-only references. They help the user align
motion, but the current frame and active group remain the only editable target.

### Timelines, Tracks, And Keyframes

Game and animation editors commonly separate an animation into tracks. Each track
targets a property on an object/actor and stores keyframes over time.

Useful patterns:

```text
track target        object, actor, component, or property
keyframe time       where the value changes
keyframe value      position, rotation, scale, color, radius, direction, etc.
interpolation       nearest, linear, cubic, angle-aware, stepped
update mode         continuous, discrete, captured/blended start
loop mode           clamp, wrap, ping-pong, section playback
```

BGE lesson: `.bgeanim` should eventually store tracks and keyframes, but the UI
should expose only the tracks for the active 10-item group unless the user opens
preview or ghost references.

### Markers And Sections

Animation tools often let users name points or ranges on the timeline. A section
can be previewed or exported without playing the whole animation.

Useful patterns:

```text
marker name         jump, attack, idle, turn, impact, end
marker color        visual grouping or priority
section playback    play between two markers
section export      render only the selected interval
```

BGE lesson: markers should be cheap and named. They help users work on small
ranges even when the final animation combines many groups.

### Reset Or Reference Pose

Some tools keep a reset/default state so saving, blending, and returning to a
known base pose are reliable.

Useful patterns:

```text
reset frame         canonical state at time 0
reference values    values used for blending and recovery
apply reset         restore safe editor baseline
```

BGE lesson: each sequence group should have a reset/reference state. Save/load
should know whether it is saving the current time, the reset state, or both.

### Runtime Versus Editor Separation

Timeline systems commonly separate editor assets from runtime playback. The
editor manages tracks, clips, markers, and preview controls. The runtime plays a
compiled or loaded version.

Useful patterns:

```text
editor asset        editable timeline/clip data
runtime instance    playback state bound to scene objects
preview instance    editor-only playback for inspection
export manifest     exact clips, assets, and settings packaged for runtime
```

BGE lesson: the sequencing window controls the editor asset; the renderer shows
the runtime/preview result. Export packages selected scenes, groups, clips, and
assets without exposing every internal editor control.

## BGE Animation Assist Vocabulary

BGE should use a small set of assist types:

```text
onion skin       same group, previous/future frames around current time
ghost group      other group shown read-only while editing active group
ping             temporary highlight of a group, item, frame, marker, or path
motion path      read-only trail of where an item moves over time
marker           named timeline point
section          range between two markers or explicit time values
reset state      canonical group/item baseline for save/load/blending
preview set      groups included in full-animation preview
```

These should not all become separate complicated windows. They are overlays and
filters on the same simple sequencing workflow.

## Proposed BGE UI

### Sequencing Window

The sequencing window remains the editor control surface.

```text
Group dropdown        active editable group
+ button              create named group
10 item slots         active editable items only
Timeline strip        current time, markers, section range, keyframes
Onion controls        before/after count, opacity, fade, before/after colors
Ghost controls        selected ghost groups, opacity, ping buttons
Motion path toggle    show active item/group paths
Preview button        open selected-group preview window
Save/export buttons   save scene, save animation, export frames/movie
```

The active group still caps editing at 10 items. Onion skins show temporal
context for those 10 items. Ghosts show other groups but remain read-only.

### Preview Window

The preview window composes selected groups, not direct editing.

```text
current group      preview only active group
selected groups    preview chosen groups
all groups         preview entire scene/animation
export set         preview what export/movie will render
```

Preview should have playback and scrub controls, but item editing still returns
to the sequencing window's active group.

### Renderer Overlay

The renderer should be able to show:

```text
current editable items     normal styling
onion previous frames      tinted/faded past positions
onion future frames        tinted/faded future positions
ghost groups               faint read-only overlays
ping highlights            temporary pulse/outline
motion paths               trails, arrows, or dots along sampled time
markers/sections           optional timeline indicators, not viewport clutter
```

## Sonar-Style Ping Behavior

Ping should feel like a visual sonar pulse over selected ghost groups. It is not
a steady selection state. It starts sharp, gives the eye a clear reference, then
fades away until the next ping interval.

Recommended ping lifecycle:

```text
onset          hard black/white outline around ghost items
peak           crisp high-contrast shape, easy to notice immediately
tonal falloff  subtle control-tonal glow, tint, or ring as the pulse softens
fade           opacity drops smoothly toward zero
rest           no ping visible until the next ping-rate interval
```

Repeated pings should only occur for ghost groups the user selected. If multiple
ghost groups are pinging, BGE can stagger them slightly or use group-specific
falloff tones after the hard black/white onset. The ping remains read-only: it
can guide alignment, but it cannot become the edited target unless the user
switches active groups.

Suggested ping controls:

```text
ping rate        interval between repeated pings for selected ghost groups
ping duration    how long one pulse lasts before fading to zero
ping contrast    black/white outline strength at onset
ping tone        optional tint/glow color during falloff
ping target      group, item, path, marker, or selected ghost set
```

## BGE Data Model Additions

A compact schema can support the techniques without forcing a large editor.

```json
{
  "schema": "bge.sequence-assist.v1",
  "activeGroup": "group.player-and-npcs",
  "onion": {
    "enabled": true,
    "mode": "frames",
    "before": 2,
    "after": 2,
    "opacity": 0.35,
    "fadeByDistance": true,
    "beforeColor": [0.35, 0.55, 1.0],
    "afterColor": [1.0, 0.45, 0.35]
  },
  "ghosts": [
    {
      "group": "group.background-traffic",
      "opacity": 0.18,
      "showPaths": true,
      "ping": {
        "enabled": true,
        "rateSeconds": 2.0,
        "durationSeconds": 0.45,
        "onset": "black-white-outline",
        "tone": [0.55, 0.75, 1.0]
      }
    }
  ],
  "markers": [
    {
      "name": "impact",
      "time": 1.25,
      "color": [1.0, 0.8, 0.2]
    }
  ],
  "preview": {
    "profile": "selected-groups",
    "groups": ["group.player-and-npcs", "group.background-traffic"]
  }
}
```

## Commands

```text
onion on|off
onion frames <before> <after>
onion opacity <0-100>
onion fade on|off
onion colors <before-color> <after-color>

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

marker add <name> <time>
marker remove <name>
section set <start-marker> <end-marker>
section clear

preview group add <name>
preview group remove <name>
preview groups all|none
preview play|pause|stop
```

## Usability Rules

- Current editable frame/group must be visually dominant.
- Onion skins, ghosts, paths, and pings are references, not editable targets.
- Before and after frames must be visually distinct.
- Opacity must be adjustable because useful reference strength varies by scene.
- The user can work with many groups by previewing or ghosting them, but only 10
  active items are editable at once.
- Clicking or keying a ghost should select or ping its group, not mutate it.
- If visual clutter rises, default to fewer reference frames and lower opacity.

## BGE Implementation Order

```text
1. Motion path sampling for active item/group.
2. Onion skin overlay for active group with before/after frame counts.
3. Opacity, fade, and before/after colors.
4. Ghost group overlay with ping highlight.
5. Timeline markers and section playback.
6. Preview group profiles reused by movie export.
```

This order keeps the first useful slice small. Motion paths and onion skins help
immediately with the current vector/actor model. Ghosts and preview composition
then help align many 10-item groups without breaking the simple editor rule.

## Acceptance Gates

- Onion skins show previous and future positions for the active group only.
- Onion controls include before/after count, opacity, fade, and colors.
- Ghost overlays show selected non-active groups read-only.
- Ping highlights a group, item, or path without changing active group selection.
- Ping starts as a hard black/white outline, falls through optional tonal color,
  and fades fully to zero.
- Repeated pings only occur for user-selected ghost groups and obey the current
  ping rate.
- Motion paths help align current actors against onion/ghost references.
- Preview can compose selected groups while editing remains on one active group.
- Commands appear in Controller History with reusable text.
- Save/load preserves onion settings, ghost selections, preview profiles,
  markers, sections, and reset/reference state.

## Recommendation

Borrow the concepts, not the complexity. BGE should use onion skins, ghost
groups, pings, motion paths, markers, and preview profiles as small assists
around the 10-item group workflow. That gives animators the context they need
without turning the editor into a dense professional animation suite.
