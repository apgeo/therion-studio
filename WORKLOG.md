# Worklog (Phased Reimplementation)

This worklog tracks Therion Studio reimplementation progress against `QtReimplementationSpecification.md` using implementation phases.

Detailed chronological history has been preserved in `WORKLOG_ARCHIVE_2026-05-13.md`.

## Current Focus

### In progress

- Phase 6: encoding detection/conversion and encoding-preserving save semantics.
- Phase 10: interactive map drawing/insertion workflow implementation (first slice active).

### Next up

- Start Phase 9 map-editor parity polish and interaction refinement.
- Continue Phase 10 interactive map drawing/insertion workflow (smart trace and mode-aware undo polish).
- Map editor selection behavior polish: in `Select` mode, clicking an area border should select the referenced `line`; clicking inside area fill should select the `area` object.
- Files sidebar refresh fix: after delete (file/folder via context menu), refresh the tree/model view immediately so removed items disappear without manual re-open/refresh.
- Continue implementation work; defer Phase 6 manual verification pass.

### Risks / blockers

- Parser/serializer round-trip coverage is still incomplete for full Therion corpus-level confidence.
- Non-UTF decoding is now implemented with Qt-supported codecs, but broader legacy-encoding coverage still needs corpus validation.
- Most map behaviors are covered by unit/smoke checks; broader cross-platform GUI automation is still limited beyond focused smoke coverage.

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

### ~~Phase 5 - TH2 Map Workspace and Background Alignment (`MVP`)~~

- ~~Spec scope: 3.3, 3.8.3, 3.11.1.~~
- ~~Goal: split/text/map workspace, geometry rendering/editing, background-layer management, fit/pan/zoom semantics.~~
- ~~Status: Completed (`MVP` closeout complete).~~
- ~~Implemented highlights:~~
- ~~Embedded map workspace modes and map help panel.~~
- ~~Point and line/area vertex drag-to-source writeback.~~
- ~~Undo/redo command integration with merge semantics and obsolete-command handling for failures.~~
- ~~Raster and `.xvi` background parsing/placement/alignment; multi-layer controls and session restore.~~
- ~~Added explicit touch-friendly controls mode in the map toolbar, persisted via session settings, and wired into mode-aware wheel/touch-pan input policy.~~
- ~~Enabled `Open Dedicated Window` map workflow: detach active TH2 map session into a dedicated top-level map window and keep the same in-memory document session (no duplicate document state).~~
- ~~Opening the same TH2 document now focuses the existing detached map window/session instead of creating a duplicate map editor instance.~~
- ~~Simplified TH2 workspace to a single embedded split layout plus detachable map pane; removed `Text Only / Map Only / Split` switcher UX to avoid conflicting states when map is detached.~~
- ~~Fixed undo/redo re-entrancy crash path for map geometry edits by deferring `refreshMapScene()` while map undo commands are executing, then applying a single pending refresh after command completion.~~
- ~~Remaining checklist (Phase 5 closeout):~~
- ~~Add focused regression coverage for live preview callback sequencing during map drags (anchor/control), including no-self-update and no-stale-preview assertions.~~
- ~~Add focused regression coverage for preview-vs-commit parity on representative line-edit operations (anchor move, smooth control move, corner control move).~~
- ~~Add a lightweight GUI smoke test for map drag/undo/redo workflow (single document, deterministic coordinates) to catch scene-refresh and undo-stack regressions.~~
- ~~Re-run Phase 5 verification suite after checklist completion and document residual parity gaps (if any) as Post-MVP.~~
- ~~Verification:~~
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

### Phase 9 - Map Editor Parity Polish (`Post-MVP`)

- Spec scope: 3.3, 3.8.3, 3.11.1.
- Goal: close remaining map-editor interaction/UX parity gaps after MVP completion.
- Status: Not started.
- Planned focus:
- Cross-platform interaction parity polish beyond MVP baseline.
- Expanded GUI automation for map-edit workflows and regressions.
- Residual map command-surface and ergonomics refinements identified during manual QA.
- Planned verification:
- `MapEditorDragUndoRedoSmokeTest`
- `MapEditorDetachedPaneTest`
- Map geometry/background/parser regression suite
- Manual parity pass on macOS, Windows, Linux

### Phase 10 - Interactive Map Drawing and Insertion (`Post-MVP`)

- Spec scope: 3.3, 3.8.3, 3.11.1.
- Goal: replace draft-card-only insertion flows with direct canvas authoring workflows for point/line/area creation.
- Status: In progress.
- Implemented highlights:
- ~~Point mode click-to-place insertion with immediate source writeback.~~
- ~~Line/area click-by-click vertex capture with `Enter`/`Complete Draft` commit.~~
- ~~Line/area draft session controls: `Backspace`/`Delete` removes last vertex, `Esc` cancels active draft.~~
- Planned focus:
- Point mode click-to-place insertion in map coordinates with immediate source writeback.
- Line/area click-by-click draft construction in canvas with explicit finish/cancel behavior and preview feedback.
- ~~Freehand line capture as sampled geometry input; keep smart-trace as explicit staged capability if tracing heuristics are not yet implemented.~~
- Mode-aware undo/redo semantics for drawing sessions (single completed draw gesture = one undo step).
- Planned verification:
- `MapEditorDragUndoRedoSmokeTest`
- New focused GUI smoke tests for point insertion and line/area draft completion paths
- Manual authoring pass on macOS, Windows, Linux (mouse, trackpad, stylus where available)

