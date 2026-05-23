# Worklog

Active work only. Completed history is archived in `WORKLOG_ARCHIVE_2026-05-13.md` and `WORKLOG_ARCHIVE_2026-05-23.md`.

## In Progress

- Phase 6: finish encoding robustness work. Implementation is mostly complete; remaining work is corpus hardening and edge-case handling.
- Phase 7: finish UX/accessibility/platform-convention work, especially shortcut and keyboard parity on macOS, Windows, and Linux.
- Phase 8: start release readiness and packaging work for all supported platforms.
- Phase 9: continue map-editor parity polish after the `Inspector -> Objects` row-drag/delete-rule/area-boundary-selection/reference-disclosure batch, selected-object explicit-orientation handle/checkbox-selection-preservation gating, single-table parsed-attribute object settings with object-level help, Selection inspector geometry-before-vertex ordering, explicit `Object Actions` and contextual `Point Details`/`Line Point` section naming, high-DPI sidebar rail icon rendering, compact compiler sidebar layout with clearable output and status-bar-only runner state, compiler output restricted to Therion process streams, stable auto-detected Therion executable path, compiler run-target selector for current/project configs with config tabs auto-current, non-config tabs locked to project, divider-separated rail project compile action, current-config toolbar compile action, and toggleable status-bar compiler state/result indicator, automatic selected-config-folder working directory with project-only override, browse, reset, and subdued inline resolved path summaries, runner helper-tool PATH augmentation, monitor-x return-map icon, XTherion-style selected-line-vertex `<<`/smooth/`>>` control group, supported line-point option preservation during Bezier conversion, logical-vertex reselection after line-point rewrites, immediate orientation/l-size inspector updates without an apply button, contextual selected-vertex insert/interactive-extend actions, safe selected-line split for non-area-border interior vertices, and source-derived orientation applicability for point types; focus on interaction gaps found during use.
- Phase 10: continue interactive map drawing/insertion polish, especially mode-aware undo/redo semantics where one completed draw gesture maps to one undo step.
- Phase 11: continue structured block-canvas authoring coverage, safer nested insertion behavior, and remaining BlockEditor extraction/refactor slices.

## Next Up

- Continue Phase 9 with the next map-editor parity issue found during use, including line-point option parity, object inspector ergonomics, and any remaining area/line ownership edge cases.
- Continue Phase 10 with Apple Pencil/freehand stroke UX, solid freehand draft preview, less-aggressive shape-sensitive bezier simplification, and point/line/area drawing workflow polish.
- Continue Phase 11 selection/details glue consolidation without behavior changes, then ratchet structure limits after the slice is stable.

## Risks / Blockers

- Parser/serializer round-trip coverage is still incomplete for full Therion corpus-level confidence.
- Non-UTF decoding is implemented with Qt-supported codecs, but broader legacy-encoding corpus support may still expose edge cases.
- Pen/touch navigation now depends on automatic input-policy behavior; Sidecar and touch-screen edge cases remain a product risk.
- Current map/text undo arbitration still depends on choosing between the map `QUndoStack` and embedded text-editor undo until the later unified command-stack refactor.
- Packaging/signing/distribution requirements have not yet been exercised end-to-end on all target platforms.

## Phase Plan

### Phase 6 - Encoding and File-Format Robustness (`MVP`)

- Status: implementation mostly complete; corpus hardening pending.
- Remaining work: improve handling for representative UTF-8 and legacy-encoded Therion files.

### Phase 7 - UX, Accessibility, and Platform Conventions (`MVP` baseline)

- Status: partially complete.
- Remaining work: improve shortcut matrix, menu behavior, focus traversal, high-DPI behavior, dark/light palette transitions, and platform-native expectations on macOS, Windows, and Linux.

### Phase 8 - Release Readiness and Packaging (`MVP`)

- Status: not started.
- Remaining work: define packaging targets, artifact layout, runtime dependency bundling, signing/notarization expectations, Linux packaging strategy, and representative performance expectations.
- Remaining packaging scope includes producing launchable artifacts on macOS, Windows, and Linux.

