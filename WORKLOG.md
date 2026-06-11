# Worklog

Active work only. Completed history is archived in `WORKLOG_ARCHIVE_2026-05-13.md` and `WORKLOG_ARCHIVE_2026-05-23.md`.

## In Progress

- P1 - Localization hygiene: keep Czech/Slovak UI catalogs, canonical Therion syntax, and Qt/platform language selection current.
- P1 - Validation and catalog metadata: keep validation conservative while moving command/option interpretation into catalog-backed logical-command metadata.
- P1 - Blocks editor: preserve document-domain filtering, data-block ownership, no-open source mutations, and single-transaction block-detail edits.
- P1 - Map editor parity: continue XTherion-aligned selection, context menus, panning, Bezier input, Smart Area, and TH2 source sync.
- P1 - Inspector consistency: keep Raw, Blocks, and Map inspectors on the shared panel foundation with aligned tabs, help, file metadata, and option editing.
- P1 - Map canvas readability and performance: keep background rendering, `.xvi` behavior, appearance repaint paths, handles, labels, and bounds stable.
- P1 - Project sidebars: keep Structure indexing and File sidebar mutations reliable, navigable, and refreshed through shared post-operation paths.
- P1 - Architecture maintenance: keep reducing `MainWindow`, `TextEditorTab`, and `MapEditorTab` shell responsibilities without behavior changes.
- P1 - Unified source parser and transaction model: migrate Raw, Blocks, Map, inspector, validation, highlighting, help, and completion onto shared source snapshots and source-range transactions.

## Unified Source DOM Implementation Plan

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
- Verification gates: each phase needs parser/projection, round-trip, source-range, undo/redo, and affected editor regression tests.

## Unified Transaction Model Plan

- Target state: programmatic source mutations enter through a single transaction request contract carrying an undo label, before-text identity, source-range edits or an explicit after-text snapshot, dirty-state policy, projection invalidation policy, selection/cursor restoration policy, and user-visible status messages.
- Target state: source-range edits are the preferred representation for parser/planner-owned mutations; full after-text snapshots remain allowed only for snapshot replay, external bulk replacement, or temporary legacy paths with focused coverage.
- Target state: Raw typing and ordinary text editing keep the embedded editor undo behavior, while remaining Raw commands, Map/visual operations, inspector changes, background source updates, and project/sidebar source mutations use shared transaction services.
- Target state: map visual undo commands and text-source undo commands shall not compete silently; each user-visible map/text operation owns one explicit undo entry, with source replacement and any visual state restoration applied as one semantic transaction.
- Phase 2 - Delegate reduction: reduce remaining public `TextEditorTab` source rewrite delegates toward narrow transaction or planner-facing methods, so controllers do not call broad text-replacement wrappers except through approved snapshot replay helpers.
- Phase 3 - Map command alignment: move remaining map canvas/source commands toward transaction requests while preserving command merging, before/after snapshot replay, draft-item restoration, scene refresh, and selection restore behavior.
- Phase 4 - Remaining adopters: route inspector, background, Raw command, and project/sidebar source mutations through transaction requests or focused adapters, with each migration backed by undo/redo and range-preservation tests.
- Phase 5 - Legacy removal gates: rename or remove broad full-text rewrite APIs after all call sites are either source-edit transactions or clearly documented snapshot replay paths.
- Verification gates: each remaining transaction migration needs focused undo/redo coverage, stale/invalid edit behavior, dirty-state refresh checks, and affected Raw/Map/manual workflow verification when UI state changes.

## Next Up

- P1: Continue Phase 2 delegate reduction by removing remaining direct `TextEditorTab` source rewrite delegate usage from controller-level map flows.
- P1: Continue Phase 3 map command alignment by finalizing remaining non-vertex map source workflows, primarily residual background-metadata transaction paths, with policy-driven restore hooks where applicable.
- P1: Add focused regression coverage for each migrated map/source operation (undo/redo, stale-state rejection, projection refresh, and selection restore behavior).
- P1: Keep `WORKLOG.md` trimmed after each completed slice so active work does not become completed-history notes.

## Risks / Blockers

- Parser/serializer round-trip coverage is still incomplete for full Therion corpus-level confidence.
- Map/text undo arbitration still depends on choosing between map `QUndoStack` and embedded text-editor undo.
- Stylus/Sidecar behavior depends on platform input routing; edge cases remain hardware-dependent.

## Backlog

- Quick-start template/example project with `thconfig`, `.th`, `.th2`, and compile-ready references.
- Replace fixed-delay map selection-restore retry timers with explicit scene-refresh completion/generation callbacks.
- Optional Structure graph view for relationships such as `preview`, `revise`, `join`, `equate`, diagnostics, and station-network edges.
- Compiler-confirmed project-index comparison once lightweight indexing is no longer sufficient.
- Broader Therion corpus regression tests for parsing, serialization, source rewrites, indexing, and map/text synchronization.
- Bounded `.xvi` cache policy for very large projects.
- Station-label declutter: add overlap suppression/priority ranking and optional user-facing `Auto/All/None` label mode.
- Make line guide-spine rendering explicit in style JSON (`guide_spine_visible`) and remove the fallback when catalog coverage allows it.
- Apple Pencil/freehand stroke UX and shape-sensitive simplification polish.
- Additional map-style catalog tuning and SVG-backed symbol evaluation.
