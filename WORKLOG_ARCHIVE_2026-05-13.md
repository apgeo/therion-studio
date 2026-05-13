# Worklog

This file records implementation progress in the repository so status does not live only in chat history.

## In progress

- Real project parsing and Therion-specific document handling.
- Text editor features beyond plain text loading and saving.
- Inspector and structure sidebar refinement.
- TH2 map geometry editing behavior beyond the initial draft creation tools.

## Next up

- Expand map-editor source-writeback workflows and validation for complex `line`/`area` editing scenarios.

## Risks / blockers

- Therion parsing and rewrite behavior still need round-trip verification once the parser exists.
- The current text path only handles UTF-8 plain text; non-UTF-8 encodings and syntax-aware editing are not implemented yet.

## Completed

### 2026-05-13

- Hardened coordinate writeback token targeting so quoted numeric strings are excluded from `point` and `line`/`area` vertex rewrites; only unquoted numeric coordinate tokens are rewritten.
- Added `TherionDocumentEditorTest` regression coverage to ensure quoted numeric payloads remain unchanged while editable geometry coordinates are updated.
- Hardened `line`/`area` vertex-token selection so metadata/options are tolerated before coordinates and trailing same-line metadata numeric payloads are no longer interpreted as writable geometry vertices.
- Hardened `line`/`area` vertex-token selection to ignore option-led continuation lines (tokens beginning with option flags) even when those lines contain numeric payload values.
- Hardened geometry numeric-token detection to treat scientific-notation values (for example `1e2`, `-2.5E-1`) as writable coordinate tokens in point and line/area rewrite paths.
- Hardened `line`/`area` vertex-token selection to ignore non-coordinate continuation metadata lines (for example `smooth off ...`) even when they include numeric payload tokens.
- Clarified and locked start-line `line`/`area` rewrite semantics so writable-vertex indexing preserves source token ordering when inline options and numeric runs are mixed on the same line.
- Added `TherionDocumentEditorTest` regression coverage for geometry lines that include metadata both before coordinates and trailing numeric metadata payloads after the real coordinate list.
- Added `TherionDocumentEditorTest` regression coverage ensuring option-led continuation payload lines are excluded from writable-vertex indexing.
- Added `TherionDocumentEditorTest` regression coverage for scientific-notation coordinate rewrite behavior in both `rewritePointCoordinates(...)` and `rewriteLineAreaVertex(...)`.
- Added `TherionDocumentEditorTest` regression coverage ensuring non-coordinate continuation metadata lines with numeric payload are excluded from writable-vertex indexing.
- Added `TherionDocumentEditorTest` regression coverage for mixed start-line option/numeric runs and interleaved `area` metadata continuation lines, verifying only true geometry-continuation coordinate lines are rewritten.
- Reduced map-drag jitter/no-op undo noise by adding a small movement threshold before committing point/vertex map edits back to source or recording undoable operations.
- Routed map point and line/area vertex drag writebacks through explicit map `QUndoCommand` entries so the map `Undo/Redo` buttons now revert and reapply source-coordinate edits deterministically.
- Added undo-command merge behavior for repeated drags on the same point or line/area vertex so micro-adjustment sequences collapse into a single undo/redo step.
- Hardened map geometry drag undo commands to mark failed rewrite operations obsolete so invalid/failing drag commits do not leave empty no-op entries in undo history.
- Added `File -> Close` and `File -> Close All Tabs` commands with unsaved-change prompts and the documented `Command/Ctrl+Shift+W` shortcut for closing all open document tabs.
- Persisted open-document session state immediately after tab-close operations so manual close/close-all actions are reflected in restored sessions.
- Verified `cmake --build build --target TherionDocumentEditorTest TherionStudio` succeeds and the current regression binaries (`TherionDocumentEditorTest`, `TherionProjectStructureIndexTest`, `MapBackgroundPlacementTest`, `TherionBackgroundMetadataTest`, `TherionXviParserTest`) all pass.
- Fixed metadata-loaded PNG sizing by treating raster pixel dimensions as TH2 image units; the third `xth_me_image_insert` numeric value is preserved as metadata and is not used to resize the raster layer.
- Corrected raster metadata placement to keep `xth_me_area_adjust` as editor padding/viewport metadata unless it exactly matches the raster dimensions, preventing `{0,0}` anchors from shifting scanned sketches away from TH2 geometry.
- Hardened metadata matching for background layers by normalizing canonical/absolute paths (including Unicode composition differences on macOS) and adding a unique filename fallback when restoring from session.
- Reapplied metadata anchoring to already-loaded raster layers during background sync so stale saved pixel positions no longer override TH2 metadata placement.
- Removed the metadata-sync early exit for non-empty background sets so session-restored layers are still reconciled against current TH2 metadata on load.
- Preserved exact placed image dimensions when opacity/gamma refreshes rebuild the pixmap.
- Extracted raster model-rect resolution into `src/core/MapBackgroundPlacement.*` so metadata placement semantics are shared and unit-testable outside the UI layer.
- Added `MapBackgroundPlacementTest` regression coverage for clopy-style fallback anchoring, area-adjust size-match behavior, and offset anchor placement.
- Extracted TH2 metadata parsing into `src/core/TherionBackgroundMetadata.*` so `xth_me_image_insert` and `xth_me_area_adjust` behavior is reusable and independently testable.
- Added `TherionBackgroundMetadataTest` coverage for raster metadata parsing, `.xvi` root-station parsing, area-adjust normalization, and malformed metadata rejection.
- Extracted `.xvi` parsing into `src/core/TherionXviParser.*` so background vector parsing is no longer embedded in `MapEditorBackgroundLayers.cpp`.
- Added `TherionXviParserTest` coverage for valid grid/station/shot/sketch parsing, file-based parsing, and invalid-content rejection.
- Verified the full `cmake --build build` target set succeeds and `TherionDocumentEditorTest`, `TherionProjectStructureIndexTest`, `MapBackgroundPlacementTest`, `TherionBackgroundMetadataTest`, and `TherionXviParserTest` pass after parser extraction.
- Expanded `TherionDocumentEditorTest` coverage for `rewriteLineAreaVertex(...)` with CRLF-preservation, mixed inline/multiline vertex indexing, missing `endline` rejection, and odd-coordinate-pair rejection scenarios.
- Verified the updated `TherionDocumentEditorTest` target build succeeds and all current test binaries (`TherionDocumentEditorTest`, `TherionProjectStructureIndexTest`, `MapBackgroundPlacementTest`, `TherionBackgroundMetadataTest`, `TherionXviParserTest`) pass.

