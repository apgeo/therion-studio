# Worklog (Phased Reimplementation)

This worklog tracks Therion Studio reimplementation progress against `QtReimplementationSpecification.md` using implementation phases.

Detailed chronological history has been preserved in `WORKLOG_ARCHIVE_2026-05-13.md` and `WORKLOG_ARCHIVE_2026-05-23.md`.

## Current Focus

### In progress

- Phase 6: encoding detection/conversion and encoding-preserving save semantics; implementation is mostly complete, manual cross-platform encoding QA remains deferred.
- Phase 9: map-editor parity polish; current focus is TH2 Visual `Inspector -> Objects` drag/drop validation and follow-up polish.
- Phase 10: interactive map drawing/insertion workflow; smart trace and undo semantics remain open.
- Phase 11: structured text authoring canvas; continue broader directive/configuration coverage and safer nested insertion behavior.

### Next up

- Finish manual validation of the TH2 Visual `Inspector -> Objects` row-drag workflow on the running app, then commit the drag/drop batch.
- Continue Phase 9 map-editor parity polish with the next interaction gap found during manual validation.
- Continue Phase 10 mode-aware undo/redo polish so one completed draw gesture maps to one undo step.
- Continue Phase 11 structured block-canvas coverage: richer block configuration, safer insertion guards, and broader directive support.

### Backlog

- Later refactor phase: unify undo/redo into one document-level command stack per editor session, covering raw text edits, visual map mutations, inspector setting changes, object/background edits, and structured blocks changes instead of arbitrating between `QTextDocument` undo and the map `QUndoStack`.
- Nice-to-have: TH2 Visual grid fallback for documents without parseable geometry or valid source bounds, e.g. user-defined grid origin/extent so the grid can be shown before map objects/backgrounds exist.
- Phase 3/7: broader GUI automation for structure, inspector, runner, map workflows, and cross-platform keyboard/shortcut matrix coverage.
- Phase 4: expand rewrite corpus/regression coverage beyond the MVP fixture set for higher corpus-level confidence.

### Risks / blockers

- Parser/serializer round-trip coverage is still incomplete for full Therion corpus-level confidence.
- Non-UTF decoding is now implemented with Qt-supported codecs, but broader legacy-encoding coverage still needs corpus validation.
- Most map behaviors are covered by unit/smoke checks, but broader cross-platform GUI automation is still limited beyond focused smoke coverage.
- Current map/text undo arbitration still depends on choosing between the map `QUndoStack` and the embedded text editor undo stack until the later unified command-stack refactor.

### Active Refactor Guardrails

- Keep `StructureConstraintsTest` as mandatory guardrail on every refactor slice; ratchet limits downward immediately after verified line-count reductions.
- Track A (Text editor): keep `TextEditorTab` as a thin orchestration shell, continue narrowing raw completion/catalog services, and reduce remaining friend/controller boundaries without merging responsibilities back into the tab.
- Track B (Map editor): extract `MapEditorTab` responsibilities into focused controllers in this order: interactive drawing, inspector objects, inspector backgrounds, selection details, scene lifecycle, undo/snapshot orchestration.
- Track C (Shared core): consolidate shared source-edit/rewrite primitives used by Raw/Block/Map modes into focused non-UI services to reduce duplicate mutation logic and improve round-trip testability.
- Slice policy: each slice shall own one responsibility, keep behavior stable, pass build + full tests + `StructureConstraintsTest`, and be logged in `Completed` with verification commands.
- Milestones: reduce `TextEditorTab.cpp` and `MapEditorTab.cpp` below current ratchets first, then continue toward sub-5k-line translation units without introducing catch-all classes.

### Remaining BlockEditor Extraction Plan

