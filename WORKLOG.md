# Worklog

Active planning only. Completed history belongs in archive files. Stable architecture belongs in `ARCHITECTURE.md`. Detailed plans live in `plans/`.

## Current Focus

1. Release readiness for `v2026.6.5`.
2. Unified Source DOM and source transaction ownership.
3. Test infrastructure hygiene and structure guardrails.

## Active Work

### Release Readiness

- Run local validation before tagging or packaging handoff.
- Keep release notes, package metadata, and CI artifact workflow aligned with `v2026.6.5`.

### Unified Source DOM / Transactions

- Tighten source-file reference resolution while preserving Therion namespace semantics from `docs/THERION_COMPATIBILITY.md`.
- Continue moving map/text undo ownership toward durable transaction metadata; keep ownership transitions centralized while source mutations still carry local callback labels.
- Expand map/text undo regression coverage for save/dirty-state transitions, detached map panes, and inspector-applied source transactions.
- Continue the phase plan in `plans/UNIFIED_SOURCE_DOM_PLAN.md`.

### Validation And Catalog Metadata

- Keep validation conservative while moving command, option, and positional argument interpretation into catalog-backed logical-command metadata.
- Keep problem reporting centralized in the Validation panel while Structure remains an orientation/navigation view.

### Test And Structure Hygiene

- Use QTest for new C++ tests while keeping CTest as the runner.
- Keep `tests/core/` and `TherionCoreQTests` as the baseline pattern for small core-only QTest cases.
- Migrate hand-rolled tests only when touched or when the migration directly supports current work.
- Keep `python3 scripts/check_structure_constraints.py` green and preserve guardrails against map-editor source mutation bypasses.

### UI Cleanup

- Continue `plans/GUI_CLEANUP.md` in independently shippable slices.
- Keep style policy, UI construction, presentation contracts, and source/model logic separated.

## Blocked / Needs Input

- Old Therion/Metapost crash fixture: keep parked until a reproducible project or minimal fixture is available.
- Stylus/Sidecar behavior: needs hardware-specific manual validation.

## Backlog

- Replace fixed-delay map selection-restore retry timers with explicit scene-refresh completion/generation callbacks.
- Optional Structure graph view for relationships such as `preview`, `revise`, `join`, `equate`, relationship status, and station-network edges.
- Compiler-confirmed project-index comparison once lightweight indexing is no longer sufficient.
- Retire or demote the manual `Validate Project` action after live project diagnostics are reliable for edits, saves, file operations, project-open, and catalog/source-model refresh events.
- Broader Therion corpus regression tests for parsing, serialization, source rewrites, indexing, and map/text synchronization.
- Add old-project integration fixtures for Therion/Metapost runner failures once a fixture exists.
- Bounded `.xvi` cache policy for very large projects.
- Station-label declutter: add overlap suppression/priority ranking and optional user-facing `Auto/All/None` label mode.
- Make line guide-spine rendering explicit in style JSON (`guide_spine_visible`) and remove the fallback when catalog coverage allows it.
- Apple Pencil/freehand stroke UX and shape-sensitive simplification polish.
- Additional map-style catalog tuning and SVG-backed symbol evaluation.
- Mapiah background editing/export follow-up for mixed XTherion/Mapiah metadata, undo/redo, Visual/Raw mode switching, selected-layer pivot marker behavior, and `Display` controls.
