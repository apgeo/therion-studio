# Worklog

Active work only. Completed history is archived in `WORKLOG_ARCHIVE_2026-05-13.md` and `WORKLOG_ARCHIVE_2026-05-23.md`.

## In Progress

- Phase 6: finish encoding QA. Implementation is mostly complete; manual cross-platform encoding validation remains deferred.
- Phase 7: finish UX/accessibility/platform-convention validation, especially shortcut and keyboard parity on macOS, Windows, and Linux.
- Phase 8: start release readiness and packaging work for all supported platforms.
- Phase 9: continue map-editor parity polish after the `Inspector -> Objects` row-drag batch; focus on interaction gaps found during manual validation.
- Phase 10: continue interactive map drawing/insertion polish, especially mode-aware undo/redo semantics where one completed draw gesture maps to one undo step.
- Phase 11: continue structured block-canvas authoring coverage, safer nested insertion behavior, and remaining BlockEditor extraction/refactor slices.

## Next Up

- Run a manual pass of TH2 Visual `Inspector -> Objects` row dragging in the running app after the latest current-location/drop-line fixes.
- Continue Phase 9 with the next map-editor parity issue found during manual testing, including validation that bezier control handles remain draggable when close to other geometry.
- Continue Phase 10 with manual validation of Apple Pencil freehand stroke preview/output, including solid freehand draft preview and less-aggressive shape-sensitive bezier simplification behavior, plus focused GUI smoke coverage for point insertion and line/area draft completion paths.
- Validate that removing the visible `Touch Controls` toolbar button leaves automatic mouse, touchpad, Magic Mouse, pinch, and stylus navigation behavior intact.
- Validate that removing the visible `Smart Trace` toolbar button leaves current point/line/freehand/area drawing workflows intact.
- Continue Phase 11 selection/details glue consolidation without behavior changes, then ratchet structure limits only after full verification passes.

## Risks / Blockers

- Parser/serializer round-trip coverage is still incomplete for full Therion corpus-level confidence.
- Non-UTF decoding is implemented with Qt-supported codecs, but broader legacy-encoding corpus validation is still pending.
- Cross-platform GUI automation remains limited beyond focused smoke tests; manual macOS/Windows/Linux parity passes are still required.
- Pen/touch navigation now depends on automatic input-policy behavior; Sidecar and touch-screen manual validation remains pending.
- Current map/text undo arbitration still depends on choosing between the map `QUndoStack` and embedded text-editor undo until the later unified command-stack refactor.
- Packaging/signing/distribution requirements have not yet been exercised end-to-end on all target platforms.

## Phase Plan

### Phase 6 - Encoding and File-Format Robustness (`MVP`)

- Status: implementation mostly complete; manual QA pending.
- Remaining work: run `docs/ENCODING_QA_CHECKLIST.md` on macOS, Windows, and Linux with representative UTF-8 and legacy-encoded Therion files.
- Verification target: `DocumentFileEncodingTest` plus manual load/edit/save/convert workflows.

### Phase 7 - UX, Accessibility, and Platform Conventions (`MVP` baseline)

- Status: partially complete.
- Remaining work: validate shortcut matrix, menu behavior, focus traversal, high-DPI behavior, dark/light palette transitions, and platform-native expectations on macOS, Windows, and Linux.
- Verification target: manual platform pass plus focused smoke tests where workflows are deterministic enough to automate.

### Phase 8 - Release Readiness and Packaging (`MVP`)

- Status: not started.
- Remaining work: define packaging targets, artifact layout, runtime dependency bundling, signing/notarization expectations, Linux packaging strategy, and representative performance smoke checks.
- Verification target: build and launch release artifacts on macOS, Windows, and Linux.

### Phase 9 - Map Editor Parity Polish (`Post-MVP`)

- Status: in progress.
- Remaining work: continue interaction parity review beyond the completed object-row drag/drop batch, including object inspector ergonomics, command-surface consistency, selection edge cases, bezier-control hit priority near overlapping geometry, and cross-platform input behavior.
- Verification target: `MapEditorDragUndoRedoSmokeTest`, `MapEditorObjectMovePlannerTest`, `MapEditorDetachedPaneTest`, map geometry/background/parser regression suite, and manual parity pass on macOS, Windows, and Linux.