- Canvas rebuild/rendering: extract `rebuildBlocksCanvasFromText(...)` first into a focused `BlockEditorCanvasRebuildController`; preserve encoding-root normalization, card creation, parent/child layout, boundary guide generation, and post-rebuild selection/details refresh behavior.
- Toolbox and scope logic: extract `populateBlockToolbox(...)`, `populateBlockToolboxScopeCombo(...)`, `selectedBlockInsertionContextToken(...)`, and `updateBlockToolboxScopeLabel(...)` after canvas rebuild, because scope auto-detection depends on selected canvas context and source outline rules.
- Selection/details glue: keep the existing details controllers, but later consolidate the remaining thin delegates and canvas item helpers (`selectBlockInCanvasAndDetails`, `refreshBlockDetailsSelectionFromScene`, `resolveBlockCanvasItem`, `selectBlockCanvasItem`, etc.) into a selection coordinator if the API boundary remains stable.
- Low-level wrappers: keep tiny `TextEditorTab` delegate methods for app-facing APIs (`configureCommandAtLine`, `deleteCommandAtLine`, raw/map source rewrite calls) until MapEditor ownership is reviewed, so BlockEditor extraction does not destabilize map/raw workflows.
- Next implementation slice: selection/details glue consolidation, with no behavior changes and a structure ratchet only after build + full CTest + `StructureConstraintsTest` pass.

### Completed