### 2026-05-13

- Removed the extra `scrap -scale` transform from TH2 map preview geometry so parsed points, lines, and areas render in the same global model coordinate space as PNG background layers.
- Kept map drag writeback in raw TH2 model coordinates, matching the background alignment model where scraps provide ownership/grouping rather than spatial transforms.
- Verified the full `cmake --build build` target set succeeds and both `TherionDocumentEditorTest` and `TherionProjectStructureIndexTest` pass after removing the scrap transform from preview geometry.

### 2026-05-13

- Fixed `.xvi` background cropping/alignment drift by deriving the rendered layer bounds from full `XVIgrid` extents (origin + grid basis vectors + node counts) instead of only shot/sketch polyline extents.
- Improved `Fit` behavior for vector backgrounds because full grid-area bounds now participate in background layer sizing.
- Verified the full `cmake --build build` target set succeeds and both `TherionDocumentEditorTest` and `TherionProjectStructureIndexTest` pass after the `.xvi` grid-bounds fix.

### 2026-05-13

- Fixed persistent misalignment from older session state by reapplying `xth_me_image_insert` metadata anchoring when restoring background layers from session (instead of trusting stale saved pixel positions).
- Restored `.xvi` session layers using metadata base position/root station when available so vector overlays and scrap geometry share the same model-space anchoring rules after reopen.
- Improved map readability by reducing point/vertex handle radii, line thickness, and overlay alpha so dense geometry no longer obscures background sketches.
- Verified the full `cmake --build build` target set succeeds and both `TherionDocumentEditorTest` and `TherionProjectStructureIndexTest` pass after the alignment/readability adjustments.

### 2026-05-13

- Reworked map preview coordinate projection to use explicit model-space pan/zoom semantics with inverted Y (`viewX = modelX * zoom + panX`, `viewY = panY - modelY * zoom`) for scene mapping and source rewrite conversion.
- Fixed `xth_me_image_insert` metadata parsing to resolve base position from the two payload groups and keep optional root-station metadata for `.xvi` references.
- Fixed raster metadata background placement to use model-space anchoring with top-edge behavior (`topLeft = (x, y - imageHeight)`) instead of center anchoring.
- Added `.xvi` background rendering support by parsing `XVIgrid`, `XVIstations`, `XVIshots`, and `XVIsketchlines`, applying offset alignment (`offset = anchoredPosition - gridOrigin`), and drawing vector overlays as map background layers.
- Implemented optional `.xvi` root-station anchoring so provided station names shift the anchored base position before applying the global offset.
- Extended session restore to rebuild `.xvi` background layers in addition to raster image layers.
- Verified the full `cmake --build build` target set succeeds and both `TherionDocumentEditorTest` and `TherionProjectStructureIndexTest` pass after background-alignment fixes.

