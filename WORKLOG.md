# Worklog

Active work only. Completed history is archived in `WORKLOG_ARCHIVE_2026-05-13.md` and `WORKLOG_ARCHIVE_2026-05-23.md`.

## In Progress

- P0 - Public release stabilization: finish final manual QA, full local build/test pass, and scheduled package workflow monitoring before tagging the first stable public build.
- P0 - Release documentation: keep `docs/USER_MANUAL.md`, `docs/USER_MANUAL.cs.md`, and `docs/USER_MANUAL.sk.md` aligned with current UI behavior, ordinary-user workflows, localized UI names, and Bezier drawing/editing guidance; keep shortcut tables limited to currently verified actions; verify Help -> User Manual resolves localized manuals from installed layouts and renders contents links/readable spacing in-app.
- P1 - Localization hygiene: keep Czech/Slovak UI catalogs complete as strings change, preserve canonical Therion syntax in UI/help, and keep Qt/platform translations participating in application language selection.
- P1 - Therion catalog generator quality: keep command metadata parser aligned with Therion Book syntax, including data-driven `revise` modeling (`<id>` argument + object-option coverage) so syntax/help/completion validation does not require app-side hardcoded exceptions.
- P1 - Block editor polish: keep `.th` versus Therion config command availability filtered by Therion Book document domain, avoid source mutations when opening/switching Blocks mode, and continue logical-line continuation support across edit/configure/delete paths.
- P1 - Map editor parity polish: continue inspector ergonomics, selection/details workflows, map text/source synchronization, and unresolved area/line rewrite edge cases without weakening round-trip safety; keep only intentional line-vertex keybindings (retain Delete/Backspace for selected line-vertex delete with object-delete fallback, remove accidental smooth toggle shortcut), ensure key-driven vertex delete resolves selected line anchors robustly, route Delete/Backspace from map-editor receivers with active map selection (map view, viewport, detached-pane widgets, and macOS focus variants) so key delete remains reliable, suppress map-level delete shortcuts when key focus is in text-entry widgets (for example line-point rows editor and other editable inspector inputs), keep map source text mutations + undo snapshot creation atomic in one command transaction (instead of per-button ad hoc ordering), keep `applySourceTextChangeWithSnapshot` (or equivalent single abstraction) as the required write path for all map-editor source-mutating operations, keep line-vertex delete fully undo/redo-safe, keep selection continuity after vertex delete by reselecting the nearest surviving vertex on the same line and after vertex insert by selecting the newly inserted anchor, keep interactive line/area drawing snap-to-line-vertex behavior across all existing line vertices (not endpoint-only) with explicit hover snap-target marker, keep line-point action buttons resilient when selection temporarily contains anchor + control-handle items, preserve non-coordinate standalone line-point rows from Therion line-data syntax (for example `altitude`, `adjust`, `mark`, `direction point`, `gradient point`) through line-vertex rewrites, expose those standalone line-point rows directly in `Selection` inspector editing (with visible vertex marker + tooltip preview) and auto-apply them on focus-out without extra buttons, require confirmation before deleting a vertex that carries such rows, avoid duplicate rewrite output for recognized dashed slope options (for example `-orientation`, `-size`), keep decorative-line geometry always readable by rendering a subtle guide spine above decoration glyph layers, and ensure touchpad pinch gestures are always routed to zoom (never viewport pan drift).
- P1 - Inspector layout consistency: keep Selection/Objects/Backgrounds/File, Context Help/File, and Block Details/Context Help/File sidebars on the shared `InspectorPanel` / `DocumentInspectorPanel` layout foundation; keep document path/copy-path action/size/last-modified timestamp/encoding/UTF-8 conversion in the shared File inspector tab with filename section headers and a native-styled copy-path button; keep Raw and Blocks contextual help on the shared command-help renderer with command/validation-titled inspector panels and no redundant inner headings; keep Blocks inline details auto-committing without an `Apply` button, with only `Edit Data Rows...` exposed for `data` body editing; keep data-block `Readings Order` tokens rendered as wrapping removable pill chips with add-token input on its own row and chip removal only via explicit `x` controls; preserve the map-inspector-like tabbed/cards presentation; and preserve robust small-height scrolling behavior with one outer tab scrollbar per inspector tab, pixel-aligned tab pane borders, no nested panel background bleed, no accidental palette overrides, and no parent stylesheets flattening native child controls.
- P1 - Map canvas readability and performance: keep the stable light paper canvas in light/dark app modes, keep appearance switches repaint-only, preserve background-layer stack order across appearance/theme refresh, reduce zoomed-out geometry/handle dominance, keep station point-name labels auto-managed by zoom with hysteresis (while preserving selected/hover readability), and keep `.xvi` rendering/background visibility stable.
- P1 - Structure sidebar hardening: maintain lightweight project indexing with Therion namespace/reference semantics, stable refresh signatures, expand/collapse preservation, root-config disambiguation, and navigable diagnostics.
- P1 - Packaging and CI hardening: keep Windows installer, Ubuntu 26.04 `.deb`, and Debian 13 AppImage packaging smoke checks healthy; signing/notarization and APT publishing remain deferred.
- P1 - Architecture maintenance: continue reducing `MainWindow`, `TextEditorTab`, and `MapEditorTab` shell responsibilities through focused controllers/services without behavior changes.

## Next Up

- P0: Run the full local test suite plus `git diff --check` after release stabilization edits.
- P0: Manually smoke-test public-release workflows: open project, edit `.th`, edit `.th2`, map insertion/selection, background layer visibility/Gamma, Blocks mode command filtering, compiler run config, language selection, Help manual, and About dialog.
- P0: Verify daily scheduled Windows installer and Linux package workflows after the final release branch state lands.
- P1: Review localized manual wording during UI smoke testing and adjust only user-facing workflow text.

## Risks / Blockers

- Parser/serializer round-trip coverage is still incomplete for full Therion corpus-level confidence.
- Map/text undo arbitration still depends on choosing between map `QUndoStack` and embedded text-editor undo until a unified document-level undo stack exists.
- Stylus/Sidecar behavior depends on platform input routing; edge cases remain hardware-dependent.
- Signing, notarization, and distribution trust flows are not yet exercised end-to-end on all target platforms.
- Linux packaging depends on Docker-backed local reproduction, Qt's deploy script, explicit Qt plugin/runtime staging, pinned AppImage tooling/runtime inputs, and target-specific smoke expectations.

## Backlog

- Unified document-level undo stack for raw text edits, visual map mutations, inspector changes, background edits, and structured block changes.
- Global project search as a dedicated activity-rail pane.
- Optional Structure graph view alongside the current tree for relationships such as `preview`, `revise`, `join`, `equate`, diagnostics, and station-network edges.
- Compiler-confirmed project-index comparison once lightweight indexing is no longer sufficient.
- Broader Therion corpus regression tests for parsing, serialization, source rewrites, indexing, and map/text synchronization.
- Async/cancellable background raster decode, Gamma/scaling work, and bounded `.xvi` cache policy for very large projects.
- Station-label declutter follow-up: add viewport-space overlap suppression/priority ranking for station point names (in addition to current zoom-hysteresis visibility) and optional user-facing `Auto/All/None` label mode.
- Apple Pencil/freehand stroke UX and shape-sensitive simplification polish.
- Additional map-style catalog tuning and SVG-backed symbol evaluation.
- APT repository publishing, package signing, Windows signing, and macOS notarization.