- ~~Cleaned up active `WORKLOG.md`: consolidated current focus/backlog wording, moved the long top-level completed-history block and dated recent-completed history to `WORKLOG_ARCHIVE_2026-05-23.md`, updated Phase 9 status/highlights to reflect current map-editor parity polish, and refreshed the automated test inventory. Verified with `git diff --check`.~~
- ~~Added invalid-drop feedback for TH2 Visual `Inspector -> Objects` row dragging: invalid targets suppress the placement line, use a forbidden cursor, and show an explanatory status note. Expanded `MapEditorDragUndoRedoSmokeTest` to exercise an invalid self-drop before restarting a valid drop. Updated the user manual. Verified with `cmake --build build --target MapEditorDragUndoRedoSmokeTest TherionStudio`, `cmake --build build-strict --target MapEditorDragUndoRedoSmokeTest TherionStudio`, `ctest --test-dir build --output-on-failure -R "MapEditorDragUndoRedoSmokeTest|StructureConstraintsTest"`, `ctest --test-dir build-strict --output-on-failure -R "MapEditorDragUndoRedoSmokeTest|StructureConstraintsTest"`, `python3 scripts/check_structure_constraints.py`, and `git diff --check`.~~
- ~~Hardened TH2 Visual `Inspector -> Objects` row drag/drop feedback and undo routing: the drop target is now a slim highlighted line aligned to the target row edge, active row dragging uses a closed-hand cursor, object moves run under the map-command apply guard so they do not leave duplicate raw-text undo entries, and `MapEditorDragUndoRedoSmokeTest` now asserts indicator geometry, drag cursor, and clean undo/redo stack state. Updated the user manual. Verified with `cmake --build build --target MapEditorDragUndoRedoSmokeTest TherionStudio`, `cmake --build build-strict --target MapEditorDragUndoRedoSmokeTest TherionStudio`, `ctest --test-dir build --output-on-failure -R "MapEditorDragUndoRedoSmokeTest|MapEditorObjectMovePlannerTest|StructureConstraintsTest"`, `ctest --test-dir build-strict --output-on-failure -R "MapEditorDragUndoRedoSmokeTest|MapEditorObjectMovePlannerTest|StructureConstraintsTest"`, and `python3 scripts/check_structure_constraints.py`.~~
- ~~Polished TH2 Visual `Inspector -> Objects` drag affordances: movable row labels/grips now use an open-hand hover cursor, active dragging keeps the closed-hand cursor even over non-target gaps, and dragging near the top/bottom of the object tree auto-scrolls the viewport. Expanded `MapEditorDragUndoRedoSmokeTest` to assert mouse tracking and hover cursor behavior. Updated the user manual. Verified with `cmake --build build --target MapEditorDragUndoRedoSmokeTest TherionStudio`, `cmake --build build-strict --target MapEditorDragUndoRedoSmokeTest TherionStudio`, `ctest --test-dir build --output-on-failure -R "MapEditorDragUndoRedoSmokeTest|StructureConstraintsTest"`, `ctest --test-dir build-strict --output-on-failure -R "MapEditorDragUndoRedoSmokeTest|StructureConstraintsTest"`, `python3 scripts/check_structure_constraints.py`, and `git diff --check`.~~
- ~~Hardened TH2 Visual `Inspector -> Objects` drag cancellation state: leaving the object tree or losing the left-button drag now clears the pressed row, drop indicator, and hand cursor, and the row drag can restart cleanly afterward. Expanded `MapEditorDragUndoRedoSmokeTest` to exercise leave cancellation before a successful drop. Verified with `cmake --build build --target MapEditorDragUndoRedoSmokeTest TherionStudio`, `cmake --build build-strict --target MapEditorDragUndoRedoSmokeTest TherionStudio`, `ctest --test-dir build --output-on-failure -R "MapEditorDragUndoRedoSmokeTest|StructureConstraintsTest"`, `ctest --test-dir build-strict --output-on-failure -R "MapEditorDragUndoRedoSmokeTest|StructureConstraintsTest"`, `python3 scripts/check_structure_constraints.py`, and `git diff --check`.~~
- ~~Adjusted TH2 Visual `Inspector -> Objects` drop target line thickness from 3 px to 2 px and updated the drag/drop smoke assertion. Verified with `cmake --build build --target MapEditorDragUndoRedoSmokeTest TherionStudio`, `cmake --build build-strict --target MapEditorDragUndoRedoSmokeTest TherionStudio`, `ctest --test-dir build --output-on-failure -R "MapEditorDragUndoRedoSmokeTest|StructureConstraintsTest"`, `ctest --test-dir build-strict --output-on-failure -R "MapEditorDragUndoRedoSmokeTest|StructureConstraintsTest"`, `python3 scripts/check_structure_constraints.py`, and `git diff --check`.~~
- Detailed completed history from this section was moved to `WORKLOG_ARCHIVE_2026-05-23.md`.

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
- Status: In progress.
- Implemented highlights:
- ~~Select-mode area hit disambiguation: area borders select referenced `line` objects, while interior fill clicks select the `area` object.~~
- ~~Map/object selection parity: graphical map selection now syncs to `Inspector -> Objects`, and empty-canvas clicks clear stale inspector object selection.~~
- ~~TH2 Visual `Inspector -> Objects` row moving foundation: `MapEditorObjectMovePlanner` handles point/line/area source-span moves before/after object targets and into scrap targets while preserving line endings and full block spans.~~
- ~~TH2 Visual `Inspector -> Objects` row drag/drop UI: movable rows expose grip handles, hover/open-hand and drag/closed-hand cursors, a 2 px drop-target line, invalid-target forbidden cursor/status feedback, edge auto-scroll, leave cancellation, selection restoration, and clean map undo/redo routing.~~
- Planned focus:
- Cross-platform interaction parity polish beyond MVP baseline.
- Expanded GUI automation for map-edit workflows and regressions.
- Residual map command-surface and ergonomics refinements identified during manual QA.
- Manual cross-platform parity validation for `Inspector -> Objects` row dragging.
- Planned verification:
- `MapEditorDragUndoRedoSmokeTest`
- `MapEditorObjectMovePlannerTest`
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
- ~~Map object details now expose `Edit Object Settings...` in Visual mode and route `scrap`/`point`/`line`/`area` edits through the same catalog-driven configuration workflow used by structured block selection (with cursor-line fallback when no geometry item is selected).~~
- ~~Map object details now provide inline orientation controls (enable + `0..359.999` degrees) for point symbols and selected line anchor vertices, with source-synchronized `-orientation` rewrite/remove behavior and catalog-driven type/subtype applicability gating.~~
- Planned focus:
- ~~Point mode click-to-place insertion in map coordinates with immediate source writeback.~~
- ~~Line/area click-by-click draft construction in canvas with explicit finish/cancel behavior and preview feedback.~~
- ~~Freehand line capture as sampled geometry input; keep smart-trace as explicit staged capability if tracing heuristics are not yet implemented.~~
- Mode-aware undo/redo semantics for drawing sessions (single completed draw gesture = one undo step).
- Planned verification:
- `MapEditorDragUndoRedoSmokeTest`
- New focused GUI smoke tests for point insertion and line/area draft completion paths
- Manual authoring pass on macOS, Windows, Linux (mouse, trackpad, stylus where available)