### Phase 10 - Interactive Map Drawing and Insertion (`Post-MVP`)

- Status: in progress.
- Remaining work: harden mode-aware undo/redo semantics for drawing sessions, manually validate Apple Pencil freehand stroke preview/output, solid freehand draft preview, and less-aggressive shape-sensitive bezier simplification, add focused GUI smoke tests for point insertion and line/area draft completion, and keep Smart Trace as a future feature until tracing heuristics are explicitly implemented.
- Verification target: `MapEditorDragUndoRedoSmokeTest`, new drawing workflow smoke tests, and manual authoring pass with mouse, trackpad, and stylus where available.

### Phase 11 - Structured Text Authoring Canvas (`Post-MVP`)

- Status: in progress.
- Remaining work: expand configurable block coverage, improve insertion anchoring and validation feedback for complex nested sources, add focused source rewrite/insertion safety tests, and continue BlockEditor extraction.
- Verification target: `TherionStudio`, `TextEditorCaretInteractionTest`, raw/blocks round-trip checks, and manual `.th` workflows for drag/drop insert, configure, save/reload, and raw-mode inspection.

## Refactor Tracks

- Track A: keep `TextEditorTab` as a thin orchestration shell; continue narrowing raw completion/catalog services and remaining friend/controller boundaries without merging responsibilities back into the tab.
- Track B: continue extracting `MapEditorTab` responsibilities into focused controllers for interactive drawing, inspector objects, inspector backgrounds, selection details, scene lifecycle, and undo/snapshot orchestration.
- Track C: consolidate shared source-edit/rewrite primitives used by Raw, Blocks, and Map modes into focused non-UI services.
- BlockEditor next slice: consolidate selection/details glue (`selectBlockInCanvasAndDetails`, `refreshBlockDetailsSelectionFromScene`, `resolveBlockCanvasItem`, `selectBlockCanvasItem`, and related thin delegates) if the API boundary remains stable.
- Guardrail: each slice should own one responsibility, keep behavior stable, pass build + tests + `StructureConstraintsTest`, and update documentation when user-visible behavior changes.

## Backlog

- Unified document-level undo stack for raw text edits, visual map mutations, inspector setting changes, object/background edits, and structured block changes.
- TH2 Visual grid fallback for documents without parseable geometry or valid source bounds, such as user-defined grid origin/extent.
- Broader GUI automation for structure, inspector, runner, map workflows, and cross-platform keyboard/shortcut matrix coverage.
- Expanded rewrite corpus/regression coverage beyond the MVP fixture set.
- Extend `input` relative-path autocomplete semantics to other path-taking Therion commands/options, such as `-sketch`.
- Completion ranking polish for complex contexts so highest-confidence context-valid tokens stay first and low-signal fallback entries are reduced.
- Future Smart Trace implementation with real trace detection, preview, bezier simplification, and one undo step per accepted trace.

## Manual QA Matrix

- Encoding checklist on macOS: pending.
- Encoding checklist on Windows: pending.
- Encoding checklist on Linux: pending.
- Shortcut/platform UX pass on macOS: pending.
- Shortcut/platform UX pass on Windows: pending.
- Shortcut/platform UX pass on Linux: pending.
- Apple Pencil freehand stroke preview/output and simplification pass: pending.
- Release artifact launch on macOS: pending.
- Release artifact launch on Windows: pending.
- Release artifact launch on Linux: pending.

## Verification Baseline

Use the relevant subset for focused slices, and run the full baseline before release or large refactor closeout.

- `cmake --build build --target TherionStudio`
- `cmake --build build-strict --target TherionStudio`
- `ctest --test-dir build --output-on-failure`
- `ctest --test-dir build-strict --output-on-failure`
- `python3 scripts/check_structure_constraints.py`
- `git diff --check`