### Phase 9 - Map Editor Parity Polish (`Post-MVP`)

- Status: in progress.
- Remaining work: continue interaction parity review beyond the completed object-row drag/drop/delete-rule/area-boundary-selection/reference-disclosure/explicit-orientation handle, checkbox-selection-preservation, line-anchor seed, single-table parsed-attribute object settings with object-level help, Selection inspector section-order/`Object Actions`/`Point Details`/`Line Point` naming batch, high-DPI sidebar rail icon rendering, compact compiler sidebar layout with clearable output and status-bar-only runner state, compiler output restricted to Therion process streams, stable auto-detected Therion executable path, compiler run-target selector for current/project configs with config tabs auto-current, non-config tabs locked to project, divider-separated rail project compile action, current-config toolbar compile action, and toggleable status-bar compiler state/result indicator, automatic selected-config-folder working directory with project-only override, browse, reset, and subdued inline resolved path summaries, runner helper-tool PATH augmentation, monitor-x return-map icon, XTherion-style selected-line-vertex `<<`/smooth/`>>` control group, supported line-point option preservation during Bezier conversion, logical-vertex reselection after line-point rewrites, immediate orientation/l-size inspector updates without an apply button, contextual selected-vertex insert/interactive-extend actions, safe selected-line split for non-area-border interior vertices, and source-derived orientation applicability for point types, including object inspector ergonomics, command-surface consistency, selection edge cases, and cross-platform input behavior.
- Define area-border rewrite semantics for `Split Here` so referenced areas can eventually be updated to include both resulting border lines.

### Phase 10 - Interactive Map Drawing and Insertion (`Post-MVP`)

- Status: in progress.
- Remaining work: harden mode-aware undo/redo semantics for drawing sessions, improve Apple Pencil freehand stroke preview/output, solid freehand draft preview, less-aggressive shape-sensitive bezier simplification, point/line/area draft completion, and keep Smart Trace as a future feature until tracing heuristics are explicitly implemented.

### Phase 11 - Structured Text Authoring Canvas (`Post-MVP`)

- Status: in progress.
- Remaining work: expand configurable block coverage, improve insertion anchoring and source-safety feedback for complex nested sources, and continue BlockEditor extraction.

## Refactor Tracks

- Track A: keep `TextEditorTab` as a thin orchestration shell; continue narrowing raw completion/catalog services and remaining friend/controller boundaries without merging responsibilities back into the tab.
- Track B: continue extracting `MapEditorTab` responsibilities into focused controllers for interactive drawing, inspector objects, inspector backgrounds, selection details, scene lifecycle, and undo/snapshot orchestration.
- Track C: consolidate shared source-edit/rewrite primitives used by Raw, Blocks, and Map modes into focused non-UI services.
- BlockEditor next slice: consolidate selection/details glue (`selectBlockInCanvasAndDetails`, `refreshBlockDetailsSelectionFromScene`, `resolveBlockCanvasItem`, `selectBlockCanvasItem`, and related thin delegates) if the API boundary remains stable.
- Guardrail: each slice should own one responsibility, keep behavior stable, and update documentation when user-visible behavior changes.

## Backlog

- Unified document-level undo stack for raw text edits, visual map mutations, inspector setting changes, object/background edits, and structured block changes.
- TH2 Visual grid fallback for documents without parseable geometry or valid source bounds, such as user-defined grid origin/extent.
- Broader GUI automation for structure, inspector, runner, map workflows, and cross-platform keyboard/shortcut matrix coverage.
- Expanded rewrite corpus/regression coverage beyond the MVP fixture set.
- Extend `input` relative-path autocomplete semantics to other path-taking Therion commands/options, such as `-sketch`.
- Completion ranking polish for complex contexts so highest-confidence context-valid tokens stay first and low-signal fallback entries are reduced.
- Future Smart Trace implementation with real trace detection, preview, bezier simplification, and one undo step per accepted trace.