### Phase 11 - Structured Text Authoring Canvas (`Post-MVP`)

- Spec scope: 3.2.
- Goal: provide a more user-friendly structured authoring surface while preserving raw Therion text as canonical.
- Status: In progress.
- Implemented highlights:
- ~~Reverted earlier tree-table structured editor slice and replaced it with a new experimental `Blocks` mode in `TextEditorTab`.~~
- ~~Added `Raw`/`Blocks` mode toggle with `.th`-only availability gating and automatic fallback to `Raw` for caret-precise commands.~~
- ~~Implemented drag-and-drop block toolbox + canvas cards PoC for `survey`, `centerline`, `data`, `map`, `scrap`, `team`, and `explo-date`, with source-synchronized insertion templates.~~
- ~~Added first-pass block `Config` actions: rename for survey/map/scrap and value editing for team/explo-date, applied through source rewrite paths.~~
- ~~Expanded block configuration for centerline/data authoring: centerline `Config` now supports quick insertion of `team`, `explo-date`, and starter `data` definitions; `data` `Config` now provides a structured dialog for editing header columns plus multi-row measurement content in one source-synchronized commit.~~
- ~~Upgraded `data` block structured editor to a schema-driven table: table columns are derived from `data ...` field definition, measurement rows are edited in-table with add/remove actions, and non-tabular rows (for example `extend ...`) are preserved in place.~~
- ~~Improved `data` table ergonomics: visible table width/columns now auto-size from active `data ...` fields, and pressing Enter in the last column creates/advances to a new row at first column for faster entry flow.~~
- ~~Added generic editable non-tabular directive rows in `data` block dialog: users can add/remove/edit rows such as `extend ...` without raw-mode switch and without command-specific hardcoded buttons.~~
- ~~Added drag-based block reorder in `Blocks` mode canvas: dragging a card now reorders source lines, supports whole-span container moves (including nested lines), and supports drop-into-compatible-container behavior.~~
- ~~Implemented multi-parameter option editing in Block Details for structured-option commands: catalog option metadata now drives fixed-arity parameter forms (`Selected Option Parameters`), exact-arity validation, and safe Therion token serialization for multi-value options such as `survey -person-rename \"old\" \"new\"`.~~
- ~~Fixed structured-option value parsing for single-value options with spaces (for example `-title`): Block Details now treats the full value cell as one logical value for `EXACTLY_ONE`/fixed-1 options, while still tokenizing true multi-value options such as `-person-rename`. Also clarified action intent by renaming `Add Option` to `Add New Option`.~~
- ~~Simplified option-section label wording in Block Details and legacy configure dialog from `Options (key/value pairs)` to `Options` to reduce UI noise.~~
- ~~Moved option row actions into the `Options` header as compact `+` / `-` controls in Block Details and removed them from the footer action row; `-` is now selection-aware (disabled when no option row is selected).~~
- ~~Fixed structured-options UX bug for multi-parameter options (`-person-rename` etc.): fixed-arity `>1` option value cells in the options table are now read-only and must be edited via `Selected Option Parameters`; validation errors now focus the offending option row automatically.~~
- ~~Fixed false arity errors on `Apply` after editing `Selected Option Parameters`: structured-options loader now preserves multi-value token boundaries when populating options table (re-serializes fixed-arity `>1` values with Therion-safe quoting), so revalidation no longer mis-splits names containing spaces.~~
- ~~Fixed `Blocks`-mode crash when selecting `survey` (and other structured-option blocks): `refreshBlockDetailsOptionArgumentEditors()` now blocks options-table signals while toggling per-row editability/tooltips, preventing recursive `itemChanged -> refresh -> setFlags` stack overflows.~~
- ~~Updated `QtReimplementationSpecification.md` structured-block requirements to explicitly mandate catalog-driven fixed multi-value option arity handling in Block Details (one field per required value label), exact-arity validation before apply, and token-boundary-preserving serialization for spaced values.~~
- ~~Made structured-options primary field naming data-driven: when a block uses structured options but is not in explicit ID mode, the Block Details primary label now resolves from the command catalog’s first argument signature (for example `source` now shows `File Name` instead of hardcoded `Primary Value`).~~
- ~~Fixed contextual-help dark-mode live adaptation in text editing surfaces: `TextEditorTab` help browsers now sync foreground/link palette roles (not only background), apply palette-driven document CSS (`body`/`a`/`code`) on appearance change, and refresh help content immediately on theme switch. Also wired `QStyleHints::colorSchemeChanged` in `TextEditorTab` to force realtime updates when platform color scheme toggles without focus/cursor movement. Verified with `cmake --build build`.~~
- ~~Fixed remaining contextual-help dark-mode lag caused by stale overridden palette reuse: panel/help surface sync now resolves colors from the current application palette (`QApplication::palette(...)`) on every appearance update in both `TextEditorTab` and `MapEditorTab`, preventing white help surfaces after runtime theme toggles. Verified with `cmake --build build`.~~
- ~~Hardened runtime theme-change propagation for help panels without deprecated APIs: installed app-level event filtering in `TextEditorTab` and `MapEditorTab` (`qApp->installEventFilter(this)`) and now react to `ApplicationPaletteChange` / `PaletteChange` / `StyleChange` events from the application object directly. This ensures contextual help repaints even when child widgets do not receive local palette events during mode/theme transitions. Verified with `cmake --build build`.~~
- ~~Fixed contextual-help palette source mismatch during macOS light/dark switching: help panels now derive their palette from the active editor/map widget palette instead of the application palette, and `QTextBrowser` plus viewport receive explicit background/text stylesheets so stale document or viewport colors cannot leave the help panel white in dark mode or dark in light mode. Verified with `cmake --build build`.~~
- ~~Added missing Phase 11 spec alignment for implemented Blocks UX behavior: toolbox scope filter requirements (`Auto` default + selected-block-derived context + persistent manual scope), hidden editable Block Details when no canvas block is selected, and command-level contextual help focus retention while editing options.~~
- ~~Promoted Blocks terminology from PoC/experimental to standard structured block-canvas wording in active docs: updated spec section heading/requirements, user manual mode labels, and current worklog Phase 11 focus wording.~~
- ~~Improved toolbox-to-canvas insertion UX for structured blocks: dropping outside block bounds now supports explicit document-edge insertion (above first = document start, below last = top-level append at end), and incompatible nested drops now auto-promote to nearest valid ancestor/root scope instead of hard-failing immediately. Also updated Blocks mode button tooltip to remove obsolete “Experimental” wording. Verified with `cmake --build build --target TherionStudio TextEditorCaretInteractionTest` and `QT_QPA_PLATFORM=offscreen ./build/TextEditorCaretInteractionTest`.~~
- ~~Added transient end-pair drag hints in Blocks canvas: while dragging from toolbox or reordering canvas cards, container closing directives (for example `endsurvey`, `endcenterline`) are rendered near container bottoms and cleared on drag end, improving drop-boundary readability without permanent end-cards. Updated spec/manual and verified with `cmake --build build --target TherionStudio TextEditorCaretInteractionTest` and `QT_QPA_PLATFORM=offscreen ./build/TextEditorCaretInteractionTest`.~~
- ~~Fixed transient end-pair hint placement: end markers now anchor to the true visual bottom of each container subtree (container + descendants), not directly below opening cards, so `endsurvey`/`endcenterline` indicate actual closure position during drag operations. Verified with `cmake --build build --target TherionStudio TextEditorCaretInteractionTest` and `QT_QPA_PLATFORM=offscreen ./build/TextEditorCaretInteractionTest`.~~
- ~~Added minimum vertical spacing for transient end-pair hints: drag-time end markers now apply collision-aware Y-offsets (with horizontal-overlap checks) so stacked `end...` labels remain visually separated and readable in dense nested blocks. Verified with `cmake --build build --target TherionStudio TextEditorCaretInteractionTest` and `QT_QPA_PLATFORM=offscreen ./build/TextEditorCaretInteractionTest`.~~
- ~~Corrected end-pair spacing direction: overlapping transient `end...` hints now stack upward (instead of downward) to preserve natural nested closing order and avoid pushing labels below append-drop guides. Verified with `cmake --build build --target TherionStudio TextEditorCaretInteractionTest` and `QT_QPA_PLATFORM=offscreen ./build/TextEditorCaretInteractionTest`.~~
- ~~Corrected end-pair anchor baseline: transient `end...` hints are now anchored above container closure boundaries (not below), then collision-stacked upward, so drop-after-end zones remain unobstructed and visually coherent during drag. Verified with `cmake --build build --target TherionStudio TextEditorCaretInteractionTest` and `QT_QPA_PLATFORM=offscreen ./build/TextEditorCaretInteractionTest`.~~
- ~~Fixed end-marker drop targeting for new block insertion: toolbox drag/drop now treats transient `end...` labels as explicit insertion anchors, so dropping on `endcenterline` inserts after the centerline block and dropping on `endsurvey` inserts after the survey block at parent/root scope. Also aligned preview line rendering for end-marker hovers. Verified with `cmake --build build --target TherionStudio TextEditorCaretInteractionTest` and `QT_QPA_PLATFORM=offscreen ./build/TextEditorCaretInteractionTest`.~~
- Planned focus:
- Expand configurable block coverage (including additional centerline command families and richer command parameters).
- Improve insertion anchoring rules and validation feedback for complex nested source structures.
- Add focused tests for structured-mode source insertion/rewrite safety.
- Planned verification:
- `cmake --build build --target TherionStudio`
- Manual `.th` workflow pass: drag-drop insert, configure, save/reload, and raw-mode round-trip checks

