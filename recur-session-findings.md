# Recur: Fixes and Improvements Identified in This Session

## Context

Recur helped coordinate the BasicGameEngine unit-test work through Slice 1, Slice 2, Slice 3, and Slice Final. The final recorded build succeeded, and Visual Studio reported 50 tests passed, 0 failed, and 0 skipped.

This document distinguishes observed behavior from proposed improvements. Recur's implementation was not inspected, so these are product requirements and investigation targets, not confirmed source-code diagnoses.

## 1. Distinguish Recorded Completion from Verified Evidence

**Priority:** High

### Observation

After the slice receipts used the configured `.complete.md` suffix, `recur warp status` returned `optimum`, zero residuals, and a `complete-state-present` signal. The returned trace-role counts were all zero. Build and test execution happened separately through Visual Studio; recur did not independently execute or validate those checks in this session.

### Proposed fix

Make the distinction explicit in both human-readable and JSON output:

- Recorded state: a completion artifact exists.
- Evidence status: absent, declared, checked, stale, or failed.
- Contract status: required gates and dependencies are satisfied or unresolved.

Do not present a suffix-derived completion verdict as equivalent to verified test success. Preserve compatibility with existing verdicts if necessary, but qualify what the verdict proves.

### Acceptance criteria

- A `.complete.md` file without supporting evidence is labeled as recorded completion, not verified success.
- Output identifies whether evidence was merely declared or actually checked.
- A failed or unresolved mandatory gate remains visible even when a completion file exists.

## 2. Provide a Supported Completion-Receipt Workflow

**Priority:** High

### Observation

Receipts were initially written as `.verified.md`. The configured policy recognized only `current`, `complete`, `strange`, and `blocked`. `recur warp status` reported that no eventness files were found until the receipts were renamed to `.complete.md`.

The `recur warp --help` output exposed read-only status and management queries. A receipt-writing workflow was not demonstrated in this session; this does not establish that no other recur command offers one.

### Proposed fix

Expose or clearly document a receipt-creation workflow that:

- Reads the active suffix policy instead of requiring callers to guess filenames.
- Produces a receipt template with the slice ID, contract reference, dependencies, gates, and evidence fields.
- Uses the recognized completion suffix.
- Avoids overwriting existing receipts and reports conflicting state artifacts.

### Acceptance criteria

- A newly created receipt is immediately discoverable by `warp status`.
- Custom suffix policies work without callers hard-coding `.complete.md`.
- Conflicting or duplicate receipts produce actionable diagnostics.

## 3. Improve Diagnostics for Unrecognized State Suffixes

**Priority:** High

### Observation

With a matching lane receipt named `.verified.md`, the response was effectively `no eventness files found`. That was technically consistent with the configured policy, but did not explain that a nearby file used an unrecognized suffix.

### Proposed fix

When no recognized eventness file is found, inspect nearby filenames for the requested lane and explain possible suffix mismatches. Offer a non-destructive suggestion rather than silently treating an unknown suffix as complete.

### Acceptance criteria

- A matching `.verified.md` file produces a message identifying the file and the recognized suffixes.
- The diagnostic distinguishes a missing lane, a wrong search directory, and an unsupported suffix where possible.
- Suggested renames never overwrite another artifact without explicit conflict handling.

## 4. Surface the Active State Policy in `reveal`

**Priority:** Medium

### Observation

`recur reveal` usefully exposed the next task, contract paths, and build command. It also displayed execution policy fields. The completion suffix policy was discovered later through a separate `recur warp config --json` call.

### Proposed fix

For warp-oriented lanes, include the effective eventness suffix policy or an explicit pointer to it in `reveal`. Include the policy's source so callers can understand whether it came from defaults or project configuration.

### Acceptance criteria

- A caller can determine the correct receipt suffix before creating a file.
- Output shows the effective policy and where it was loaded from.
- Machine-readable output provides the same policy information as human-readable output.

## 5. Validate the Warp Map as a Whole

**Priority:** High

### Observation

