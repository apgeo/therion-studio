# Worklog (Phased Reimplementation)

This worklog tracks Therion Studio reimplementation progress against `QtReimplementationSpecification.md` using implementation phases.

Detailed chronological history has been preserved in `WORKLOG_ARCHIVE_2026-05-13.md`.

## Current Focus

### In progress

- Phase 5: map editor interaction parity and command-surface completion.

### Next up

- Start Phase 6 non-UTF-8 loading/conversion workflow and encoding persistence.
- Continue Phase 5 map-editor parity polish and interaction refinement.

### Risks / blockers

- Parser/serializer round-trip coverage is still incomplete for full Therion corpus-level confidence.
- Text pipeline is still UTF-8-only in practice; non-UTF-8 conversion UX is not implemented yet.
- Most map behaviors are covered by unit/smoke checks; dedicated GUI automation is still missing.

## Phase Plan

Legend:

- `MVP`: required for first cross-platform release.
- `Post-MVP`: enhancement after MVP acceptance criteria are met.

### ~~Phase 0 - Foundation and Build System (`MVP`)~~

- ~~Spec scope: Sections 1, 2, 5, 6.~~
- ~~Goal: app skeleton, build tooling, app startup/shutdown stability.~~
- ~~Status: Completed.~~
- ~~Verification:~~
- ~~`cmake --build build`~~
- ~~Manual launch sanity on desktop target.~~

### ~~Phase 1 - Project Browser, Tabs, Session Lifecycle (`MVP`)~~

- ~~Spec scope: 3.1, 3.7, 3.8.1, 3.10.~~
- ~~Goal: open project, open/close tabs, dirty-state prompts, session restore.~~
- ~~Status: Completed (including `Close` and `Close All Tabs`).~~
- ~~Verification:~~
- ~~`cmake --build build`~~
- ~~Manual workflows: open project, close/save/discard prompts, reopen session.~~

### ~~Phase 2 - Text Editor Core (`MVP`, with Phase 6 dependency)~~

- ~~Spec scope: 3.2, 3.8.2, 3.9 (find/replace-related commands).~~
- ~~Goal: syntax highlighting, inline find/replace, contextual help panel, status strip.~~
- ~~Status: Completed for UTF-8 documents; encoding-conversion sub-scope deferred to Phase 6.~~
- ~~Verification:~~
- ~~`cmake --build build`~~
- ~~Manual workflows: find/replace, help-panel collapse/expand, document status display.~~

### ~~Phase 3 - Structure/Inspector and Therion Runner (`MVP`)~~

- ~~Spec scope: 3.4, 3.5, 3.6, 3.8.5, 3.8.6, 3.8.7.~~
- ~~Goal: parser-backed structure tree, inspector editing/writeback, async Therion runner.~~
- ~~Status: Completed baseline; iterative refinement ongoing.~~
- ~~Verification:~~
- ~~`./build/TherionProjectStructureIndexTest`~~
- ~~`cmake --build build`~~
- ~~Manual workflows: selection sync, inspector rename/apply, Therion run/copy output.~~

### ~~Phase 4 - TH2 Source Editing Engine and Data Integrity (`MVP`)~~

- ~~Spec scope: 4, 4.1, 3.11, 3.11.1.~~
- ~~Goal: safe source rewrites for point/line/area geometry and draft insertion with robust token handling.~~
- ~~Status: Completed (`MVP` acceptance scope).~~
- ~~Implemented highlights:~~
- ~~`appendScrapBlock`, `appendDraftGeometry`, `rewritePointCoordinates`, `rewriteLineAreaVertex`.~~
- ~~Extensive token-selection hardening: quoted tokens, option-led continuation lines, mixed metadata, CRLF preservation, scientific notation.~~
- ~~Corpus-style positive/negative rewrite fixtures: targeted coordinate rewrites, malformed block failures, and no-mutation guarantees on failure paths.~~
- ~~Verification:~~
- ~~`./build/TherionDocumentEditorTest`~~
- ~~`./build/TherionProjectStructureIndexTest`~~
- ~~`./build/MapBackgroundPlacementTest`~~
- ~~`./build/TherionBackgroundMetadataTest`~~
- ~~`./build/TherionXviParserTest`~~
- ~~`cmake --build build --target TherionDocumentEditorTest TherionStudio`~~