### 2026-05-13

- Extended direct map-to-source geometry editing from points to parsed `line` and `area` vertices by adding draggable vertex handles in the map preview.
- Added transform-aware source rewrite routing for line/area vertex moves so edits stay correct when scrap `-scale` mappings are active.
- Added `TherionDocumentEditor::rewriteLineAreaVertex(...)` with token-span replacement across multi-line `line`/`area` blocks, preserving surrounding text and comments.
- Added focused regression coverage for line/area vertex rewrite success/failure paths in `TherionDocumentEditorTest`.
- Verified the full `cmake --build build` target set succeeds and both `TherionDocumentEditorTest` and `TherionProjectStructureIndexTest` pass after the line/area rewrite integration.

### 2026-05-13

- Added automatic background-layer discovery from TH2 metadata by parsing `##XTHERION## xth_me_image_insert` lines when a map document has no restored background session state.
- Resolved metadata-referenced image paths relative to the active TH2 document and auto-loaded matching raster files into the map workspace.
- Applied metadata anchor coordinates to initial background placement in the preview when geometry bounds are available.
- Updated map help text to document metadata-driven background auto-loading behavior.
- Verified the full `cmake --build build` target set succeeds after metadata background auto-load integration.

### 2026-05-13

- Added TH2 scrap `-scale` transform support in map-geometry extraction so parsed point/line/area coordinates render in mapped workspace coordinates instead of raw local scrap coordinates.
- Preserved direct point-drag source rewrites under transformed rendering by applying inverse scrap transform before committing coordinate edits back to source.
- Verified the full `cmake --build build` target set succeeds after scrap-scale transform integration.

### 2026-05-13

- Reduced TH2 preview clutter by switching geometry rendering to scale-aware stroke/marker widths instead of fixed heavy line/dot sizes.
- Limited map preview labels in dense scenes (station-priority labeling with compact mode for large feature sets) to keep geometry readable.
- Updated Map Help text to match current behavior (point-handle source rewrite and draft-object focus) and removed stale references to map cards.
- Verified the full `cmake --build build` target set succeeds after the preview readability refinements.

### 2026-05-13

- Fixed window scaling pressure by making the TH2 map toolbar horizontally scrollable instead of forcing all controls into a fixed-width row.
- Relaxed minimum-width constraints in map and console status labels (`Ignored` horizontal size policy + wrapping where appropriate) so long paths/status text no longer force oversized windows.
- Updated the Therion console form layout to wrap long rows on narrow widths.
- Verified the full `cmake --build build` target set succeeds after the window-scaling layout fixes.

### 2026-05-13

- Fixed TH2 map preview deformation by switching geometry projection from independent X/Y scaling to a uniform aspect-fit transform.
- Applied the same aspect-fit projection to editable point drag/write-back and draft geometry preview-to-source mapping so edits stay aligned with rendering.
- Removed in-canvas map object card rendering from the map scene so the temporary large card boxes no longer appear in the preview area.
- Verified the update builds successfully and both test binaries run successfully in this environment.

### 2026-05-13

- Added the TH2 map editor shell as a wrapper around the existing text editor with workspace mode switching and a placeholder map canvas.
- Routed TH2 documents through dedicated map tabs while keeping text documents on the existing text editor tab.
- Extended session persistence and tab lifecycle handling so open documents, active documents, save actions, and close prompts work across both text and map tabs.
- Made the Map menu action disable itself unless a TH2 document is active.
- Replaced the TH2 placeholder canvas with a parsed scene of selectable map-object cards driven from the live document text.
- Added movable TH2 map cards with per-object visibility toggles.
- Added TH2 map zoom controls plus undo/redo command actions for card moves and visibility changes.
- Verified the map editor shell and document-routing changes build successfully in this environment.

### 2026-05-13