## Post-MVP Backlog (Phase-Linked)

- Phase 3/7: broader GUI automation for structure/inspector/runner/map workflows.
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
- `MapEditorDetachedPaneTest`
- `MapEditorDragUndoRedoSmokeTest`
- `SessionStoreTest`

## Recent Completed (Latest Slice)

### 2026-05-14

- ~~Updated map-mode status UX in main status bar from plain text to a color badge for faster mode recognition: `Insert` now appears in a red box and `Select` in a green box for active map-editor tabs. Added explicit mode-state query wiring from `MapEditorTab` into `MainWindow::refreshDocumentStatusWidgets` and kept tooltip text with full mode description. Updated user manual/spec requirements. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Improved Bezier area closure fidelity: when interactive area drafting has resolvable closing tangents, the final closing segment (last anchor back to first anchor) is now emitted as an explicit cubic row in the generated closed border line instead of relying on implicit straight `-close` closure. Area draft preview now renders this curved close path as well. Added `MapEditorDragUndoRedoSmokeTest` assertions for explicit cubic closing-row emission and endpoint closure to the first anchor. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest TherionDocumentEditorTest`, `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`, and `./build/TherionDocumentEditorTest`.~~
- ~~Extended interactive area drafting to support bezier curve authoring with the same click/drag control-point workflow as line drafting (including control-handle drag + mirrored opposite handle on smooth vertices). Area commit/cancel paths now serialize drafted coordinate rows into the generated closed border line and keep area reference-block output unchanged. Added `MapEditorDragUndoRedoSmokeTest` coverage asserting area click+drag produces bezier coordinate rows in the referenced border line. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest TherionDocumentEditorTest`, `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`, and `./build/TherionDocumentEditorTest`.~~
- ~~Reworked interactive bezier line-draft seeding to improve curve-authoring ergonomics: click+drag now treats the drag position as a curve pull point and derives cubic controls from that pull point (quadratic-to-cubic elevation), replacing prior midpoint-coupled handle seeding that forced awkward endpoint-parallel behavior. Updated `MapEditorDragUndoRedoSmokeTest` control-hit calculations to the new seeding model. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Fixed interactive line-drafting tangent consistency for consecutive curved anchor placement: when click+drag creates a new curved segment from a draft vertex that already has the opposite control handle, the existing opposite handle is now auto-mirrored immediately (preview + captured draft data) so both controls stay collinear around that vertex. Added `MapEditorDragUndoRedoSmokeTest` regression coverage that commits a 3-anchor consecutive click+drag draft and asserts middle incoming/outgoing controls remain collinear/opposed in serialized TH2 rows. Verified with `cmake --build build --target MapEditorDragUndoRedoSmokeTest TherionStudio` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Changed area insertion serialization to Therion border-reference form: committing an interactive area now writes a closed `line border -id ... -close on` geometry block and an `area ...` block that references that line id (instead of inline area coordinates). Added parser/renderer support for area blocks that reference existing line ids so area fill remains visible, while referenced-border areas are treated as non-directly-editable area vertices in map view. Updated `TherionDocumentEditorTest` and `MapEditorDragUndoRedoSmokeTest` regressions plus user manual/spec behavior notes. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest TherionDocumentEditorTest`, `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`, and `./build/TherionDocumentEditorTest`.~~
- ~~Refined interactive bezier draft editing in Line mode: hovering draft control points now shows hand cursor feedback (`OpenHand` hover, `ClosedHand` drag), and dragging one control on a draft vertex with both handles now mirror-adapts the opposite handle (smooth tangent behavior parity with regular line editing). Expanded `MapEditorDragUndoRedoSmokeTest` with a 3-anchor bezier draft case that drags one middle-vertex control and verifies mirrored opposite-control serialization geometry. Updated specification and user manual behavior notes. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest TherionDocumentEditorTest`, `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`, and `./build/TherionDocumentEditorTest`.~~
- ~~Made bezier control points usable during interactive line insertion: line-draft preview now renders control handles/connectors for curved segments, supports direct handle drag adjustment before commit, and preserves adjusted control coordinates in serialized TH2 line rows. Added smoke coverage in `MapEditorDragUndoRedoSmokeTest` that drags a draft bezier control before commit and verifies serialized control-point coordinates change. Updated specification and user manual behavior notes. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest`, `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`, and `./build/TherionDocumentEditorTest`.~~
- ~~Implemented interactive bezier line insertion semantics in Line mode: click-only anchor placement creates straight segments, while click-and-drag on new anchor placement creates curved segments with control points and serializes bezier coordinate rows. Added `appendDraftLineGeometry` insertion path for explicit line coordinate rows, wired map-line commit/cancel to use it, and expanded `MapEditorDragUndoRedoSmokeTest` with click+drag bezier regression assertions. Updated specification and user manual behavior text. Verified with `cmake --build build --target TherionStudio TherionDocumentEditorTest MapEditorDragUndoRedoSmokeTest`, `./build/TherionDocumentEditorTest`, and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Fixed interactive-drawing commit misplacement/shape-collapse when drafting outside the fitted existing-geometry strip: preview-to-source mapping no longer clamps to fitted bounds and now uses shared inverse map transform directly. This preserves relative drafted shape when committing with `Enter`/`Esc`/`Complete Draft`. Verified with `cmake --build build --target TherionStudio TherionDocumentEditorTest MapEditorDragUndoRedoSmokeTest`, `./build/TherionDocumentEditorTest`, and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Fixed drafted-line shape corruption after multi-click insert: line commits are now serialized as per-anchor coordinate rows (one vertex row per point) instead of a single packed coordinate row that the parser interpreted as cubic control geometry. This preserves drafted polyline shape and placement. Updated `TherionDocumentEditorTest` expected serialization and kept map smoke regression on total committed coordinate tokens across the inserted line block. Verified with `cmake --build build --target TherionStudio TherionDocumentEditorTest MapEditorDragUndoRedoSmokeTest`, `./build/TherionDocumentEditorTest`, and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Fixed line/area interactive-drawing vertex-loss regression: `appendDraftGeometry` no longer truncates to first 2 (line) / first 3 (area) vertices and now writes all captured vertices on `Enter`/`Esc`/`Complete Draft` commit. Added regression coverage in `TherionDocumentEditorTest` (multi-vertex line + area append) and `MapEditorDragUndoRedoSmokeTest` (4-click line draft commit preserves >=8 numeric coordinate tokens). Updated user manual note. Verified with `cmake --build build --target TherionStudio TherionDocumentEditorTest MapEditorDragUndoRedoSmokeTest`, `./build/TherionDocumentEditorTest`, and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Added status-bar map mode indicator for active map-editor tabs: main status area now shows `Map mode: Select` or `Map mode: Insert` and updates live on mode transitions. Implemented via `MapEditorTab::statusModeText()` + `modeStatusChanged` signal wiring into `MainWindow::refreshDocumentStatusWidgets`. Updated user manual and specification requirements. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Hardened Enter/Esc interactive-drawing key handling against focus issues: moved drawing hotkeys to map-tab `QShortcut`s with `WidgetWithChildrenShortcut` context and mode-aware enablement (`Esc` active in insert modes, `Enter` active in line/area modes). Removed duplicate fragile viewport-only Enter/Esc handling path. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Adjusted line/area Enter-commit mode persistence: pressing `Enter` now commits the current valid line/area draft and explicitly keeps the same insert mode active, so users can insert the next line/area without reselecting the tool. Added smoke coverage that commits by Enter and then inserts another line without re-entering Line mode. Updated user manual wording. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Adjusted interactive drawing `Esc` semantics for `Line`/`Area`: pressing `Esc` now exits insert mode to `Select` while preserving captured work by committing the current valid draft (line: >=2 vertices, area: >=3 vertices). Incomplete line/area drafts are canceled. Added smoke coverage for both line and area `Esc`-commit flows in `MapEditorDragUndoRedoSmokeTest`. Updated user manual behavior text. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Fixed interactive drawing cancel behavior for `Esc`: pressing `Esc` now exits active insert mode (`Point`, `Line`, `Freehand`, `Area`) back to `Select`; for `Line`/`Area` it cancels the current draft session. Added a map-editor tab-level `Esc` shortcut (`WidgetWithChildrenShortcut`) so cancel works even when focus is in the text pane. Expanded `MapEditorDragUndoRedoSmokeTest` coverage for point and line Esc-cancel flows. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Implemented direct interactive freehand drawing: `Freehand` mode now captures sampled drag strokes in the map canvas and inserts a line on mouse release (single gesture, no draft-card handoff). Updated user manual workflow text and expanded `MapEditorDragUndoRedoSmokeTest` coverage for freehand insertion. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Started Phase 10 interactive drawing workflow with direct canvas authoring for core modes: `Point` now inserts on click, `Line`/`Area` now capture click-by-click vertices and commit via `Enter` or `Complete Draft`, with `Backspace`/`Delete` last-vertex removal and `Esc` cancel for active drafts. Added smoke coverage in `MapEditorDragUndoRedoSmokeTest` for point click insertion and line draft commit. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Implemented scrap-block text-to-map selection sync: placing text cursor on `scrap` or `endscrap` now selects/highlights all map objects inside that scrap block (line/area/point/station object starts). Added smoke coverage for both directives in `MapEditorDragUndoRedoSmokeTest`. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Mapped text-cursor selection on TH2 block-end directives to owning geometry blocks: placing cursor on `endline` now selects the parent `line` object, and placing cursor on `endarea` selects the parent `area` object (matching block-start behavior). Added smoke coverage in `MapEditorDragUndoRedoSmokeTest` for both end-directive mappings. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Fixed remaining `.th2` active-line highlight drift in map-editor text panes by switching highlighted-row painting from `cursorRect(blockCursor)` to block-layout geometry (`firstVisibleBlock` + `blockBoundingGeometry`/`blockBoundingRect` with `contentOffset()`), matching gutter row positioning and removing per-document-type offset behavior. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Fixed text-editor active-line highlight row offset regression after line-number gutter work: active-line paint now uses the highlighted block’s `cursorRect().top()` directly (no extra line-height shift), so the highlighted band aligns with the actual cursor/source row. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Added text-editor line numbers: `QPlainTextEdit` now renders a synchronized left gutter with 1-based line numbers and active-line emphasis in the custom editor widget. Updated user manual + specification text-editor requirements. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`.~~
- ~~Confirmed non-hardcoded line-option cursor sync: any non-coordinate row inside a `line ... endline` block now maps to the current vertex (not keyword-specific), and smoke coverage now includes an arbitrary option row (`altitude 12`) in addition to `smooth off` / `subtype presumed`. Verified with `cmake --build build --target MapEditorDragUndoRedoSmokeTest MapEditorDetachedPaneTest`, `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`, and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDetachedPaneTest`.~~
- ~~Fixed text-cursor vertex sync for line option rows beyond `smooth`: option-only rows like `subtype presumed` now resolve to the current line vertex, so cursor placement on those rows highlights the corresponding map vertex. Added `MapEditorDragUndoRedoSmokeTest` coverage for `smooth off` and `subtype presumed` row mapping plus shifted blank-line point-sync regression checks. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest MapEditorDetachedPaneTest`, `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`, and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDetachedPaneTest`.~~
- ~~Fixed map click-to-text line targeting for dense geometry: map-selection sync now uses the last left-click scene position (instead of only global-cursor fallback), resolves the clicked selected item deterministically, and ignores decorative overlays by making grid/labels/connectors/detail strokes mouse-transparent. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest MapEditorDetachedPaneTest`, `QT_QPA_PLATFORM=offscreen ./build/MapEditorDragUndoRedoSmokeTest`, and `QT_QPA_PLATFORM=offscreen ./build/MapEditorDetachedPaneTest`.~~
- ~~Hardened map click-intent filtering for text sync: pending click-target capture and cursor-hit fallback now ignore non-interactive/non-selectable map scene items (including mouse-transparent overlays), preventing overlay z-order from biasing source-line navigation to adjacent lines. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest` plus offscreen smoke runs for `MapEditorDragUndoRedoSmokeTest` and `MapEditorDetachedPaneTest`.~~
- ~~Fixed map-driven source-line visual offset by changing text-editor line navigation to place the cursor at the start of the resolved line instead of selecting through line end; the existing full-width current-line highlight now marks the navigated line directly. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest MapEditorDetachedPaneTest` plus offscreen smoke runs for both map-editor tests.~~
- ~~Fixed station/point map-to-text navigation landing on blank separator lines: point selection now resolves the current text row from the selected point coordinates before falling back to scene item line metadata. Added a smoke regression that inserts a blank line above a point and deliberately skews the item line role to prove selection still lands on the coordinate row. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest MapEditorDetachedPaneTest` plus offscreen smoke runs for both map-editor tests.~~
- ~~Separated the text editor's visible current-line highlight from transient cursor normalization: map-driven `goToLine` / `goToLineColumn` calls now set an explicit highlighted source line, so the blue active-line band stays on the resolved row instead of a neighboring blank separator. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest MapEditorDetachedPaneTest` plus offscreen smoke runs for both map-editor tests.~~
- ~~Changed text-editor source-row highlighting from full-width block background to text-range background so the visible highlight sits under the actual command text instead of the inter-line gap. Point source-row resolution now picks the nearest point coordinate within a small tolerance to account for preview/source round-trip precision. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest MapEditorDetachedPaneTest` plus offscreen smoke runs for both map-editor tests.~~
- ~~Restored reliable current-line highlight visibility by combining a full-width active-line band with a stronger text-range overlay on the same source row. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest MapEditorDetachedPaneTest` plus offscreen smoke runs for both map-editor tests.~~
- ~~Removed the full-width text-editor active-line band after it painted in the visual gap above the cursor row on some line layouts; current-line highlighting now uses a stronger text-token background anchored to the actual block text. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest MapEditorDetachedPaneTest` plus offscreen smoke runs for both map-editor tests.~~
- ~~Replaced fragile `QTextEdit::ExtraSelection` active-line rendering with a small `QPlainTextEdit` subclass that paints the highlighted row from actual block geometry, including a left accent strip. This restores visible current-line highlighting while avoiding the previous one-line visual offset. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest MapEditorDetachedPaneTest` plus offscreen smoke runs for both map-editor tests.~~
- ~~Corrected custom text-editor active-line painting to use `cursorRect()` for the highlighted block instead of `blockBoundingGeometry()`, fixing the consistent one-line-up visual offset while preserving the visible row band and accent strip. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest MapEditorDetachedPaneTest` plus offscreen smoke runs for both map-editor tests.~~
- ~~Applied a direct row-offset correction to custom text-editor active-line painting after repeated manual screenshots showed the painted rectangle was consistently one rendered line above the caret/source row. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest MapEditorDetachedPaneTest` plus offscreen smoke runs for both map-editor tests.~~
- ~~Adjusted text-cursor-to-vertex fallback on TH2 line rows: when cursor is outside explicit coordinate tokens on a line row, source-vertex resolution now prefers the row anchor pair (last coordinate pair) instead of nearest-first control pairs. This keeps text-line clicks aligned with expected anchor vertex selection on cubic rows. Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest MapEditorDetachedPaneTest` plus offscreen smoke runs for both map-editor tests.~~
- ~~Adjusted map hit-target priority for sync/navigation selection: anchor/area vertices and points are now preferred over line-control handles when multiple interactive items overlap the click position. This prevents control-handle captures that were driving text navigation to adjacent control rows (per repeated “one line above” vertex-selection screenshots). Verified with `cmake --build build --target TherionStudio MapEditorDragUndoRedoSmokeTest MapEditorDetachedPaneTest` plus offscreen smoke runs for both map-editor tests.~~
- ~~Hardened map-click source-line reveal targeting: map-to-text sync now first resolves the selected item under the current cursor hit-test (when available) before fallback selection iteration, reducing wrong-line highlights caused by ambiguous selected-item ordering.~~
- ~~Fixed map-selection source-line reveal instability: map-to-text sync now resolves the navigation target from the primary visible selected map item, preventing stale hidden selections from overriding the highlighted text line when clicking line/point objects.~~
- ~~Extended text-to-map vertex sync for line option rows: placing the text cursor on vertex-related options (for example `smooth off`) now selects the corresponding current line vertex in map view.~~
- ~~Updated repository instructions (`AGENTS.md`) with an explicit “living specification” rule: keep `QtReimplementationSpecification.md` current for important behavior/acceptance/architecture changes, and prefer framework-agnostic wording for future reimplementation reuse.~~
- ~~Completed a specification-hardening pass for cross-framework reuse: removed stale TH2 mode-switch requirements (`text-only/map-only/split` and workspace-mode persistence), aligned spec to synchronized embedded text+map plus detachable map-pane behavior, and added explicit `Help -> User Manual (Full)` requirement with fallback semantics.~~
- ~~Added persistent active-line highlighting in the text editor (`QPlainTextEdit` extra-selection), so map-driven object/vertex selections visibly highlight the resolved source line without relying on text-range selection.~~
- ~~Implemented vertex-level bidirectional map/text sync: selecting line/area vertices (including line control points) now navigates text to the matching coordinate token, and moving the text cursor onto coordinate tokens now selects the corresponding map vertex/control.~~
- ~~Fixed reverse map/text sync gap: selecting map objects now moves the TH2 text cursor to the selected object's source line (cards and geometry items), matching existing text-to-map highlighting behavior.~~
- ~~Updated map-editor geometry presentation to reduce clutter: line/area vertices and control handles now appear only for selected objects, and line accent highlight overlays render only for selected lines.~~
- ~~Refined map-editor vertex visibility behavior: line control handles/connectors now appear only for the selected line vertex (or selected control handle), while non-selected vertices keep their controls hidden.~~
- ~~Removed Qt menu-construction deprecation warnings by replacing deprecated `QMenu::addAction(..., shortcut)` overload usage with explicit `QAction` setup and signal wiring in `MainWindow::buildMenus`.~~
- ~~Updated repository agent instructions to require avoiding deprecated APIs in new or touched code paths.~~
- ~~Expanded `Help` menu with in-app `Quick User Manual` and `User Manual (Full)` viewer actions while keeping contextual map-help panel workflows intact.~~
- ~~Added live appearance-switch handling so runtime OS light/dark changes now reapply app chrome styling and refresh map workspace rendering without relaunch.~~
- ~~Fixed map-theme regression where dark-mode app chrome could still render a light map canvas: map theme selection now cross-checks `QStyleHints::colorScheme` against effective application palette lightness and prefers palette-derived mode when they disagree.~~
- ~~Fine-tuned light-mode map rendering: explicit OS light/dark scheme now takes precedence over scene-local palette overrides, and light-theme canvas/grid/label/stroke values were adjusted for cleaner contrast balance and stronger line readability.~~
- ~~Fixed sidebar-rail and status-bar divider styling to use palette-driven border color (`palette(mid)`) instead of a hardcoded light gray, keeping separators consistent in dark mode while preserving light-mode contrast.~~
- ~~Applied a dark-mode map readability polish pass: increased grid contrast/weight, brightened map label text, slightly strengthened dark-theme geometry base stroke, and raised accent overlay alpha for clearer line visibility at normal zoom.~~
- ~~Fixed dark-mode map-canvas theme detection: map scene theme now prefers `QStyleHints::colorScheme` (OS light/dark) with guarded palette fallback, preventing false light-canvas rendering in dark mode.~~
- ~~Improved map-canvas theme contrast in `MapEditorSceneRenderer`: canvas/frame/grid, geometry strokes/fills, labels, and control-handle visuals now adapt to active light/dark palette so line/area geometry remains visible in OS light mode.~~
- ~~Started Phase 6 encoding workflow: added encoding-aware `DocumentFile` read/write APIs with detection (`UTF-8`/`UTF-16*`/`System`/`Latin1`) and codec-preserving save behavior.~~
- ~~Extended Phase 6 encoding workflow with Therion `encoding ...` directive-aware decoding and codec-name preserving save semantics (including legacy single-byte codecs when supported by Qt).~~
- ~~Added explicit text-editor `Convert to UTF-8` action (visible for non-UTF documents) and wired saves to preserve original encoding until conversion.~~
- ~~Polished conversion UX: `Convert to UTF-8` now requires confirmation, marks conversion as an in-memory pending change, and shows explicit encoding-status notes about save behavior.~~
- ~~Removed hard minimum sidebar width clamp and relaxed sidebar/page/content minimum-size policies so the sidebar can be resized to narrow widths across Files/Structure/Map/Console panes.~~
- ~~Aligned sidebar collapsed rail width with the activity-menu strip by fixing the rail width and matching dock minimum/collapsed width constraints, so the left action menu no longer changes width when collapsing/expanding.~~
- ~~Locked dock width while sidebar is collapsed (min=max=rail width) and restored normal width constraints on expand, preventing empty-strip growth when dragging the sidebar border in collapsed mode.~~
- ~~Made sidebar activity rail width icon-driven (computed from button layout) instead of fixed padding-heavy width, and reused that measured rail width for collapsed dock sizing so no extra white gutter appears next to the left action bar.~~
- ~~Updated activity-rail icon click behavior to treat rail-only manual resize as collapsed state, so clicking a rail icon expands sidebar content even when width was reduced via border drag instead of collapse toggle.~~
- ~~Fixed crash when clicking sidebar rail icon after manual resize-to-rail width: activity-button handler now captures the collapse-state probe lambda by value (not dangling reference), eliminating post-build lambda lifetime invalid access in `buildStructureSidebar` callbacks.~~
- ~~Fixed no-op rail clicks after manual resize-to-zero width: activity icon handlers now detect “effectively collapsed but flag is false”, force sidebar content visibility restore, reset dock width constraints, and restore expanded width before switching panes.~~
- ~~Added a global status-bar top divider border via app theme styling (`main.cpp`) so the bottom window-edge status bar is visually separated from the main content area.~~
- ~~Removed double-divider artifact at the bottom-left edge by clearing `SidebarDock` frame borders and styling `SidebarActivityRail` with only a right-edge border; keeps a single status-bar top divider while extending rail visually to the status edge.~~
- ~~Adjusted divider contrast to match surrounding UI separators by switching sidebar-rail and status-bar border colors from `palette(mid)` to `palette(midlight)`.~~
- ~~Rebalanced divider visibility after the previous contrast reduction: sidebar-rail right border and status-bar top border now use a slightly stronger neutral gray so separators stay visible without looking heavier than surrounding UI lines.~~
- ~~Moved active-document context to the main window status bar: full path + encoding are now shown globally for the focused document tab, with live updates on tab/document state changes.~~
- ~~Removed duplicate per-tab path/encoding rows from map workspace and simplified text-tab inline row to encoding-note / `Convert to UTF-8` only (source `encoding ...` command text remains untouched).~~
- ~~Restored visible left status/message area (`Ready` and transient status messages) by constraining global path widget width and rendering document path with middle elision in the status bar.~~
- ~~Removed extra visual divider line above the status bar by dropping the forced status-bar top border style, eliminating double-separator appearance with existing central-pane borders.~~
- ~~Reintroduced the status-bar top divider (`#bebebe`) after path/encoding status-bar refactor so bottom chrome remains visually anchored; kept `QStatusBar::item { border: 0; }` to avoid the earlier double-line artifact.~~
- ~~Added sidebar auto-snap collapse threshold on manual resize: if dock width is reduced below a minimum usable content width, sidebar now snaps to rail-collapsed mode; expanded-width memory now ignores tiny widths so re-expand restores a practical width.~~
- ~~Fixed sidebar auto-snap regression introduced by initial threshold logic: added collapse-transition guard to prevent collapse/resize race, enabled reliable auto-restore of content when manually widening a collapsed sidebar past threshold, and added hidden-content self-heal during non-collapsed dock resizes.~~
- ~~Reworked sidebar snap/collapse recovery after the resize regression: collapsed sidebars now keep the pane in layout but clamp its width to zero, the rail is fixed to icon width, and manual widening past the snap threshold restores content in-place instead of fighting the drag with saved-width restoration.~~
- ~~Verified `cmake --build build --target TherionStudio` after the sidebar snap/collapse recovery changes.~~
- ~~Disabled the unstable DockWidget auto-snap resize hook after continued rail-floating regressions: manual sidebar resizing is now free-form again, explicit rail collapse remains locked to icon width, and narrow manual widths recover via activity-icon click instead of resize-loop snapping.~~
- ~~Updated `docs/USER_MANUAL.md` to document the restored free-form sidebar resize behavior and verified `cmake --build build --target TherionStudio`.~~
- ~~Replaced the left sidebar `QDockWidget` with a main horizontal splitter pane: the activity rail is now fixed-width, sidebar resizing affects only the content pane, and collapse/expand uses splitter sizes rather than dock constraints.~~
- ~~Removed obsolete DockWidget title-bar helper code from the sidebar implementation after the splitter migration.~~
- ~~Updated `QtReimplementationSpecification.md` and `docs/USER_MANUAL.md` for the fixed-rail/resizable-content sidebar behavior; verified `cmake --build build --target TherionStudio`.~~
- ~~Fixed splitter-sidebar snap artifact by making the activity rail, sidebar content, and editor separate main-splitter panes; collapsing now sets only the content pane to width 0, so no empty left panel remains between rail and editor.~~
- ~~Verified `cmake --build build --target TherionStudio` after the three-pane splitter correction.~~
- ~~Removed the unwanted rail splitter handle by moving the fixed activity rail outside the splitter; the splitter now owns only sidebar content and editor, so the only resize handle is between content and editor.~~
- ~~Verified `cmake --build build --target TherionStudio` after moving the rail outside the splitter.~~
- ~~Restored reliable sidebar auto-snap with the fixed-rail splitter design: dragging content below the usable-width threshold now immediately collapses only the content pane, while the rail keeps its icon-driven width and no rail handle is shown.~~
- ~~Updated `QtReimplementationSpecification.md` and `docs/USER_MANUAL.md` for automatic sidebar content snap behavior; verified `cmake --build build --target TherionStudio`.~~
- ~~Removed visible triangle toggle symbols from text and map contextual-help splitter handles, leaving clean splitter-only help panes; updated the user manual accordingly.~~
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
- ~~Enabled dedicated map-window lifecycle parity in `MainWindow`: detached map sessions are tracked in open-document session state, save-all/dirty-close prompts include detached maps, and explicit open actions reuse/focus existing detached sessions by canonical path.~~
- ~~Updated `Close All Tabs` to include detached map sessions (with unsaved-change confirmation) so tab-level close workflows behave consistently across embedded and detached TH2 editors.~~
- ~~Fixed detached map-window parenting/transfer path so `Open Dedicated Window` moves the actual `MapEditorTab` into a true top-level window (preventing empty detached-window opens).~~
- ~~Implemented map-pane detach workflow inside `MapEditorTab`: `Open Map in Window` now moves only the graphical map pane into a second top-level window while keeping text editing in the main tab, and `Return Map Pane` (or closing detached window) reattaches it.~~
- ~~Simplified TH2 workspace UX by removing the `Text Only / Map Only / Split` switcher and standardizing on embedded split view plus detachable map pane window.~~
- ~~Removed obsolete map workspace-mode persistence API from `SessionStore` (`therionMapWorkspaceMode` read/write), since split layout is now fixed by design.~~
- ~~Added `MapEditorDetachedPaneTest` coverage for map-pane detach/reattach lifecycle: open-window transition, single detached-window presence, reattach via toggle, and no leftover detached window after return.~~
- ~~Fixed map-editor undo crash after vertex moves: map command apply now defers scene rebuild during command execution (`mapCommandApplyInProgress_` / `mapSceneRefreshPending_`), preventing `undoStack_->clear()` re-entrancy corruption while text rewrite signals are emitted.~~
- ~~Fixed map-canvas drag-preview regression after undo-crash hardening: editable point/vertex items now use expanded repaint bounds to prevent ghost trails, and line-path/control-connector visuals are recomputed live while dragging anchors/handles so geometry updates in real time before commit.~~
- ~~Fixed map-editor undo-history regression introduced by deferred scene refresh: command-originated refresh now rebuilds map visuals without clearing the undo stack, so vertex/control-point moves can be undone/redone correctly while preserving crash-safe re-entrancy guards.~~
- ~~Fixed text-tab dirty-state false-positive after map undo/redo-to-baseline: dirty marker is now derived from current text+encoding snapshot versus last load/save snapshot, so undoing all edits clears `*modified` as expected.~~
- ~~Fixed remaining map-undo dirty false-positive by preserving coordinate token precision/style in `TherionDocumentEditor` point/line vertex rewrites (including integer tokens), so moving geometry and undoing back restores original source text instead of normalization drift.~~
- ~~Fixed persistent dirty marker on CRLF-backed files after undo-to-baseline: clean-text snapshot now captures the editor-normalized text representation on load, preventing false dirty due to line-ending canonicalization (`\r\n` file bytes vs `\n` in editor).~~
- ~~Improved smooth line-handle editing preview: while dragging an incoming/outgoing control on a smooth vertex, the opposite handle is now mirrored live (collinear update during drag), so curve shape no longer jumps only after mouse release.~~
- ~~Refactored smooth opposite-handle mirroring into shared helper `mirroredSmoothControlPoint` and reused it in both map-preview drag coupling and commit-time line-vertex rewrite coupling so preview and persisted behavior stay mathematically consistent.~~
- ~~Added `MapGeometryFeatureParsingTest` regression coverage for smooth-mirroring math: preserved opposite length, collinear/opposite direction, missing-opposite mirror-length fallback, and zero-vector `nullopt` behavior.~~
- ~~Fixed map anchor-drag preview parity: while dragging a line anchor, attached incoming/outgoing control handles now translate in real time by the same source-space delta (not only after drop), keeping curve shape stable during interactive edits.~~
- ~~Fixed map-handle drag boundary clamp: editable point/vertex/control handles are now constrained by full map preview bounds instead of the fitted geometry rectangle, allowing control points to cross grid lines and move across the full canvas region.~~
- ~~Fixed remaining fitted-rect coordinate clamp during map edit drags: `sceneCoordsPreviewToSource` now converts preview positions without clamping to fitted geometry bounds, so vertex and control-point drags continue updating source coordinates beyond fitted-rect edges and no longer snap back at interior grid-aligned boundaries.~~
- ~~Added regression coverage for unclamped map preview-to-source conversion (`MapGeometryFeatureParsingTest`): points dragged outside fitted geometry bounds now map to out-of-range source coordinates (no implicit fitted-rect clamp), preventing reintroduction of vertex/control snap-back at interior grid-aligned boundaries.~~
- ~~Refactored line-vertex secondary move coupling into shared helper `collectLineSecondaryMovesForVertexDrag` and wired map command generation through it, keeping anchor/control coupling behavior consistent across preview and commit paths.~~
- ~~Added `MapGeometryFeatureParsingTest` regression coverage for anchor-drag coupling: dragging an anchor now produces incoming/outgoing control secondary moves translated by the same source delta.~~
- ~~Expanded `MapGeometryFeatureParsingTest` with line-control coupling regressions: smooth incoming-control drags now assert one mirrored opposite-control secondary move with preserved opposite-handle length, while corner (`smooth off`) drags assert no coupled secondary move.~~
- ~~Unified live-preview and commit-time line coupling behavior by routing map preview coupling through shared `collectLineSecondaryMovesForVertexDrag`, removing duplicated preview-only coupling math and reducing parity drift risk between drag preview and persisted undoable edits.~~
- ~~Added preview-specific coupling helper `collectLinePreviewSecondaryMovesForVertexDrag` with defensive self-target filtering for malformed/shared source-index cases, and routed live preview coupling through it to prevent accidental self-reposition loops during drag callbacks.~~
- ~~Expanded `MapGeometryFeatureParsingTest` with preview-coupling self-filter regression: malformed same-source incoming/outgoing control mapping now yields command-time secondary move but empty preview secondary move set after self-target filtering.~~
- ~~Added current-state-aware preview coupling helper `collectLinePreviewCoupledUpdatesForVertexDrag` and wired live map preview updates through it so anchor-drag control transport is delta-sequenced (no stale absolute reset between drag events).~~
- ~~Expanded `MapGeometryFeatureParsingTest` with live-preview sequencing regression coverage: multi-step anchor drag preview now asserts cumulative control transport, plus explicit no-self-update filtering in malformed shared source-index cases.~~
- ~~Expanded `MapGeometryFeatureParsingTest` with explicit preview-vs-commit parity coverage for representative line edits: anchor move, smooth control move, and corner control move now assert identical coupled-secondary move sets between preview and command-time coupling helpers.~~
- ~~Added `MapEditorDragUndoRedoSmokeTest` as a focused GUI smoke executable for deterministic line-anchor drag + undo + redo, asserting text mutation, undo-to-baseline restoration, and dirty-state transitions through the map command path.~~
- ~~Completed Phase 5 checklist re-verification pass after smoke-test integration: ran `MapGeometryFeatureParsingTest`, `MapBackgroundPlacementTest`, `TherionBackgroundMetadataTest`, `TherionXviParserTest`, `TherionDocumentEditorTest`, `MapEditorDetachedPaneTest` (offscreen), and `MapEditorDragUndoRedoSmokeTest` (offscreen).~~
- ~~Separated map-editor Post-MVP parity work into standalone Phase 9 and marked Phase 5 as MVP-complete.~~
- ~~Hardened map-geometry undo/redo dirty-state fidelity: point and line/area vertex move commands now restore exact pre/post command text snapshots through `TextEditorTab::replaceTextForCommand`, eliminating residual dirty flags caused by rewrite-path formatting drift in multi-step map commands.~~
- ~~Extended Phase 5 line-edit parity with smoothness toggling: map canvas now supports `S` to toggle selected line vertex smooth/corner state, line-coordinate rewrite validation now accepts `smooth` continuation rows, and line-row regeneration preserves `smooth off` markers during map-driven line rewrites.~~
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
