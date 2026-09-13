# Recur CLI Cheatsheet & Operational Guide

This document captures exact CLI syntax, discovery behavior, and conventions for working with `recur` in this repository.

---

## 1. Hierarchy & File Discovery

### `recur tree`
Displays hierarchical dot-separated structure as an ASCII tree.
* **Important:** Requires a `<BASE>` argument; it does not default to root if omitted.
* Use `-d <DIR>` to specify project root or subdirectory.
```powershell
# Inspect the entire bge hierarchy from project root
recur tree bge -d .

# Inspect warp bubble slices specifically
recur tree bge -d .recur/warp

# Inspect other domain trees
recur tree space-rocks -d .
```

### `recur files`
Lists files matching a hierarchical wildcard pattern.
* **Important:** Requires a `<PATTERN>` argument.
```powershell
# List all hierarchical files in workspace
recur files "**" -d .

# Search for specific lane patterns
recur files "bge.tests.**" -d .
recur files "*.recur.md" -d .
```

---

## 2. Lane Rehydration (`recur reveal`)

`recur reveal` surfaces ignition capsules (`*.recur.md`) without needing to traverse the repository.

```powershell
# List all available reveal capsules
recur reveal

# Reveal a specific capsule (e.g. newly created tests lane or honing lane)
recur reveal bge.tests.all-passing.expanding
recur reveal bge.eventness.open-portable-honing.collapsing
```

* Capsule files are stored directly in `.recur/<lane-name>.recur.md`.
* `skip_persona_if_known = true` is configured in policy: repeated reveals within the same continuous session suppress redundant persona banners.

---

## 3. Warp Bubbles & Eventness Scoring (`recur warp`)

`recur warp` is a read-only query engine that scores lane evidence and residuals.

### Suffix Status Mapping
Default mapping from suffix to state:
* `.current.md` $\rightarrow$ Active (emits residual `missing-verification` when incomplete)
* `.complete.md` $\rightarrow$ Complete
* `.strange.md` $\rightarrow$ Interesting
* `.blocked.md` $\rightarrow$ Blocked

### Commands
```powershell
# Inspect warp suffix configuration
recur warp config -d .

# Score a warp lane against its evidence files
recur warp status bge.tests.warp0.slice0-baseline -d .recur/warp

# Explain the residuals and evidence breakdown
recur warp explain bge.tests.warp0.slice0-baseline -d .recur/warp
```

### Warp Bubble Map & Slices
* Bubble maps are named `<warp-id>.warp-map.json` using schema `warp-bubble-map-v1`.
* Maps specify `required_slices`, `contract_hash`, `depends_on`, and `evidence_gates`.
* Active lane slices transition from `.current.md` $\rightarrow$ `.complete.md` as implementation layers satisfy their contract gates.