- Added the root [AGENTS.md](AGENTS.md) instructions file for this specification-first project.
- Added initial guidance for file size, file splitting, best practices, spec alignment, verification, and error handling.
- Created the first Qt Widgets application scaffold with CMake build setup, an application entry point, and a shell main window.
- Added the first core document I/O helper and a reusable text editor tab widget.
- Wired the main window to open project folders, browse filesystem contents, open text files in tabs, and save the active or all open documents.
- Fixed initial tab loading so opening a file does not mark it dirty during load.
- Added basic session persistence for the last project path and main window geometry/state.
- Added dirty-tab close prompts and quit-time unsaved-change handling.
- Verified the scaffold and the session persistence changes build successfully in this environment.
- Verified the dirty-tab close prompt implementation builds successfully.
- Added a JSON-backed syntax highlighting palette and attached it to the text editor tabs.
- Verified the syntax highlighting slice builds successfully.
- Implemented Close Project so it clears project-scoped tabs and resets the project browser root.
- Verified the Close Project implementation builds successfully.
- Switched the main app target to build as a standard macOS `.app` bundle.
- Verified the macOS `.app` bundle build successfully.
- Added an inline find/replace bar to the text editor and wired the Find/Replace menu actions to it.
- Verified the inline find/replace bar builds successfully.
- Added a first project-structure scanner and wired the structure sidebar to show Therion items from the open project.
- Verified the structure sidebar scanner builds successfully.
- Made structure sidebar selection update the inspector and jump to the referenced source line in the editor.
- Verified the structure selection integration builds successfully.
- Upgraded the inspector to show selection-specific structure details and an open-source action.
- Verified the inspector upgrade builds successfully.
- Added a reusable Therion line parser and switched the structure scan to use parsed tokens instead of raw regex matching.
- Verified the parser-backed structure scan builds successfully.
- Switched Therion syntax highlighting to parser-backed token spans and keyword metadata.
- Verified the parser-backed syntax highlighting build successfully.
- Added a collapsible contextual help panel backed by structured Therion command metadata.
- Verified the contextual help panel builds successfully.
- Made the structure sidebar hierarchical with a project summary root and category counts.
- Improved the inspector empty state to show a project-level structure summary.
- Added category-aware inspector labels and object-kind summaries for selected structure items.
- Added an editable inspector name field with validation and in-memory apply support.
- Made structure-sidebar container rows toggle and drill down to the first navigable object.
- Persisted inspector name overrides as project-scoped JSON session data.
- Added richer project-derived metadata fields to the inspector.
- Added source-to-model navigation that follows the editor cursor to the nearest structure object.
- Made the structure sidebar show by default again and added a View menu toggle for it.
- Persisted inspector edits back into the underlying Therion source document.
- Added a Therion runner console dock with async process execution, stdout/stderr capture, and persisted runner settings.
- Verified the Therion document rewrite path preserves unrelated content and line endings.
- Verified the Therion document rewrite path preserves quoted station names, inline comments, and point-station entries.
- Verified the Therion document rewrite path covers the writable survey, map, scrap, line, area, point, and station directives.
- Verified the Therion document rewrite path rejects comment-only lines, out-of-range line numbers, and unsupported categories without mutating the source.
- Verified the Therion document rewrite path also rejects percent-comment-only lines and blank lines without mutating the source.
- Verified the Therion document rewrite path escapes backslashes and hash characters in rewritten names.
- Verified the Therion document rewrite path preserves tab-indented lines and double-quoted names.
- Verified the Therion document rewrite path preserves trailing token noise and normalizes unterminated quoted names.
- Verified the Therion document rewrite path preserves the station-specific token layout under tab-indented formatting.
- Verified the Therion document rewrite path escapes apostrophes in single-quoted rewritten names.
- Stopped the project browser from rooting itself at the user home directory on startup to avoid macOS privacy prompts.
- Verified the startup privacy-prompt fix builds successfully.
- Skipped automatic project restore when the saved project is inside a macOS protected user folder.
- Verified the protected-folder startup restore guard builds successfully.
- Fixed the app shutdown crash by making the primary main window heap-owned instead of stack-owned.
- Improved source navigation so line jumps select the referenced line instead of only moving the caret.
- Verified the source-line selection change builds successfully.
- Added persistence for open editor tabs and the active document path.
- Verified the open-tab session restore slice builds successfully.
- Added a persistent document status strip at the bottom of the editor showing the active path and UTF-8 encoding.

### 2026-05-13

- Added direct TH2 map draft-geometry creation tools for point, line, and area objects in the scene.
- Added draft-geometry completion handling plus undo/redo commands for creating and editing draft objects.
- Preserved draft geometry across map-scene refreshes and cleared stale drafts when switching files.
- Verified the TH2 map editor and draft geometry tools build successfully in this environment.

### 2026-05-13

- Added a TH2 geometry preview layer that renders parsed point, line, and area geometry above the object cards.
- Rendered point anchors, line paths, and area fills from parsed source geometry when available.
- Kept the existing object cards as a secondary object list so the source metadata remains visible alongside the geometry preview.
- Verified the geometry preview update builds successfully in this environment.

### 2026-05-13

- Restyled the TH2 geometry preview toward a white map canvas with black linework, point markers, and subdued labels.
- Added explicit station-point markers so the rendered preview reads more like a real map sketch.
- Verified the preview restyle builds successfully in this environment.

