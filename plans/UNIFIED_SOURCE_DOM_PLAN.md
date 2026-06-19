# Unified Source DOM Plan

This plan tracks the long-running migration toward one shared, lossless Therion source model for `.th`, `.th2`, and `thconfig` files. Stable ownership and dependency rules remain in `ARCHITECTURE.md`; active operational priorities remain in `WORKLOG.md`.

## Phase Plan

- Phase 1 - Source snapshot foundation: preserve lines, comments, blanks, indentation, newlines, encoding, source type, offsets, token spans, and revision identity.
- Phase 2 - Logical command layer: group continuations, classify block-body rows, record block ranges, and expose command/argument/option source ranges.
- Phase 3 - Catalog-aware metadata: provide command domains, aliases, arity, allowed values, arguments, and document-type applicability from metadata.
- Phase 4 - Validator migration: make `TherionSourceValidator` consume the shared projection while preserving ranges, severity policy, and false-positive coverage.
- Phase 5 - Raw editor migration: drive highlighting, context help, completion, tooltips, and underlines from shared revision-keyed source data.
- Phase 6 - Blocks projection migration: read Blocks cards, nesting, references, fixed root commands, data bodies, and details from the shared projection.
- Phase 7 - Map/TH2 projection migration: read TH2 objects, references, Bezier geometry, backgrounds, and Smart Area insertions from the shared projection.
- Phase 8 - Structure/project index migration: feed Structure, validation, search, namespaces, root-config inference, and diagnostics from cached DOM snapshots.
- Phase 9 - Transaction and cache model: centralize source mutations, undo labels, revision checks, dirty state, projection invalidation, and selection restore.
- Phase 10 - Legacy removal gates: remove editor-local parsers and serializers only after consumers migrate and regression coverage exists.

## Current Implementation Baseline

- `TherionSourceText` preserves physical source lines and line endings for targeted source edits.
- `TherionSourceDocument` owns parsed physical lines, source metadata, block stack state, block ranges, source line roles, and round-trip text reconstruction.
- `TherionSourceLogicalDocument` owns continuation grouping, logical command ranges, physical token/argument/option ranges, block context, and catalog-aware command metadata.
- `TherionCommandLineModel`, `TherionTokenRules`, and `TherionStringUtils` are the current shared command/token/string rule seams.
- `TherionSourceValidator`, `ProjectStructureIndex`, Raw completion/help/highlighting paths, and several Blocks/Map helpers already consume shared command/source parsing in focused areas.
- `TextEditorSourceTransactionController` is the current central source-transaction seam for source snapshots, undo labels, revision checks, projection invalidation, and selection restoration hooks.
- Map editor source writes are partially routed through `applySourceTextChangeWithSnapshot`, but several projection/rewrite helpers still own local source-shape knowledge.
- `TherionSourceDocumentTest` and `TherionSourceLogicalDocumentTest` now run inside `TherionCoreQTests`, with QTest coverage for physical/logical source snapshot roles, ranges, metadata, continuation handling, and catalog metadata.
- `TherionSourceSnapshotCache` provides the first narrow revision-keyed cache for physical and logical source projections. It only caches snapshots with positive source revisions, and catalog-backed logical projections require an explicit catalog revision key.
- Raw context-help token lookup now reuses `TherionSourceSnapshotCache` keyed by the editor document revision instead of reparsing the full document for each help-token lookup.
- Raw completion scope analysis, cursor token context, and required-argument tooltip lookup now reuse the same revision-keyed source snapshot cache from `RawEditorCompletionController`.

## Remaining Slices

### Slice 2 - Shared Projection Cache

Goal: stop reparsing full documents for cursor movement, context help, completion, structure refresh, and unrelated inspector/UI updates.

- Expand `TherionSourceSnapshotCache` into Structure/Validation only after stale-projection behavior is covered.
- Keep cache ownership outside widgets; UI shells should request snapshots by document revision or explicit text input through a narrow collaborator.
- Add invalidation tests for text edits, undo/redo, document reload, source type changes, and catalog refresh.

### Slice 3 - Raw Editor Consumer Migration

Goal: make Raw mode a projection over the shared source snapshot instead of a set of local parsing passes.

- Route syntax highlighting and validation underlines through the cached logical document where practical.
- Preserve current source ranges and false-positive behavior while migrating one consumer at a time.
- Add focused regressions for continuation lines, block-content rows, quoted strings, options with embedded values, and thconfig-specific command applicability.

### Slice 4 - Blocks Consumer Migration

Goal: make Blocks cards and details read command/source structure from the logical document.

- Replace local command/option scans in Blocks details, option args, and move planning with logical command ranges where source fidelity matters.
- Keep block move/rewrite behavior source-preserving and one undo transaction per user-visible action.
- Add regressions for nested blocks, fixed root commands, data bodies, comments, continuation rows, and move undo/redo.

### Slice 5 - Map/TH2 Projection Migration

Goal: reduce TH2 map parser drift by making Map scene objects consume shared command and option ranges.

- Migrate Map object discovery, option parsing, area/line reference resolution, and Smart Area insert planning to shared logical commands in small vertical slices.
- Keep geometry-specific parsing in map-focused types, but remove duplicated generic command/option token rules.
- Add round-trip and undo/redo coverage for line/area/point edits, background metadata, object delete/move, and inspector quick-field writes.

### Slice 6 - Transaction Ownership Closure

Goal: make source mutation semantics uniform across Raw, Blocks, Map, inspector, validation fixes, and background workflows.

- Route remaining user-visible source mutations through `TextEditorSourceTransactionController` or an equivalent narrow successor.
- Require each source transaction to carry an undo label, expected revision when available, dirty-state behavior, projection invalidation policy, and selection/cursor restoration policy.
- Keep structure guardrails preventing direct map-editor source mutation bypasses.

### Slice 7 - Structure, Project Index, And Diagnostics

Goal: feed orientation and validation surfaces from cached DOM snapshots instead of independent reparsing.

- Reuse cached logical documents for Structure, project indexing, namespace/reference resolution, search, and validation.
- Keep Therion namespace semantics exactly as documented in `docs/THERION_COMPATIBILITY.md`.
- Make live diagnostics debounced, cancellable, revision-keyed, and centralized in the Validation panel.

### Slice 8 - Legacy Removal Gates

Goal: delete duplicate parsing/rewrite code only after coverage and consumers have moved.

- Track remaining editor-local tokenizers, option parsers, line splitters, numeric classifiers, and source-range heuristics.
- Remove one legacy path at a time after a migrated consumer has regression coverage.
- Keep unknown valid directives, comments, formatting, encodings, and line endings round-trip safe.

## Verification Gates

- Parser/projection coverage for each migrated consumer.
- Round-trip coverage for source-sensitive edits.
- Source-range coverage for diagnostics and mutations.
- Undo/redo coverage for user-visible source changes.
- Affected editor regression coverage for Raw, Blocks, Map, Structure, Validation, and compiler-facing workflows.