### Phase 5 - TH2 Map Workspace and Background Alignment (`MVP` core, `Post-MVP` parity polish)

- Spec scope: 3.3, 3.8.3, 3.11.1.
- Goal: split/text/map workspace, geometry rendering/editing, background-layer management, fit/pan/zoom semantics.
- Status: In progress (core implemented; feature parity refinement continues).
- Implemented highlights:
- ~~Embedded map workspace modes and map help panel.~~
- ~~Point and line/area vertex drag-to-source writeback.~~
- ~~Undo/redo command integration with merge semantics and obsolete-command handling for failures.~~
- ~~Raster and `.xvi` background parsing/placement/alignment; multi-layer controls and session restore.~~
- Verification:
- `./build/MapBackgroundPlacementTest`
- `./build/TherionBackgroundMetadataTest`
- `./build/TherionXviParserTest`
- `./build/TherionDocumentEditorTest`
- `cmake --build build`

### Phase 6 - Encoding and File-Format Robustness (`MVP`)

- Spec scope: 3.2 (encoding conversion), 4 (encoding preservation).
- Goal: detect/load non-UTF-8 inputs, explicit UTF-8 conversion action, preserve original encoding until conversion.
- Status: Not started.
- Planned verification:
- New unit tests for encoding detection and conversion paths.
- Manual workflows with sample non-UTF-8 Therion files.

### Phase 7 - UX/Accessibility/Platform Conventions (`MVP` baseline)

- Spec scope: 3.9, 3.13.1, 5.
- Goal: keyboard command parity, panel ergonomics, platform-native behavior checks.
- Status: Partially completed.
- Implemented highlights:
- Map command shortcut, close-all-tabs shortcut, collapsible sidebar/console/help panes.
- Activity-rail sidebar and console integration.
- Planned verification:
- Manual shortcut matrix pass on macOS/Windows/Linux.

### Phase 8 - Release Readiness and Packaging (`MVP`)

- Spec scope: 5.1, 5.2.
- Goal: packaging/signing/distribution readiness and performance verification pass.
- Status: Not started.
- Planned verification:
- Release artifact checks per platform.
- Performance smoke pass on representative project sizes.

## Post-MVP Backlog (Phase-Linked)

- Phase 3/7: broader GUI automation for structure/inspector/runner/map workflows.
- Phase 5: map-editor parity polish beyond MVP command/interaction coverage.
- Phase 4: expand rewrite corpus/regression coverage beyond MVP fixture set for higher corpus-level confidence.
- Phase 8: extended performance benchmarking and packaging convenience improvements beyond baseline release criteria.

## Test Inventory (Current)

Automated tests currently in-tree and used as regression baseline:

- `TherionDocumentEditorTest`
- `TherionProjectStructureIndexTest`
- `MapBackgroundPlacementTest`
- `TherionBackgroundMetadataTest`
- `TherionXviParserTest`

## Recent Completed (Latest Slice)

### 2026-05-14