### 2026-05-13

- Switched the TH2 workspace to a horizontal side-by-side text and map split to better match the original SwiftUI layout.
- Verified the workspace layout update builds successfully in this environment.

### 2026-05-13

- Combined the TH2 object settings and object tree into a single left sidebar dock with a vertical split.
- Added explicit "Object Settings" and "Objects" headers to better match the original SwiftUI workspace structure.
- Updated the View menu to toggle the combined sidebar instead of a separate inspector placeholder.
- Verified the sidebar refactor builds successfully in this environment.

### 2026-05-13

- Added a collapsible Map Help panel below the TH2 text/map workspace.
- Made the help panel update for selected map cards and draft geometry items.
- Verified the help-panel addition builds successfully in this environment.

### 2026-05-13

- Tightened the TH2 combined sidebar spacing and hierarchy by reducing padding, adding section dividers, and hiding the redundant tree header.
- Gave the object list a clearer section introduction so the sidebar reads more like two explicit panels.
- Verified the sidebar polish builds successfully in this environment.

### 2026-05-13

- Polished the Map Help panel with lighter chrome, tighter margins, and clearer selection guidance.
- Added a contextual subtitle and cleaner help browser framing so the footer reads more like documentation.
- Verified the help-panel polish builds successfully in this environment.

### 2026-05-13

- Reworked the left sidebar into a switchable three-pane layout with top controls for File, Structure, and Map.
- Restored the Map pane as a combined object-settings and objects browser view while keeping structure selection synchronized across panes.
- Verified the corrected switchable sidebar layout builds successfully in this environment.

### 2026-05-13

- Enabled the native macOS Qt style explicitly on macOS builds.
- Replaced the custom sidebar mode buttons with a native tab-bar switcher and reduced extra sidebar chrome on macOS.
- Relaxed custom help-panel styling on macOS so more panels can render with platform-native appearance.
- Verified the macOS style pass builds successfully in this environment.

### 2026-05-13

- Generalized platform style selection at startup so macOS prefers the macOS native style, Windows prefers the Windows native style, and Linux prefers Fusion.
- Added platform-specific fallback style names where Qt exposes alternate native style keys.
- Verified the cross-platform style selection update builds successfully in this environment.

### 2026-05-13

- Made the sidebar mode switcher icon-only and replaced the generic style icons with custom vector icons for file, structure, and map modes.
- Styled the macOS sidebar switcher closer to an NSSegmentedControl with a rounded track and selected-segment highlight.
- Verified the segmented sidebar switcher update builds successfully in this environment.

### 2026-05-13

- Disabled the sidebar Map pane for non-TH2 documents and made the switcher fall back to the last non-map pane when the active tab stops supporting map editing.
- Reused the same TH2-only document check for both the sidebar switcher and the map-editor command state.
- Verified the TH2-only Map pane gating builds successfully in this environment.

### 2026-05-13

- Made the left sidebar explicitly closable from its own header row and restore its last expanded width when shown again.
- Kept the dock resizable while visible and persisted the current width on collapse.
- Verified the left sidebar resize and collapse update builds successfully in this environment.

### 2026-05-13

- Split the Structure browser and Map Editor object list into separate parser-backed models instead of reusing one shared tree.
- Changed the Structure browser to show project `.th` and `.th2` hierarchy by file, surveys, maps, and scraps.
- Changed the Map Editor object list to show only objects from the current TH2 document, grouped under scraps.
- Updated the map-object rename path to handle option-backed TH2 identifiers such as station names and line IDs.
- Verified the structure/object model split builds successfully in this environment.

### 2026-05-13

- Extended the project structure hierarchy to include stations, points, lines, and areas under the appropriate file and section nodes.
- Kept the project summary aligned with the richer structure tree categories.
- Verified the hierarchy refinement builds successfully in this environment.

### 2026-05-13

- Reworked the Structure browser scanner to follow Therion survey hierarchy instead of file roots.
- Expanded `input` commands inline so included `.th` and `.th2` content is merged into the survey tree at the command location.
- Added namespace-aware survey labels and a regression test for nested survey and input traversal.
- Verified the survey-hierarchy update builds and passes the new regression test in this environment.

### 2026-05-13

- Added explicit repository guidance for splitting large translation units by responsibility instead of by line count.
- Clarified that orchestration shells should stay thin and that extracted behavior should be backed by focused tests.

### 2026-05-13

