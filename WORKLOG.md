# Worklog (Phased Reimplementation)

This worklog tracks Therion Studio reimplementation progress against `QtReimplementationSpecification.md` using implementation phases.

Detailed chronological history has been preserved in `WORKLOG_ARCHIVE_2026-05-13.md`.

## Current Focus

### In progress

- Phase 5: map editor interaction parity and command-surface completion.
- Phase 6: encoding detection/conversion and encoding-preserving save semantics.

### Next up

- Continue Phase 5 map-editor parity polish and interaction refinement.
- Continue implementation work; defer Phase 6 manual verification pass.

### Risks / blockers

- Parser/serializer round-trip coverage is still incomplete for full Therion corpus-level confidence.
- Non-UTF decoding is now implemented with Qt-supported codecs, but broader legacy-encoding coverage still needs corpus validation.
- Most map behaviors are covered by unit/smoke checks; dedicated GUI automation is still missing.

### Manual QA Runs

- Encoding checklist pass (`docs/ENCODING_QA_CHECKLIST.md`):
- macOS: Deferred
- Windows: Deferred
- Linux: Deferred

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
- ~~Added explicit touch-friendly controls mode in the map toolbar, persisted via session settings, and wired into mode-aware wheel/touch-pan input policy.~~
- Verification:
- ~~`./build/MapBackgroundPlacementTest`~~
- ~~`./build/TherionBackgroundMetadataTest`~~
- ~~`./build/TherionXviParserTest`~~
- ~~`./build/TherionDocumentEditorTest`~~
- ~~`cmake --build build`~~

### Phase 6 - Encoding and File-Format Robustness (`MVP`)

- Spec scope: 3.2 (encoding conversion), 4 (encoding preservation).
- Goal: detect/load non-UTF-8 inputs, explicit UTF-8 conversion action, preserve original encoding until conversion.
- Status: In progress.
- Implemented highlights:
- ~~Encoding-aware `DocumentFile` read/write path with detection and codec-preserving save semantics.~~
- ~~Therion `encoding ...` directive-aware decode path with codec-name preservation for save.~~
- ~~Text editor status row now exposes explicit `Convert to UTF-8` action for non-UTF documents.~~
- ~~Text editor conversion flow now requires confirmation and surfaces explicit encoding-state notes for save behavior.~~
- ~~Added `DocumentFileEncodingTest` regression coverage for UTF-8 detection plus Latin1, Windows-1250, `cp1250` alias, and Windows-1252 byte-preserving save round-trips.~~
- ~~Added inspector-fallback encoding-preservation regression coverage to `DocumentFileEncodingTest`: non-UTF (`cp1250`) read-detect-rewrite-write path keeps source encoding while applying structure rename and line-option rewrite.~~
- ~~Added complementary inspector-fallback coverage for `windows-1252` so the same read-detect-rewrite-write encoding-preservation path is validated across two legacy single-byte codec families.~~
- ~~Added negative `DocumentFileEncodingTest` coverage for unknown `encoding ...` directive tokens, verifying UTF-8 fallback decode and byte-preserving save behavior.~~
- ~~Expanded `DocumentFileEncodingTest` non-UTF fixture coverage with `cp1252` alias resolution, `latin2` directive decoding (`iso-8859-2`), and unknown-directive Latin1 fallback byte-preserving round-trip checks.~~
- Planned verification:
- ~~New unit tests for encoding detection and conversion paths.~~
- Manual workflows with sample non-UTF-8 Therion files (tracked in `docs/ENCODING_QA_CHECKLIST.md`).

### Phase 7 - UX/Accessibility/Platform Conventions (`MVP` baseline)

- Spec scope: 3.9, 3.13.1, 5.
- Goal: keyboard command parity, panel ergonomics, platform-native behavior checks.
- Status: Partially completed.
- Implemented highlights:
- ~~Map command shortcut, close-all-tabs shortcut, collapsible sidebar/console/help panes.~~
- ~~Activity-rail sidebar and console integration.~~
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
- `DocumentFileEncodingTest`
- `MapEditorInputPolicyTest`
- `SessionStoreTest`

## Recent Completed (Latest Slice)

### 2026-05-14

