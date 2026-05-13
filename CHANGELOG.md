# Worklog

This file records implementation progress in the repository so status does not live only in chat history.

## In progress

- Real project parsing and Therion-specific document handling.
- Text editor features beyond plain text loading and saving.
- Inspector and structure sidebar refinement.
- TH2 map geometry editing behavior beyond the initial draft creation tools.

## Next up

- Remove the temporary `#if 0` compatibility fence in `MainWindow.cpp` now that the extracted translation units are linked.

## Risks / blockers

- Therion parsing and rewrite behavior still need round-trip verification once the parser exists.
- The current text path only handles UTF-8 plain text; non-UTF-8 encodings and syntax-aware editing are not implemented yet.

## Completed

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