- Restored the split `MainWindow` shell by reintroducing the constructor and UI bootstrap methods.
- Moved `openCurrentDocumentInMapEditor()` back into the compiled portion of `MainWindow.cpp` so the menu action links again.
- Verified the `TherionStudio` app target links successfully after the source split recovery.
- Split the MainWindow sidebar, structure tree, and inspector logic into a dedicated `MainWindowSidebar.cpp` translation unit.
- Kept the main window source focused on app orchestration and document/tab workflow.
- Moved sidebar UI-construction methods (`buildProjectBrowser`, `buildStructureSidebar`, `buildInspector`, `setSidebarPane`) from `MainWindow.cpp` into `MainWindowSidebar.cpp`.
- Removed the stale `#if 0` compatibility block from `MainWindow.cpp` now that equivalent logic lives in compiled split units.
- Verified the full `cmake --build build` target set succeeds after the additional split.
- Split `MapEditorTab` workspace/document-shell behavior into a new `MapEditorTabWorkspace.cpp` translation unit.
- Moved constructor, UI bootstrap, workspace-mode persistence, tab/document wrappers, and status updates out of `MapEditorTab.cpp`.
- Kept `MapEditorTab.cpp` focused on map scene rendering, selection interaction, draft geometry commands, and geometry preview helpers.
- Verified the full `cmake --build build` target set succeeds after the `MapEditorTab` split.
- Added a `Copy Output` action to the Therion console command row so full console output can be copied in one click.
- Added clipboard handling and empty-output feedback for the Therion console copy flow.
- Verified the full `cmake --build build` target set succeeds after the console copy-output update.
- Added explicit Therion console fields for active config name and resolved config path.
- Added config-path resolution from `--config`/`-c` Therion arguments with fallback to `thconfig` in the active working context.
- Made config display refresh live when project root, working directory, or Therion arguments change.
- Verified the full `cmake --build build` target set succeeds after the console config-surface update.
- Extended auto-detection to treat the active `.thconfig` editor tab as the Therion config context when no explicit `--config` argument is provided.
- Auto-synced the Therion working directory to the resolved auto-detected config directory when the current working directory is still auto-managed (empty, project root, or previously auto-resolved).
- Kept explicit `--config` argument workflows non-destructive by skipping automatic working-directory overrides in that mode.
- Added Therion executable preflight resolution before process start (absolute path, working-directory relative path, or `PATH` lookup via Qt).
- Added an actionable runner warning when the executable is missing or non-executable instead of only surfacing the later `execve` failure.
- Added Homebrew-aware Therion binary discovery using `HOMEBREW_PREFIX`, `/opt/homebrew/bin/therion`, and `/usr/local/bin/therion`.
- Made the console executable field default to detected Homebrew Therion when no persisted executable path exists.
- Added a Homebrew fallback in runner preflight resolution for the default `therion` executable name.
- Added a `Browse...` button next to the Therion executable field for selecting custom executable paths.
- Validated browse selections as executable files before applying them.
- Reused existing session persistence so the selected executable path is saved and restored via app config.
- Added an explicit Therion run-policy field in the console surface: reject parallel runs while a process is active.
- Added clear status/console messaging when a run is requested while Therion is already running.
- Logged the run-policy rule in console startup output for deterministic behavior visibility.
- Added a dedicated `Map` menu and moved the "Open Current Document in Map Editor" command there.
- Assigned the documented map command shortcut (`Command/Ctrl+Option/Alt+Shift+G`) to "Open Current Document in Map Editor".
- Added an explicit collapse control to the Therion console panel header and restored the last expanded console height when re-shown.
- Made panel expand controls explicit in the View menu with `Show Sidebar` and `Show Console` toggle actions.
- Updated the specification and acceptance criteria to require explicit collapse/re-expand controls for both sidebar and console panels.
- Verified the full `cmake --build build` target set succeeds after the panel-collapse/expand update.
- Replaced in-panel collapse buttons with triangle collapse/expand toggles in the sidebar and console dock title bars (window handle area).
- Changed panel collapse behavior from hide-only to in-place collapse/expand, so the same triangle control can uncollapse the panel.
- Kept View menu toggles (`Show Sidebar`, `Show Console`) as an additional explicit show/hide path.
- Verified the full `cmake --build build` target set succeeds after the dock-title-bar triangle collapse update.
- Tightened collapsed sidebar/console rails to compact icon-only state by hiding title text when collapsed.
- Reduced collapsed rail footprint to a small fixed size so the editor keeps more usable width/height.
- Verified the full `cmake --build build` target set succeeds after the compact collapsed-rail refinement.
- Split map-scene support types out of `MapEditorTab.cpp` into `MapEditorSceneSupport` companion files.
- Moved map-card and draft-geometry graphics item classes, draft/card undo commands, and geometry parsing helpers into `MapEditorSceneSupport.cpp`.
- Added multi-background image support in the TH2 map editor via multi-select image import.
- Added per-layer background controls for ordering, visibility toggle, and opacity adjustment in the map toolbar.
- Persisted background layer stacks (path, visibility, opacity, order) per TH2 document in session settings and restored them on reopen.
- Added explicit `Freehand` and `Smart Trace` actions to the TH2 map toolbar to satisfy command-surface coverage requirements.
- Wired `Freehand` and `Smart Trace` to the current draft-line workflow so each action inserts a line draft that can be completed through `Complete Draft`.
- Verified the current tree builds successfully with `cmake --build build`.
- Verified regression smoke tests by running `./build/TherionDocumentEditorTest` and `./build/TherionProjectStructureIndexTest`.
- Rewired `MapEditorTab.cpp` to use scene-support command factories and shared geometry helpers without changing user-facing behavior.
- Verified the full `cmake --build build` target set succeeds after the `MapEditorTab` scene-support split.
- Moved the Map Help panel under the map canvas inside the map-side workspace pane.
- Changed the map side to a vertical splitter so map canvas and Map Help are resizable and can collapse/expand independently.
- Kept TextEditor contextual help behavior unchanged, so in Split mode both help panes now sit side by side and each can be shown/hidden from its own header control.
- Verified the full `cmake --build build` target set succeeds after the map-help workspace layout update.
- Aligned the `Map Help` panel chrome to match the `Contextual Help` panel structure (header row plus shared browser body style).
- Replaced the map-tab footer label with a single shared status bar below the full split workspace that shows document path and encoding.
- Hid the nested `TextEditorTab` inline status row when embedded in the map workspace so status is no longer duplicated.
- Verified the full `cmake --build build` target set succeeds after the help-panel and shared-status-bar alignment update.
- Added a styled frame border around the `Contextual Help` panel so it visually matches `Map Help`.
- Verified the full `cmake --build build` target set succeeds after the contextual-help border alignment update.
- Moved help collapse controls from in-panel `Hide` buttons to small triangle toggles on the help-pane splitter borders for both text and map workspaces.
- Made both help panes collapse/expand directly from the border toggle by driving splitter sizes to and from zero-height help-pane states.
- Verified the full `cmake --build build` target set succeeds after the help-border toggle migration.
- Extracted TH2 scene-entry collection and the full map workspace render pass out of `MapEditorTab::refreshMapScene()` into `MapEditorSceneSupport` helpers.
- Kept `MapEditorTab` focused on orchestration by delegating geometry/card rendering to shared scene-support functions while preserving current visuals and interactions.
- Verified the full `cmake --build build` target set succeeds after the map-scene rendering split.
- Added `Select`, `Insert Scrap`, and `Fit + BG` actions to the TH2 map toolbar as explicit command-surface parity placeholders.
- Wired deterministic placeholder messaging in the toolbar summary/help so unfinished actions are visible without failing silently.
- Extended Map Help metadata to document the new placeholder actions and current fallback behavior.
- Verified the full `cmake --build build` target set succeeds after the map-toolbar parity placeholder update.
- Implemented real `Insert Scrap` behavior so the toolbar action appends a unique `scrap ... endscrap` block into the active TH2 source.
- Added `TherionDocumentEditor::appendScrapBlock` with name sanitization, uniqueness suffixing, CRLF-preserving append, and inserted-line reporting.
- Added focused regression coverage for `appendScrapBlock` (null guard, default insertion, unique naming, sanitization, line-number reporting).
- Fixed station-name rewrite targeting for `point station ...` lines so station identifier edits no longer replace the `station` type token.
- Verified `./build/TherionDocumentEditorTest` passes after the rewrite-target fix and scrap-append tests.
- Verified the full `cmake --build build` target set succeeds after the map scrap-insertion implementation.
- Added multi-image background support in the TH2 map workspace via `Add BG` and `Clear BG` toolbar actions.
- Made the map scene preserve loaded background layers across source reparses while clearing them when switching to a different TH2 file.
- Implemented real `Fit + BG` behavior that fits the viewport to geometry plus all loaded background layer bounds.
- Updated toolbar summary and map help text to report active background-layer count and the new background workflow.
- Verified `./build/TherionDocumentEditorTest` and the full `cmake --build build` target set succeed after multi-background support.
- Moved background image management out of the map toolbar into a dedicated `Background Images` panel in the Map sidebar.
- Added sidebar controls for layer add/remove/reorder/visibility plus per-layer `X`/`Y` position editing and nudge arrows.
- Added sidebar slider controls for per-layer opacity and gamma adjustments with reset actions.
- Extended background-layer session persistence to include per-layer position and gamma alongside path, visibility, opacity, and stack order.
- Updated map help text to point background-layer workflows to the Map sidebar panel.
- Verified `./build/TherionDocumentEditorTest` and the full `cmake --build build` target set succeed after the Map-sidebar background-management move.
- Replaced the top sidebar mode tabs with a persistent left activity rail (`Files`, `Structure`, `Map`, `Console`) that remains visible while panes switch.
- Changed sidebar layout so the selected pane opens to the right of the activity rail, matching the requested VS Code-style interaction model.
- Added a dedicated Console pane to the sidebar page stack and moved the Therion runner surface into that pane.
- Updated sidebar collapse behavior to hide only pane content while keeping the activity rail visible for quick re-entry.
- Kept the View menu `Show Console` action by remapping it to switch the sidebar into the Console pane.
- Verified `./build/TherionDocumentEditorTest` and the full `cmake --build build` target set succeed after the activity-rail sidebar update.
- Removed per-component sidebar/background-panel stylesheet overrides so the new activity rail and map background panel inherit the shared app theme defined in `main.cpp`.
- Verified the full `cmake --build build` target set succeeds after removing local component styling overrides.
- Removed the sidebar title-bar collapse/expand button to keep sidebar control centralized in the activity rail.
- Implemented activity-rail click toggling: clicking the active rail item collapses the sidebar content area, and clicking any rail item while collapsed expands the sidebar and opens that pane.
- Verified the full `cmake --build build` target set succeeds after the sidebar toggle-control update.
- Removed the sidebar dock header row so the `Sidebar` title is no longer shown above the activity rail.
- Verified the full `cmake --build build` target set succeeds after removing the sidebar header.
- Implemented source-write flow for map draft completion by appending TH2 geometry directives into scrap context.
- Added `TherionDocumentEditor::appendDraftGeometry` with fallback scrap creation, CRLF-preserving insertion, and per-kind validation for point/line/area minimum vertex counts.
- Wired `MapEditorTab::Complete Draft` to convert scene draft geometry into source coordinates and commit via `TextEditorTab` into the active TH2 document.
- Added regression coverage for draft-geometry append behavior, including null guard, insertion into existing scrap, CRLF fallback scrap creation, and validation failures.
- Verified `cmake --build build`, `./build/TherionDocumentEditorTest`, and `./build/TherionProjectStructureIndexTest` succeed after draft-to-source integration.
- Added `TherionDocumentEditor::rewritePointCoordinates` for in-place coordinate rewrites on existing `point` / `station` source lines.
- Wired parsed point markers in the map preview as draggable geometry handles that commit coordinate updates back to TH2 source on release.
- Added `TextEditorTab::rewritePointCoordinates` and map-tab callback plumbing so point drags mark documents dirty and trigger scene reparse/sync.
- Added regression coverage for point-coordinate rewrite behavior (null guard, non-point rejection, point/station rewrite success).
- Verified `cmake --build build`, `./build/TherionDocumentEditorTest`, and `./build/TherionProjectStructureIndexTest` succeed after in-place point-edit integration.