The session's warp map listed required slices, dependency relationships, contract identifiers, and evidence gates. Individual lane status checks were demonstrated, but a map-wide dependency and gate validation result was not.

### Proposed fix

Provide or document a map-level validation operation that checks:

- Every required slice has an identifiable receipt.
- Slice dependencies are satisfied and contain no cycles or missing references.
- Receipt contract references match the current map.
- Required gates have evidence with explicit outcomes.
- Contradictory or stale receipts are reported.

Clarify whether a `contract_hash` field is an opaque identifier or a cryptographically validated digest. The map used `sha256:`-prefixed human-readable values; this session did not establish how recur interprets that field.

### Acceptance criteria

- A final receipt alone cannot satisfy a map with missing prerequisite receipts.
- Changing a contract invalidates or flags evidence tied to the old contract, according to documented rules.
- Output lists unresolved gates and dependencies by slice.
- Historical baseline artifacts can remain available without being mistaken for unfinished implementation work.

## 6. Support Structured, Fresh Build and Test Evidence

**Priority:** Medium

### Observation

Build and test evidence was manually summarized in Markdown receipts. The actual verification included a Debug x64 solution build, a full 50-test run, and a source scan showing zero `Assert::Fail` calls. The receipts were not demonstrated to be machine-checked against those results.

### Proposed fix

Define an evidence format that can reference external build/test results without requiring recur to become a build system. Useful fields include:

- Evidence kind and producer.
- Project, configuration, and platform.
- Execution timestamp and result-artifact path.
- Test totals: discovered, executed, passed, failed, and skipped.
- Build outcome or process exit code, when available.
- Source revision or content fingerprint, including a way to represent a dirty working tree.

Report evidence freshness and completeness. Allow manual declarations, but identify them as declarations.

### Acceptance criteria

- Zero executed tests cannot satisfy an all-tests-passing gate.
- Skips and failures cannot disappear behind a passing percentage.
- Missing result artifacts and source changes after verification are visible.
- External runner evidence can be attached without rerunning commands automatically.

## 7. Reconcile Lane Summaries with Receipts

**Priority:** Medium

### Observation

The active `expanding.recur.md` entry initially contained aspirational text under `Completed Reality` while its readiness fields still pointed to Slice 1. After execution, the entry was manually updated to point to the final receipt and report completion.

### Proposed fix

Separate desired outcomes from observed completion in lane templates. Provide a reconciliation check that compares readiness fields and summary claims with the referenced receipts, while preserving human-authored intent and historical evidence.

### Acceptance criteria

- Templates label target outcomes distinctly from verified results.
- A summary that still says `ready to implement slice 1` while all required slices are recorded complete produces a warning.
- Reconciliation proposes changes or uses explicitly generated fields; it does not silently overwrite narrative content.

## Suggested Implementation Order

1. Improve unsupported-suffix diagnostics and expose the suffix policy in `reveal`.
2. Document or add a policy-aware receipt-creation workflow.
3. Separate recorded completion from checked evidence in status output.
4. Add structured evidence ingestion and freshness checks.
5. Add map-wide gate/dependency validation and summary reconciliation.

## What Worked Well

- `recur reveal` provided a useful entry point and next-task context.
- The warp map and baseline made the work order and evidence gates explicit.
- `recur warp config` revealed the receipt-suffix mismatch.
- `recur warp status` confirmed recognition of the corrected completion artifacts.
- Per-slice receipts created a readable handoff and audit trail.

## Problems Not Attributed to Recur

The following occurred in separate tools or assistant behavior and should not be filed as recur defects based on this session:

- Visual Studio editor tools could not modify a `.vcxproj` file.
- Some test-selection filters selected zero tests; exact fully qualified names worked.
- The assistant's separate plan tracker reported inconsistent step identifiers during closure.
- The assistant's closing summaries incorrectly said recur bookkeeping remained unverified, even though the active entry and completion filenames had been checked successfully.

## Overall Assessment

Recur was useful as a workflow-state and handoff tool. The most important improvement is to make its completion claims precise: distinguish a recognized completion artifact from evidence that has actually satisfied the declared contract.
