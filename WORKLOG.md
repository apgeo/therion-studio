# Worklog

Active work only. Completed history is archived in `WORKLOG_ARCHIVE_2026-05-13.md` and `WORKLOG_ARCHIVE_2026-05-23.md`.

## In Progress

- Phase 6: finalize encoding robustness, with focus on corpus hardening and edge-case handling.
- Phase 7: finalize UX/accessibility/platform-convention parity, with focus on shortcuts and keyboard behavior consistency on macOS, Windows, and Linux.
- Phase 8: continue release-readiness and packaging work across supported platforms, including distribution strategy, signing/notarization expectations, and reproducible release artifacts.
- Phase 9: continue map-editor parity polish, focusing on remaining interaction gaps, inspector ergonomics, and selection/ownership edge cases.
- Phase 10: continue interactive drawing/insertion polish, focusing on Apple Pencil/freehand performance and remaining insertion edge cases.
- Phase 11: continue structured block-canvas authoring coverage and BlockEditor extraction/refactor slices.

## Next Up

- Continue Phase 9 with line-point option parity, object-inspector ergonomics, and remaining area/line ownership edge cases.
- Continue Phase 10 with Apple Pencil/freehand stroke UX, broader parsed-document snapshot use, less-aggressive shape-sensitive bezier simplification, and point/line/area workflow polish.
- Continue Phase 11 with object-reference target picking/resolution shortcuts, then selection/details glue consolidation without behavior changes.
- Finalize and exercise a Linux release-packaging path (`.deb` first candidate) aligned with distro Qt builds.

## Risks / Blockers

- Parser/serializer round-trip coverage is still incomplete for full Therion corpus-level confidence.
- Non-UTF decoding relies on Qt-supported codecs; broader legacy-encoding corpus support may still expose edge cases.
- Pen/touch navigation depends on automatic input-policy behavior; Sidecar and touch-screen edge cases remain a product risk.
- Map/text undo arbitration still depends on choosing between map `QUndoStack` and embedded text-editor undo until unified command-stack refactor.
- Packaging/signing/distribution requirements are not yet exercised end-to-end on all target platforms.

## Phase Plan

### Phase 6 - Encoding and File-Format Robustness (`MVP`)

- Status: implementation mostly complete; corpus hardening pending.
- Remaining work: improve handling for representative UTF-8 and legacy-encoded Therion files.

### Phase 7 - UX, Accessibility, and Platform Conventions (`MVP` baseline)

- Status: partially complete.
- Remaining work: improve shortcut matrix, menu behavior, focus traversal, high-DPI behavior, dark/light palette transitions, and platform-native expectations.

### Phase 8 - Release Readiness and Packaging (`MVP`)

- Status: in progress.
- Remaining work: define final packaging targets, release artifact layout, signing/notarization expectations, Linux packaging strategy, and end-to-end release verification.

### Phase 9 - Map Editor Parity Polish (`Post-MVP`)

- Status: in progress.
- Remaining work: continue interaction parity review for inspector ergonomics, command-surface consistency, selection edge cases, and cross-platform input behavior.
- Open design item: define area-border rewrite semantics for `Split Here` so referenced areas can be updated to include both resulting border lines.

### Phase 10 - Interactive Map Drawing and Insertion (`Post-MVP`)

- Status: in progress.
- Remaining work: improve Apple Pencil/freehand stroke preview/output, solid freehand draft preview, less-aggressive shape-sensitive bezier simplification, and remaining insertion edge cases.

### Phase 11 - Structured Text Authoring Canvas (`Post-MVP`)

- Status: in progress.
- Remaining work: expand configurable block coverage, add target-picking/navigation affordances for map object references, improve insertion anchoring/source-safety feedback, and continue BlockEditor extraction.

## Refactor Tracks

- Track A: keep `TextEditorTab` as a thin orchestration shell; continue narrowing raw completion/catalog services and remaining friend/controller boundaries.
- Track B: continue extracting `MapEditorTab` responsibilities into focused controllers for drawing, inspectors, selection details, scene lifecycle, and undo/snapshot orchestration.
- Track C: consolidate shared source-edit/rewrite primitives used by Raw, Blocks, and Map modes into focused non-UI services.
- BlockEditor next slice: consolidate selection/details glue (`selectBlockInCanvasAndDetails`, `refreshBlockDetailsSelectionFromScene`, `resolveBlockCanvasItem`, `selectBlockCanvasItem`, and related thin delegates) if API boundaries remain stable.
- Guardrail: each slice should own one responsibility, keep behavior stable, and update documentation when user-visible behavior changes.

## Backlog

- Unified document-level undo stack for raw text edits, visual map mutations, inspector setting changes, object/background edits, and structured block changes.
- TH2 Visual grid fallback for documents without parseable geometry or valid source bounds, such as user-defined grid origin/extent.
- Broader GUI automation for structure, inspector, runner, map workflows, and cross-platform keyboard/shortcut matrix coverage.
- Expanded rewrite corpus/regression coverage beyond the MVP fixture set.
- Extend `input` relative-path autocomplete semantics to other path-taking Therion commands/options, such as `-sketch`.
- Completion ranking polish for complex contexts so highest-confidence context-valid tokens stay first and low-signal fallback entries are reduced.
- Future Smart Trace implementation with real trace detection, preview, bezier simplification, and one undo step per accepted trace.