- ~~Improved map-canvas interaction affordance for editable geometry handles: added hover/selected/drag visual states, stronger contrast, and per-handle tooltips (point and line/area vertices).~~
- ~~Increased map preview handle and stroke readability (larger point/vertex radii and thicker preview line strokes) to improve selection/drag usability at normal fit scale.~~
- ~~Added hover-state visual feedback to map cards and draft-geometry cards for clearer active-target discovery.~~
- ~~Improved map zoom/fit consistency: fit mode now auto-refits on map viewport resize and keeps geometry-only vs fit-with-background mode semantics.~~
- ~~Synced zoom indicator to actual `QGraphicsView` transform scale after fit/manual zoom operations instead of using a stale fixed baseline.~~
- ~~Added middle-mouse panning for map navigation without changing left-drag behavior for selectable/movable map items.~~
- ~~Refined map input-device controls by source type: standard wheel defaults to cursor-centered zoom, precise scrolling devices default to pan, `Cmd/Ctrl` + scroll forces zoom, and right-button drag pans the map viewport.~~
- ~~Added native pinch (`QNativeGestureEvent::ZoomNativeGesture`) cursor-centered zoom handling for touch surfaces.~~
- ~~Added gesture-suppression guardrails so wheel/pinch/touchpan viewport gestures are ignored while a primary pointer interaction is active, reducing accidental pan/zoom during direct geometry edits.~~
- ~~Added select-mode-only two-touch threshold pan handling in the map view event filter to support touchpad touch-tracking panning without interfering with draw/edit modes.~~
- ~~Expanded zoom clamping and normalization to `0.1 .. 50.0` with viewport-anchored zoom application so toolbar zoom actions and gesture zoom use the same bounded semantics.~~
- ~~Implemented line-feature segment parsing for TH2 map preview so `line ... endline` coordinate data is interpreted as linear (`x y`) and cubic Bezier (`c1x c1y c2x c2y x y`) segments instead of a forced polyline.~~
- ~~Updated map-canvas line rendering to draw parsed cubic arcs via `QPainterPath::cubicTo`, preserving straight segments where appropriate and keeping existing vertex-handle editing.~~
- ~~Stopped treating line-block metadata continuations (for example `smooth off` and option-led lines) as drawable geometry coordinates in map feature extraction.~~
- ~~Added `MapGeometryFeatureParsingTest` coverage for Bezier segment extraction and metadata-line exclusion, and wired the new test target into CMake.~~
- ~~Introduced an explicit TH2 line-vertex model in map geometry features (`anchor`, optional `incomingControl`/`outgoingControl`, `isSmooth`, and source-vertex index mapping) to align map parsing/rendering with Therion curve semantics.~~
- ~~Updated TH2 line parsing so `smooth off` marks the current/last parsed line vertex as non-smooth, and cubic control points equal to anchor coordinates are normalized back to nil handles in memory.~~
- ~~Switched map-canvas line rendering to the line-vertex handle rule: segment is straight when both handles are absent, cubic when either adjacent handle exists (`QPainterPath::cubicTo` with anchor fallback).~~
- ~~Added editable control-handle visualization for parsed line curves (incoming/outgoing control points plus anchor-to-handle guide lines), wired to existing coordinate rewrite callbacks through source vertex indices.~~
- ~~Expanded `MapGeometryFeatureParsingTest` with smooth-state and anchor-equivalent handle-normalization assertions for TH2 curve parsing behavior.~~
- ~~Normalized map-editor line-handle writeback so control-handle drags use `line` rewrite semantics (instead of unsupported `line control` kind), removing a persistence gap for control-point edits.~~
- ~~Added smooth-coupled line-handle update behavior in map writeback commands: when a smooth vertex handle is moved, the opposite handle is auto-updated collinearly with preserved/opportunistic mirrored length and committed in the same undoable command.~~
- ~~Added anchor-drag handle transport in line writeback commands: moving an anchor now translates attached incoming/outgoing controls by the same delta and persists all affected coordinates as one undoable command.~~
- ~~Added a corpus-style `TherionDocumentEditorTest` fixture that performs coordinated point/line/area rewrites within a realistic TH2 scrap block containing metadata, comments, and CRLF line endings.~~
- ~~Verified rewrite stability in the fixture across option-led continuation lines (`-subtype`), inline metadata (`-id`), `%` comments, and mixed-precision coordinate tokens.~~
- ~~Added negative corpus-style fixture checks that assert rewrite failures (missing `endline`, incomplete area coordinate tuple) return errors and leave source text unchanged.~~
- ~~Verified `TherionDocumentEditorTest`, `TherionProjectStructureIndexTest`, `MapBackgroundPlacementTest`, `TherionBackgroundMetadataTest`, and `TherionXviParserTest`.~~
- ~~Verified `cmake --build build --target TherionStudio TherionDocumentEditorTest TherionProjectStructureIndexTest MapBackgroundPlacementTest TherionBackgroundMetadataTest TherionXviParserTest` plus execution of all five regression binaries after map input-control changes.~~
- ~~Verified `cmake --build build --target TherionStudio MapGeometryFeatureParsingTest TherionDocumentEditorTest TherionProjectStructureIndexTest MapBackgroundPlacementTest TherionBackgroundMetadataTest TherionXviParserTest` plus execution of all six regression binaries.~~
- ~~Closed Phase 4 (`MVP`) and moved additional corpus-scale rewrite expansion into the Post-MVP backlog.~~

