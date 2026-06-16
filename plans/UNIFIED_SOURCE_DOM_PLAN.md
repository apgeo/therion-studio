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

## Verification Gates

- Parser/projection coverage for each migrated consumer.
- Round-trip coverage for source-sensitive edits.
- Source-range coverage for diagnostics and mutations.
- Undo/redo coverage for user-visible source changes.
- Affected editor regression coverage for Raw, Blocks, Map, Structure, Validation, and compiler-facing workflows.