- ~~Started Phase 6 encoding workflow: added encoding-aware `DocumentFile` read/write APIs with detection (`UTF-8`/`UTF-16*`/`System`/`Latin1`) and codec-preserving save behavior.~~
- ~~Extended Phase 6 encoding workflow with Therion `encoding ...` directive-aware decoding and codec-name preserving save semantics (including legacy single-byte codecs when supported by Qt).~~
- ~~Added explicit text-editor `Convert to UTF-8` action (visible for non-UTF documents) and wired saves to preserve original encoding until conversion.~~
- ~~Polished conversion UX: `Convert to UTF-8` now requires confirmation, marks conversion as an in-memory pending change, and shows explicit encoding-status notes about save behavior.~~
- ~~Expanded `DocumentFileEncodingTest` and verified UTF-8 detection plus Latin1 and Windows-1250 byte-preserving save round-trip behavior.~~
- ~~Expanded encoding regression fixtures with `cp1250` alias and Windows-1252 directive coverage, verifying byte-preserving round-trip saves for both paths.~~
- ~~Closed inspector encoding-preservation gap for unopened-source fallback edits: structure rename and line `-close`/`-reverse` rewrites now read and write using detected file encoding instead of forcing UTF-8.~~
- ~~Added `DocumentFileEncodingTest` regression for inspector-style fallback rewrites on `cp1250` input (`readTextFile` + `rewrite...` + `writeTextFile`), verifying byte-level encoded output after map-name rename and line `-close` toggle edits.~~
- ~~Added `DocumentFileEncodingTest` regression for inspector-style fallback rewrites on `windows-1252` input with accented map-name rename and line `-close` toggle, verifying byte-level encoded output preservation.~~
- ~~Added negative `DocumentFileEncodingTest` case for unknown `encoding` directive token fallback, confirming UTF-8 decode fallback and byte-preserving UTF-8 save round-trip.~~
- ~~Expanded `DocumentFileEncodingTest` with broader non-UTF fixture coverage: `cp1252` alias round-trip, `latin2` directive round-trip, and unknown-directive Latin1 fallback round-trip with byte-preserving save verification.~~
- ~~Re-verified Phase 5 regression suite after ongoing map/editor work: built `TherionStudio`, `MapGeometryFeatureParsingTest`, `MapBackgroundPlacementTest`, `TherionBackgroundMetadataTest`, `TherionXviParserTest`, and `TherionDocumentEditorTest`, then executed all five map-related binaries plus `TherionDocumentEditorTest`.~~
- ~~Implemented explicit touch-friendly controls mode for map editor workflows: added toolbar toggle, persisted state in `SessionStore`, and routed wheel/touch-pan behavior through tested input policy helpers.~~
- ~~Added `MapEditorInputPolicyTest` and `SessionStoreTest` coverage for mode-aware pan/zoom decision logic and touch-controls persistence round-trip behavior.~~
- ~~Added `docs/ENCODING_QA_CHECKLIST.md` and linked it from the user manual to standardize cross-platform manual validation for open/save/convert/reopen/byte-check encoding workflows.~~
- ~~Populated `docs/USER_MANUAL.md` with current implemented UI/workflow coverage: menus, sidebar panes, map workspace behavior, cross-platform shortcuts, settings/session persistence, platform notes, and troubleshooting.~~
- ~~Updated repository instructions to require maintaining a living `docs/USER_MANUAL.md` and updating it whenever UI layout, workflows, keyboard shortcuts, or settings behavior changes.~~
- ~~Created initial `docs/USER_MANUAL.md` scaffold to track UI structure, workflows, shortcuts, settings, platform notes, and troubleshooting.~~
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
- ~~Removed scaffold-only map-canvas header text blocks (`TH2 Map Workspace`, geometry preview captions) from the rendered scene to reduce visual noise in the map viewport.~~
- ~~Resized the map geometry preview region to fill the scene canvas (instead of a fixed small top strip), so `Fit` no longer leaves a large empty dark area below geometry.~~
- ~~Updated `mapPreviewBounds` to match the full-canvas preview layout, keeping fit/background placement math consistent with the new map-scene geometry region.~~
- ~~Changed map `Fit` geometry bounds to use only tagged geometry items (paths, anchors, curve handles) instead of the full preview rectangle/scaffold extents, improving framing accuracy.~~
- ~~Updated `Fit + BG` framing behavior to union real geometry bounds with visible background-layer bounds, avoiding oversized empty-canvas margins from non-content scene decorations.~~
- ~~Hardened map-editor pinch zoom handling to prevent viewport lockups: added native-gesture begin/end tracking, short wheel-suppression during active pinch, stale-gesture auto-reset, and finite/clamped per-event zoom delta normalization before applying zoom.~~
- ~~Fixed a map-handle drag crash (`EXC_BAD_ACCESS` in `MapEditableGeometryVertexItem::mouseReleaseEvent`) by preventing post-callback item access in handle `mouseReleaseEvent` when commit callbacks may trigger scene rebuild and item deletion.~~
- ~~Implemented line option parsing for map geometry features (`-close` and `-reverse`) with toggle-value handling (`on/off/yes/no/true/false/1/0` plus bare-flag enable semantics).~~
- ~~Updated map line rendering to honor parsed closed-state by closing the line path when `-close` is enabled.~~
- ~~Surfaced parsed line-state markers in map entry subtitles (`[closed]`, `[reversed]`) for faster state inspection while iterating map parity.~~
- ~~Added `TherionDocumentEditor::rewriteLineOptionToggle` for safe source-line mutation of line toggles (`-close` / `-reverse`) with explicit value rewriting, bare-flag expansion, and comment/line-ending preservation.~~
- ~~Wired line toggle rewrite wrappers through `TextEditorTab` and `MapEditorTab` so line-state changes participate in the current map/text refresh and save flow.~~
- ~~Extended Inspector with line-state controls (`Closed`, `Reversed`) for selected `Lines` objects and connected them to persisted source rewrites plus sidebar/map selection refresh.~~
- ~~Added a corpus-style `TherionDocumentEditorTest` fixture that performs coordinated point/line/area rewrites within a realistic TH2 scrap block containing metadata, comments, and CRLF line endings.~~
- ~~Verified rewrite stability in the fixture across option-led continuation lines (`-subtype`), inline metadata (`-id`), `%` comments, and mixed-precision coordinate tokens.~~
- ~~Added negative corpus-style fixture checks that assert rewrite failures (missing `endline`, incomplete area coordinate tuple) return errors and leave source text unchanged.~~
- ~~Verified `TherionDocumentEditorTest`, `TherionProjectStructureIndexTest`, `MapBackgroundPlacementTest`, `TherionBackgroundMetadataTest`, and `TherionXviParserTest`.~~
- ~~Verified `cmake --build build --target TherionStudio TherionDocumentEditorTest TherionProjectStructureIndexTest MapBackgroundPlacementTest TherionBackgroundMetadataTest TherionXviParserTest` plus execution of all five regression binaries after map input-control changes.~~
- ~~Verified `cmake --build build --target TherionStudio MapGeometryFeatureParsingTest TherionDocumentEditorTest TherionProjectStructureIndexTest MapBackgroundPlacementTest TherionBackgroundMetadataTest TherionXviParserTest` plus execution of all six regression binaries.~~
- ~~Expanded `MapGeometryFeatureParsingTest` with line-option assertions for `-close`/`-reverse` parsing behavior, including explicit on/off and bare-flag cases.~~
- ~~Expanded `TherionDocumentEditorTest` with `rewriteLineOptionToggle` coverage: null/non-line rejection, missing-option insertion before comments, bare-option disable normalization, explicit-value rewrite, and CRLF preservation.~~
- ~~Verified `cmake --build build --target TherionStudio TherionDocumentEditorTest MapGeometryFeatureParsingTest` plus execution of `TherionDocumentEditorTest`, `MapGeometryFeatureParsingTest`, `TherionProjectStructureIndexTest`, `MapBackgroundPlacementTest`, `TherionBackgroundMetadataTest`, and `TherionXviParserTest`.~~
- ~~Added reusable line-curve editing helpers in map scene support: de Casteljau segment split insertion (`insertLineVertexByDeCasteljau`) and neighbor-control reconnect removal (`removeLineVertexWithReconnect`) on TH2 line-vertex sequences.~~
- ~~Expanded `MapGeometryFeatureParsingTest` with direct curve-edit behavior checks for cubic split insertion and middle-vertex removal reconnect semantics.~~
- ~~Verified `cmake --build build --target TherionStudio TherionDocumentEditorTest MapGeometryFeatureParsingTest TherionProjectStructureIndexTest MapBackgroundPlacementTest TherionBackgroundMetadataTest TherionXviParserTest` plus execution of all six regression binaries.~~
- ~~Added safe source rewrite helper `rewriteLineCoordinateRows` for replacing coordinate rows in `line` blocks while preserving `line`/`endline` lines and CRLF, with explicit guardrails that reject inline-coordinate starts and mixed non-coordinate continuation content.~~
- ~~Wired line coordinate-row rewrite through `TextEditorTab` and map-tab command path, including keyboard-driven line anchor editing (`Insert` or `I` = split/add via de Casteljau, `Delete/Backspace` = remove middle anchor with neighbor-handle reconnect).~~
- ~~Added `MapEditableGeometryVertexItem` accessors (`lineNumber`, `geometryKind`, `vertexIndex`) and map-view focus/key handling needed for selection-driven anchor edit commands.~~
- ~~Expanded `TherionDocumentEditorTest` with `rewriteLineCoordinateRows` coverage for null/non-line rejection, inline-start rejection, mixed-content rejection, and successful CRLF-preserving rewrite.~~
- ~~Verified `cmake --build build --target TherionStudio TherionDocumentEditorTest MapGeometryFeatureParsingTest TherionProjectStructureIndexTest MapBackgroundPlacementTest TherionBackgroundMetadataTest TherionXviParserTest` plus execution of all six regression binaries after keyboard vertex-edit wiring.~~
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