### 2026-05-13

- ~~Added `File -> Close` and `File -> Close All Tabs` commands with unsaved-change prompts.~~
- ~~Added `Command/Ctrl+Shift+W` shortcut for `Close All Tabs`.~~
- ~~Persisted open-document session state immediately after tab-close operations.~~
- ~~Verified build and full current regression baseline.~~
- ~~Added project-tree context menu actions for file rows: `Open`, `Open in Map Editor` (for `.th2`), and `Open Externally` for unsupported file types.~~
- ~~Wired `Open Externally` through platform default-app launch with actionable failure feedback.~~
- ~~Verified `cmake --build build --target TherionStudio TherionDocumentEditorTest` plus current regression binaries (`TherionDocumentEditorTest`, `TherionProjectStructureIndexTest`, `MapBackgroundPlacementTest`, `TherionBackgroundMetadataTest`, `TherionXviParserTest`).~~
- ~~Extended project-tree context menus with file/folder management actions: `Duplicate`, `Rename`, `Delete`, `Rename Folder`, and `Delete Folder`.~~
- ~~Added project-tree creation actions in directory/root context: `New Folder`, `New .th File`, `New .th2 File`, and `New thconfig`.~~
- ~~Added safety guardrails for destructive file/folder operations by blocking rename/delete when affected documents are currently open in editor tabs.~~
- ~~Verified `cmake --build build --target TherionStudio TherionDocumentEditorTest` plus current regression binaries (`TherionDocumentEditorTest`, `TherionProjectStructureIndexTest`, `MapBackgroundPlacementTest`, `TherionBackgroundMetadataTest`, `TherionXviParserTest`).~~
- ~~Added project-tree icon differentiation so supported Therion files (`.th`, `.th2`, `thconfig`) use a distinct Therion-document icon while unsupported files use the generic document icon.~~
- ~~Refined project-tree icon handling so non-Therion files keep their native filetype icons while Therion files get a guaranteed `TH`-badged icon (fix for "all icons look the same").~~
- ~~Extended Therion icon classification to include all `*.thconfig` files (plus `thconfig`, `.th`, and `.th2`).~~
- ~~Expanded `TherionDocumentEditorTest` rewrite coverage for mixed `%` comments, CRLF preservation, quoted numeric noise, and option-heavy multi-line line/area vertex blocks.~~
- ~~Verified `TherionDocumentEditorTest`, `TherionProjectStructureIndexTest`, `MapBackgroundPlacementTest`, `TherionBackgroundMetadataTest`, and `TherionXviParserTest` after rewrite-coverage expansion.~~
- ~~Verified `cmake --build build --target TherionStudio TherionDocumentEditorTest` plus current regression binaries (`TherionDocumentEditorTest`, `TherionProjectStructureIndexTest`, `MapBackgroundPlacementTest`, `TherionBackgroundMetadataTest`, `TherionXviParserTest`).~~
