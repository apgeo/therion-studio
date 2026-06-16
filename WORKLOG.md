# Worklog

Active work only. Completed history is archived in `WORKLOG_ARCHIVE_2026-05-13.md` and `WORKLOG_ARCHIVE_2026-05-23.md`. Stable architecture direction is maintained in `ARCHITECTURE.md`.

## In Progress

- P1 - Localization hygiene: keep Czech/Slovak UI catalogs, canonical Therion syntax, and Qt/platform language selection current; all new user-visible UI/status/tooltip/validation text must be translatable and added to the Czech and Slovak catalogs in the same change, except Therion syntax or user-authored source/data snippets.
- P1 - Localization expansion staging: prepare French, German, Italian, Spanish, and Portuguese as hidden staged catalogs until each application catalog and localized user manual passes strict localization checks and can be promoted into Settings/runtime/packaging.
- P1 - Validation and catalog metadata: keep validation conservative while continuing to move command/option/positional argument interpretation into catalog-backed logical-command metadata and live project diagnostics.
- P1 - Blocks editor: preserve document-domain filtering, data-block ownership, no-open source mutations, and single-transaction block-detail edits.
- P1 - Map editor parity: continue XTherion-aligned selection, context menus, panning, Bezier input, Smart Area, TH2 source sync, object-level Geometry controls such as clipping/alignment, robust standalone slope line-point metadata parsing (`l-size`/`orientation`), generated point insertion that does not write artificial station references, embedded Raw-mode command-bar map tools staying visible but inactive, subtle always-visible `altitude`/`subtype`-only vertex metadata rings, viewport-scale snap for dragged line/area vertices and point/line/area insertion with nearby-object candidate vertex guides and active target highlighting, centralized status-bar workflow hints for drawing/editing actions, and the guardrail that project validation diagnostics must not perturb map canvas selection/drag/draw workflows.
- P1 - Inspector consistency: keep Raw, Blocks, and Map inspectors on the shared panel foundation with aligned segmented selectors, help, file metadata, and option editing.
- P1 - Map canvas readability and performance: keep background rendering, `.xvi` behavior, Mapiah `format=xvi`/`format=raster` background import/render/editing, percent-encoded Mapiah filenames, appearance repaint paths, handles, labels, and bounds stable.
- P1 - Project sidebars: keep Structure indexing, File sidebar mutations, Survey/Map projections, expansion state, and post-operation refresh paths reliable while preserving the ownership split between file operations, survey namespace, map composition, and validation.
- P1 - Project templates: keep the bundled default project template compile-ready, including declared export directories, remembered project-parent defaults, and aligned welcome/project-menu/open-tab workflow.
- P1 - Architecture maintenance: keep reducing `MainWindow`, `TextEditorTab`, and `MapEditorTab` shell responsibilities without behavior changes.
- P1 - MainWindow sidebar split: keep extracting remaining sidebar surfaces into focused translation units while keeping `MainWindowSidebar.cpp` focused on structure rail, pane switching, and collapse state.
- P1 - GUI cleanup planning: follow `GUI_CLEANUP.md` to continue separating style policy, UI construction, presentation contracts, and view-technology-ready state/intent boundaries in independently shippable phases.
- P1 - Change discipline: keep commits narrowly scoped with Conventional Commit-style English subjects and body notes for non-trivial architecture or verification context.
- P1 - Release readiness: prepare `v2026.6.4` with local validation, release notes, and CI artifact workflow handoff before tagging and publishing.
- P1 - Structure constraints hygiene: keep ratcheted translation-unit limits green by extracting oversized delegates, running `python3 scripts/check_structure_constraints.py` before commits/PRs, keeping CMake source lists aligned with `src/`, and preserving guardrails against map-editor source mutation bypasses.
- P1 - Test infrastructure hygiene: use QTest for all new C++ tests while keeping CTest as the runner; keep `tests/core/`, the shared `TherionCoreQTests` runner, and converted `TherionTokenRulesTest` as the baseline pattern, migrate existing hand-rolled test files incrementally when touched, and consolidate small test executables only where their dependency and runtime boundaries already match.
- P1 - Unified source parser and transaction model: migrate Raw, Blocks, Map, inspector, validation, highlighting, help, completion, and undo ownership onto shared source snapshots and source-range transactions.
- P1 - Project index relationships: extend the DOM-backed project snapshot with non-hierarchical graph links such as map preview, revise, join, and equate for validator and graph/detail views without mixing them into the Structure sidebar ownership tree.

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

## Next Up

- P1: Continue Unified Source DOM Phase 3/8 by tightening source-file reference resolution, keeping validation centralized in the Validation panel, preserving Structure as navigation/orientation, and ensuring namespace/reference behavior follows `docs/THERION_COMPATIBILITY.md`.
- P1: Continue Phase 9 ownership unification by replacing `MapEditorTab`'s preferred-owner state with durable transaction ownership metadata attached to each source mutation.
- P1: Audit remaining map source mutation flows for ad hoc undo ownership, especially inspector-applied edits and background/metadata source updates, before moving to one document-level undo timeline.
- P1: Expand focused map/text undo regression coverage to include save/dirty-state transitions, detached map panes, and inspector-applied source transactions.

## Risks / Blockers

- Parser/serializer round-trip coverage is still incomplete for full Therion corpus-level confidence during Unified Source DOM phase migrations.
- Map/text undo ownership is still coordinated between map `QUndoStack` and embedded text-editor undo rather than stored on one durable document transaction timeline.
- Stylus/Sidecar behavior still requires hardware-specific manual validation because CI coverage for platform input routing edge cases is limited.

## Backlog

- Replace fixed-delay map selection-restore retry timers with explicit scene-refresh completion/generation callbacks.
- Optional Structure graph view for relationships such as `preview`, `revise`, `join`, `equate`, relationship status, and station-network edges.
- Compiler-confirmed project-index comparison once lightweight indexing is no longer sufficient.
- Retire or demote the manual `Validate Project` action after live project diagnostics are reliable for edits, saves, file operations, project-open, and catalog/source-model refresh events.
- Broader Therion corpus regression tests for parsing, serialization, source rewrites, indexing, and map/text synchronization.
- Add old-project integration fixtures for Therion/Metapost runner failures: loading and compiling mid-2000s-style projects must not crash the application, and `TherionRunnerService`/runner presentation should surface a clear, actionable external-process error.
- Bounded `.xvi` cache policy for very large projects.
- Station-label declutter: add overlap suppression/priority ranking and optional user-facing `Auto/All/None` label mode.
- Make line guide-spine rendering explicit in style JSON (`guide_spine_visible`) and remove the fallback when catalog coverage allows it.
- Apple Pencil/freehand stroke UX and shape-sensitive simplification polish.
- Additional map-style catalog tuning and SVG-backed symbol evaluation.
- Mapiah background editing/export follow-up: keep XTherion metadata for simple placement and upgrade individual layers to Mapiah metadata only when advanced transforms are used; continue hardening undo/redo, Visual/Raw mode switching, persistent selected-layer pivot marker behavior and `Display` controls in the Backgrounds inspector, and interoperability tests around mixed XTherion/Mapiah background headers.