## Post-MVP Backlog (Phase-Linked)

- Phase 3/7: broader GUI automation for structure/inspector/runner/map workflows.
- Phase 4: expand rewrite corpus/regression coverage beyond MVP fixture set for higher corpus-level confidence.
- Phase 8: extended performance benchmarking and packaging convenience improvements beyond baseline release criteria.
- Nice-to-have: extend current `input` relative-path autocomplete semantics (explicit `./` / `../` activation and current-file-relative suggestions) to other path-taking Therion commands/options (for example `-sketch`) for cross-command UX consistency.
- Nice-to-have: completion ranking polish for complex contexts (keep highest-confidence context-valid tokens first and further reduce low-signal fallback entries).

## Test Inventory (Current)

Automated tests currently in-tree and used as regression baseline:

- `CommandCatalogServiceTest`
- `DocumentFileEncodingTest`
- `TherionDocumentEditorTest`
- `TherionProjectStructureIndexTest`
- `MapBackgroundPlacementTest`
- `TherionBackgroundMetadataTest`
- `TherionXviParserTest`
- `MapEditorInputPolicyTest`
- `MapEditorObjectMovePlannerTest`
- `MapEditorDetachedPaneTest`
- `MapEditorDragUndoRedoSmokeTest`
- `ProjectStructureScannerTest`
- `SessionStoreTest`
- `TherionExecutableSelectionControllerTest`
- `TherionRunnerConfigDisplayControllerTest`
- `TherionRunnerConfigResolverTest`
- `TherionRunnerPresenterTest`
- `TherionRunnerServiceTest`
- `TextEditorCaretInteractionTest`
- `TextEditorCompletionHighlightTest`
- `StructureConstraintsTest`

## Recent Completed

Recent dated completion history was moved to `WORKLOG_ARCHIVE_2026-05-23.md`; keep only the latest active checkpoint entries in the top-level `Completed` section above.