### 2026-05-13

- Split all translation units exceeding 1000 lines by responsibility so no source file exceeds the project line-count guideline.
- Extracted inspector logic (`buildInspector`, `updateInspectorFromStructureItem`, name-edit handlers, source-open navigation) into `MainWindowInspector.cpp`.
- Extracted structure-browser and map-object-tree logic into `MainWindowStructureBrowser.cpp` with a shared `MainWindowStructureRoles.h` header for role constants.
- Extracted map background-panel UI construction and refresh logic into `MainWindowMapBackground.cpp`.
- Extracted Therion runner logic (console build, process management, config resolution, Homebrew detection) into `MainWindowTherionRunner.cpp`.
- Extracted background-layer management (session persistence, Xtherion metadata parsing, layer controls) from `MapEditorTab.cpp` into `MapEditorBackgroundLayers.cpp`.
- Split `MapEditorSceneSupport.cpp` into `MapEditorSceneItems.cpp` (item classes, undo commands, coordinate helpers, factory functions) and `MapEditorSceneRenderer.cpp` (entry categorization, workspace renderer, geometry feature collection).
- Introduced `MapEditorSceneInternals.h` as a shared internal header to expose coordinate helpers and `MapEditablePointItem` across the two scene translation units without polluting the public API.
- Removed dead code: `DockTitleBarWidget` and four `consoleDock_` methods that were unreachable because `consoleDock_` was never assigned.
- Updated `CMakeLists.txt` to reference all new translation units and internal headers.
