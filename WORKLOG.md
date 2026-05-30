# Worklog

Active work only. Completed history is archived in `WORKLOG_ARCHIVE_2026-05-13.md` and `WORKLOG_ARCHIVE_2026-05-23.md`.

## In Progress

- P0 - Phase 9: continue map-editor parity polish, focusing on remaining interaction gaps, inspector ergonomics, and unresolved area/line ownership rewrite semantics.
- P1 - Architecture roadmap: Phase 2 is complete (session/project/runner workflow extraction from MainWindow); Phase 3 is complete (`TextEditorTab` coupling reduced via document/mode/bootstrap/status/cursor/source-rewrite/raw-find/block-details/context-help/encoding delegate splits); Phase 4 is complete with map-editor shell decomposition across scene-refresh lifecycle, selection/inspector workflows, source-edit workflows, event routing, line-extension planning, workspace/document/status delegates, detached-pane lifecycle/window shell, inspector-panel UI delegates, inspector/object-details controller delegates, and grouped map-editor state surfaces in `MapEditorTab.h`; Phase 5 is complete with centralized startup/bootstrap entrypoints, Lucide icon theming, shared workspace command-bar styling, shared main-window/sidebar/status/detached-pane appearance policy (`src/app/ApplicationStylePolicy.h`), and centralized map-canvas scene theme policy (`src/app/text_editor/map_editor/MapEditorSceneThemePolicy.h`); Phase 6 is complete for the current packaging/CI hardening scope with install-layout smoke checks, scheduled Windows installer artifact naming+manifest verification with `dev-<short_sha>` snapshot labels, Linux `amd64`/`arm64` `.deb` and `x86_64`/`aarch64` AppImage packaging scripts consolidated under `scripts/linux-packages/`, Docker-backed local reproduction/smoke wrappers, Ubuntu 26.04 `.deb` smoke checks, and Debian 13 plus Ubuntu 26.04 AppImage smoke checks. APT publishing is deferred and signing/notarization remains later release-readiness work.
- P1 - Repository review follow-up: rationalize trivial `TextEditorTab*` context-factory translation units, consolidate remaining direct Lucide SVG rendering through `LucideIconFactory`, and remove compatibility-only headers such as `WorkspaceCommandBarStyle.h` when include users are stable.
- P1 - Localization hygiene: keep Czech/Slovak catalogs complete as new UI strings are added, maintain `docs/LOCALIZATION.md`, avoid translating canonical Therion command syntax/accepted values/examples, and reduce remaining non-`QObject` translation-helper extraction warnings when touching those controllers.
- P1 - Project index hardening: evolve the current structure sidebar scanner into a core `ProjectIndexSnapshot` with reusable object/reference data, stable source-backed object IDs, enum-backed object kinds, project-root-scoped `.thconfig`/`source` file graph traversal, target-config disambiguation for roots with multiple `*.thconfig` files, map-to-scrap reference resolution plus unresolved-reference diagnostics outside UI widgets, structure-sidebar snapshot application guarded by a structural signature to avoid refresh churn while typing, and continued work toward compiler-confirmed snapshots.
- P1 - MapEditorTab decomposition: keep line-extension workflow in a focused companion translation unit and continue reducing map-editor shell size without changing user-visible behavior.
- P1 - Phase 10: continue interactive drawing/insertion polish, focusing on Apple Pencil/freehand performance and remaining insertion edge cases.
- P2 - Phase 11: continue structured block-canvas authoring coverage and BlockEditor extraction/refactor slices.
- P1 - Block-canvas drag-loop crash hardening on macOS: guard drag-preview/drop callbacks during TextEditorTab teardown and verify no stale callback path remains around `.th2` + `.xvi` workflows.
- P1 - Block editor logical-line continuation support: treat trailing `\` command continuations as one command block across parsing/details/apply/configure/delete paths and verify with UI regression coverage.
- P1 - Map canvas zoom-level rendering balance: keep zoomed-out overview readable by reducing dominant geometry and `.xvi` stroke/handle visual weight without hurting editability at normal zoom, remove the unreliable editor-generated metric grid, and render embedded `.xvi` grids only as background-layer content.
- P1 - Background layer visibility persistence: keep manual `.xvi` show/hide state stable across zoom/reprojection refreshes and session reloads without treating old session `visible` values as metadata overrides.
- P1 - Block editor source-mutation safety: keep opening/switching to Blocks mode non-mutating, including missing `encoding` directives, and only dirty documents after explicit user edits or encoding conversion.
- P1 - Background layer model/loading cleanup: extract `MapEditorBackgroundLayerModel`/loader responsibilities before adding more background-layer features, move large raster decode/gamma/scaling work behind cancellable async jobs, and add a bounded `.xvi` cache policy or clear-on-project-close hook if large `.xvi` projects become common.
- P1 - Map canvas pan/zoom performance: keep point labels cached inside point items, suppress distant labels, limit pattern repaints to exposed rects, avoid `.xvi` reprojection on view-only zoom, cull `.xvi` vector drawing through a spatial line-tile index, and use a viewport update strategy that avoids excessive region bookkeeping with many scene items.
- P1 - Map line-style simplification: remove legacy secondary line-detail overlay (`detailWidth`/`detailColor`) and keep a single-stroke line style model.
- P1 - Line decoration style coverage: keep JSON-driven `stroke_visible`, `closed_fill`, and `decorations` support for symbol-only and compound line symbols (`symbols`, `waves`, `teeth`, `ticks`, `slope_ticks`, `offset_stroke`, `parallel`, `rungs`) aligned with Therion MetaPost symbol-set coverage, preferring SKBB definitions and then UIS, and tune bundled line subtype presets, including Therion-correct left/right side orientation for ticks/teeth, right-side SKBB wall material marks, path-segment-based MetaPost-like sawtooth `teeth`, MetaPost-style adjusted spacing for repeated decorations, wall material subtypes, closed rock-border background clean-fill, pit/floor-step/ceiling-step ticks, overhang teeth, survey/border presets, and SKBB-ratio long-short alternating slope ticks where `l-size` is the long tick length.
- P1 - Map object style gallery: keep the generator covering command-catalog point/line/area type+subtype combinations, explicit type/global-default fallback entries, style-only selectors, documented build steps, and visual review output for bundled/user override tuning.
- P1 - Map insertion inspector workflow: continue validating pending-insert `Selection` inspector defaults, reliable `Esc` cancellation from focused inspector fields and command-toolbar focus, cleared-selection empty-state behavior, screen-space area/line hit testing, full-width style-preview layout, style-preview fidelity including exact `symbol_parts` fill/stroke colors without artificial symbol halos, line-decoration size/spacing/color contrast on the light preview surface, area tile coverage, decoration jitter, and dark-mode contrast, and the two-step scrap insertion UX across point/line/freehand/area workflows.
- P1 - Style schema naming cleanup: use consistent `stroke_width` keys for point and line widths in map style JSON (no legacy aliases).
- P1 - Style schema naming cleanup: use `stroke_style` instead of `pen_style` for line/area stroke style keys in map style JSON (no legacy aliases).
- P1 - Style schema naming cleanup: use `raw_type` + `raw_subtype` selector naming consistently in map style JSON.
- P1 - Point/line/area symbol style schema: use only `symbol_parts` for JSON symbol geometry across point styles, repeated line `symbols`, and area `dots`; keep per-part `fill_color`, `stroke_color`, and non-negative `stroke_width` as the unified symbol color model; derive area dot motif size from `symbol_parts` unless explicitly overridden; keep `point.station` on the bundled triangle symbol with `label_field: "name"`, use `point.label` with `label_field: "text"` and cached Therion label text rendering inside point items, keep point labels anchored with screen-space offsets next to their symbol and hidden at distant zoom, document point/line/area user override parameters, support built-in shape parts (`circle`, `oval`, `square`, `diamond`, `triangle`, `star`, `asterisk`, `plus`, `x`) plus primitive parts (`line`, `polyline`, `polygon`, `ellipse`).
- P1 - Point orientation rendering: keep catalog-gated point `-orientation` controls, draggable handles, symbol rotation, and style-controlled label text rotation in sync; station names remain screen-aligned, while bundled `point.label` text follows explicit orientation.
- P1 - Main-window map-tab signal hardening: keep map-editor tab focus/open/reattach workflows free of duplicate UI signal wiring and Qt debug aborts from invalid lambda `Qt::UniqueConnection` usage.
- P1 - Area fill-pattern renderer tuning: continue post-vectorization tuning of `fill_pattern` readability across zoom (LOD-aware spacing/width/size/opacity shaping, exposed-rect-limited repainting, visual `-clip on` clipping to resolvable scrap wall boundaries, Therion-style `-place bottom/default/top` render ordering, symbolic `dots` grid/scatter placement, size/angle jitter, LOD-correct `symbol_parts` stroke widths, and `cross_hatch` density at high zoom).
- P2 - SVG map-style symbols: evaluate SVG-backed `symbol_parts` for future point styles and area `dots`, using safe relative files under `map_object_styles/`, no remote/external references, and cached QtSvg rendering.
- P1 - Area style catalog coverage: refine per-`raw_type` area styles in split `resources/map_object_styles/*.json` files for all generated Therion area types, including dot-symbol granular fills that stay visually aligned with matching point symbols such as debris, and tuned clay/ice/water/sump/mudcrack hatch fills, keeping pattern density readable across zoom.
- P1 - Split map-style catalog: keep bundled and user map-style files on the same partial JSON convention, loading bundled `resources/map_object_styles/*.json` first via a scoped CMake resource glob and application-data overrides second.
- P1 - User map-style overrides: load partial JSON override files from the application data `map_object_styles` directory after bundled defaults, keep macOS/Windows/Linux manual paths current, with default/type/subtype precedence and no project-level override layer.
- P1 - Windows installer Qt runtime layout: install `TherionStudio.exe` under `bin/` so deployed Qt DLLs and plugin directories, including `platforms/qwindows.dll`, stay relative to the executable.
- P1 - Windows GUI subsystem: link `TherionStudio.exe` without a console window so closing a console host cannot terminate the app.
- P1 - Main-window startup geometry: enforce usable default/minimum main-window size and clamp restored session geometry to avoid narrow Windows launches while preserving half-screen resizing.
- P2 - GitHub Actions runtime maintenance: keep Windows installer workflow actions on Node 24-compatible versions to avoid Node 20 deprecation warnings.
- P1 - Inspector type dropdown hydration: ensure `point`/`line`/`area` `Type` select values load from command catalog reliably across catalog schema variants (`commands` array/object), then validate map selection-panel editing workflow.
- P1 - Selection-panel editable type/subtype combos: keep popup suggestions unfiltered by current text so users can switch away from parsed value without clearing the field first.
- P2 - Phase 7: finalize UX/accessibility/platform-convention parity, with focus on shortcuts and keyboard behavior consistency on macOS, Windows, and Linux.
- P3 - Phase 8: continue release-readiness and packaging hardening, with focus on signing/notarization expectations and release reproducibility.

## Next Up

- P0: Continue Phase 9 with remaining map-editor parity polish in selection/details ergonomics and cross-platform input edge cases.
- P1: Continue Phase 10 with Apple Pencil/freehand stroke UX, broader parsed-document snapshot use, less-aggressive shape-sensitive bezier simplification, and point/line/area workflow polish.
- P2: Continue Phase 11 with object-reference target picking/resolution shortcuts, then selection/details glue consolidation without behavior changes.
- P1: Continue ProjectIndex hardening with compiler-confirmed snapshot comparisons.
- P1: Address the repo review follow-up by consolidating remaining direct Lucide SVG rendering through `LucideIconFactory`.
- P2: Monitor scheduled Windows/Linux packaging runs as maintenance; APT repository publishing is deferred.

## Risks / Blockers

- Parser/serializer round-trip coverage is still incomplete for full Therion corpus-level confidence.
- Pen/touch navigation depends on automatic input-policy behavior; Sidecar and touch-screen edge cases remain a product risk.
- Map/text undo arbitration still depends on choosing between map `QUndoStack` and embedded text-editor undo until unified command-stack refactor.
- Fast Linux push/PR CI intentionally stays on the hosted Ubuntu 24.04 runner only. Windows installer and Linux packaging now run daily on the default branch. Linux packaging depends on the local-reproducible helper cluster under `scripts/linux-packages/`, the Ubuntu 26.04 `amd64`/`arm64` `.deb` container script, the Debian 13 `x86_64`/`aarch64` AppImage container script, the full local build+smoke wrapper, Docker-backed install/launch smoke scripts, Debian 13 distro Qt, Qt's Linux deploy script, explicit Qt plugin/runtime library staging, an AppRun runtime-path wrapper, pinned `appimagetool`/runtime inputs, downloaded-artifact-root discovery in smoke jobs that excludes CPack internals, split snapshot artifact/Debian version metadata, and separate `.deb` versus AppImage target expectations.
- Signing/distribution requirements are not yet exercised end-to-end on all target platforms.

## Phase Plan

### Phase 7 - UX, Accessibility, and Platform Conventions (`MVP` baseline)

- Status: in progress.
- Remaining work: improve shortcut matrix, menu behavior, focus traversal, high-DPI behavior, dark/light palette transitions, and platform-native expectations.
- ~~View menu now exposes dynamic `Expand/Collapse Sidebar`, active right-panel labels (`Context Help`, `Block Inspector`, or `Map Inspector`), and `Show/Hide Map Magnifier`; the removed compiler visibility control remains available through the sidebar surface.~~
- ~~Detached map pane state now exposes both `Map Inspector` and `Context Help` View controls because raw source and visual map panels are visible simultaneously.~~
- ~~The View sidebar action now derives its label from the effective splitter/content state, so a visually collapsed sidebar shows `Expand Sidebar` even if the legacy collapse flag is stale.~~
- ~~Map and Window menus were removed; the fullscreen toggle remains under View, matching macOS menu conventions.~~
- ~~Help menu now keeps only `User Manual` plus About; manual resolution prefers localized `USER_MANUAL.<language>.md` files and falls back to `USER_MANUAL.md`.~~
- ~~File -> New Window now opens a clean empty window session instead of restoring and duplicating the current project/open documents.~~
- ~~macOS now relies on the native View -> Enter Full Screen item instead of adding a duplicate explicit Qt action.~~
- ~~Help -> About Therion Studio now shows installed version/build metadata, Qt/platform details, repository, license, maintainer, and third-party notice location.~~
- ~~On macOS, About now uses the native application-menu role so the app-menu About item is enabled and routes to the Therion Studio About dialog.~~
- ~~Bundled Qt UI translation catalogs now cover Czech and Slovak for menus, toolbars, dialogs, alerts, empty states, inspectors, search controls, console labels, and help-panel chrome while preserving canonical Therion syntax tokens.~~
- ~~Settings dialog now owns the application language override, Therion executable path, and default `.th`/`.thconfig` editor mode; the Compiler sidebar keeps run-context fields only.~~
- ~~macOS bundle metadata now advertises English, Czech, and Slovak localizations so the system per-app language selector can offer the shipped translations; Settings language choices sync with the macOS app-domain language override, and startup loads Qt standard translation catalogs so native app-menu items such as Preferences participate in localization.~~

### Phase 8 - Release Readiness and Packaging (`MVP`)

- Status: in progress.
- Remaining work: harden release process and artifacts after the current Windows-installer + Homebrew + Linux `.deb`/AppImage flow, keep scheduled packaging checks healthy, and keep signing/notarization expectations explicit.
- ~~Regenerated bundled app icons (`resources/app/TherionStudio.icns`, `resources/app/TherionStudio.ico`) from `resources/app/app-icon.svg` for the current branding iteration.~~
- ~~Switched runtime app-window icon source from SVG to clipped PNG (`resources/app/app-icon.png`) to avoid Qt SVG `clipPath` rendering artifacts in UI icon surfaces.~~
- ~~Regenerated bundled app icons (`resources/app/TherionStudio.icns`, `resources/app/TherionStudio.ico`) from externally refined `resources/app/app-icon.png` master artwork.~~

### Phase 9 - Map Editor Parity Polish (`Post-MVP`)

- Status: in progress.
- Remaining work: continue interaction parity review for inspector ergonomics, command-surface consistency, selection edge cases, and cross-platform input behavior.
- ~~Background layer Hide/Show now persists as UI/session state only and no longer rewrites `.th2` xth metadata or marks the document dirty.~~
- ~~Open validation item: confirm robust source rewrite behavior for `Split Here` on open area-referenced borders across mixed area-body layouts; closed-line split remains unsupported.~~
- ~~Fix `.xvi` background handling: do not treat `.xvi` as raster bitmap (proper scaling path + gamma disabled).~~
- ~~Fix `.xvi` metadata-load regression causing UI freeze/spinning ball via unbounded offscreen render and mismatched model bounds.~~
- ~~Improve `.xvi` visual fidelity with zoom-aware supersampled re-render so overlay stays sharp during map zoom.~~
- ~~Switch `.xvi` background rendering from pre-rasterized pixmap overlays to a vector-painted scene item path (grid/shots/sketch) to keep zoom quality and reduce re-raster cost.~~
- ~~Fix `.xvi` open-time UI hangs by caching parsed `.xvi` payloads and skipping geometry rebuilds when projection inputs are unchanged.~~
- ~~Differentiate `.xvi` shot classes visually: render splays with distinct color from traverse shots and keep sketches visually separate.~~
- ~~Tune `.xvi` rendering performance/clarity: faster station lookup for shot classification and LOD-aware drawing (hide splays/sketch/grid at distant zoom), plus slightly stronger line weights.~~
- ~~Preserve and render `.xvi` sketch-line color tokens (`connect` as dotted gray, named colors mapped directly) so auxiliary sketch semantics are visible in map background overlays.~~
- ~~Inspector smooth/control toggles now preserve line-vertex targeting even when scene selection temporarily drops during UI interaction (fallback to stored selected line/vertex).~~
- ~~Inspector `<<` / `>>` control-handle toggles now resolve through line-control owner vertices (selection/controller/details mapping), so closed-line vertices can reliably enable both handles for full smooth editing.~~
- ~~Inspector smooth toggle now preserves vertex selection across asynchronous scene refresh (flush + multi-attempt owner-vertex selection recovery).~~
- ~~Map render style defaults are now data-driven from `resources/map_object_styles/*.json` (point symbol/size/stroke, line thickness/style, area stroke/fill-opacity/style), with safe hardcoded fallback when JSON is missing/invalid.~~
- ~~Extended map object style catalog to support per-type/per-subtype overrides with colors, dash patterns, and area fill patterns (`solid`/`hatch`/`dots`) from `resources/map_object_styles/*.json`, keeping defaults as fallback.~~

### Phase 10 - Interactive Map Drawing and Insertion (`Post-MVP`)

- Status: in progress.
- Remaining work: improve Apple Pencil/freehand stroke preview/output, solid freehand draft preview, less-aggressive shape-sensitive bezier simplification, and remaining insertion edge cases.
- ~~Line draft closure UX: clicking the first anchor in line mode now commits as closed line (`-close on`) without adding a duplicate terminal anchor row.~~
- ~~Closed-line smoothing parity: map editor now supports editing/rendering control handles for the closing last->first segment (including persisted closing Bezier row rewrite and parsed-model normalization without duplicate terminal anchor).~~

### Phase 11 - Structured Text Authoring Canvas (`Post-MVP`)

- Status: in progress.
- Remaining work: expand configurable block coverage, add target-picking/navigation affordances for map object references, improve insertion anchoring/source-safety feedback, and continue BlockEditor extraction.

## Refactor Tracks

- Architecture refactor roadmap: `AGENTS.md` owns durable architecture rules; this worklog owns active roadmap/backlog state. Phase 1 dependency-injection seams are complete, Phase 2 main-window service extraction is complete, Phase 3 text-editor shell reduction is complete, Phase 4 map-editor shell decomposition is complete, Phase 5 platform/appearance centralization is complete, and Phase 6 packaging/CI hardening is complete for the current scope.
- Track A: Phase 3 completed; continue with focused Phase 4 map-editor shell decomposition and document-workflow seams that are shared across Raw/Block/Map modes.
- Track B: continue extracting `MapEditorTab` responsibilities into focused controllers for drawing, inspectors, selection details, scene lifecycle, and undo/snapshot orchestration.
- Track C: consolidate shared source-edit/rewrite primitives used by Raw, Blocks, and Map modes into focused non-UI services.
- MainWindow maintenance: split status-bar/status-lifecycle methods into `src/app/MainWindowStatusUi.cpp` to keep `src/app/MainWindow.cpp` within structure-constraint line limits without behavior changes.
- BlockEditor next slice: consolidate selection/details glue (`selectBlockInCanvasAndDetails`, `refreshBlockDetailsSelectionFromScene`, `resolveBlockCanvasItem`, `selectBlockCanvasItem`, and related thin delegates) if API boundaries remain stable.
- Guardrail: architecture rules live in `AGENTS.md`; do not reintroduce hidden production adapters, static service facades, callback service-locators, or god-widget workflow growth.
- Guardrail: non-trivial user proposals should be reviewed against architecture, performance, UX, security, portability, data integrity, and testability before implementation; risky proposals should include alternatives and tradeoffs.
- Guardrail: each slice should own one responsibility, keep behavior stable, and update documentation when user-visible behavior changes.

## Backlog

- Unified document-level undo stack for raw text edits, visual map mutations, inspector setting changes, object/background edits, and structured block changes.
- Add focused tests around `IFileSystem`/`ISessionStore` fakes when document/session seams are next touched.
- Split `TherionDocumentEditor` by rewrite domain only after adding or confirming round-trip regression tests for the touched rewrite operations.
- Broader GUI automation for structure, inspector, runner, map workflows, and cross-platform keyboard/shortcut matrix coverage.
- Expanded rewrite corpus/regression coverage beyond the MVP fixture set.
- Extend `input` relative-path autocomplete semantics to other path-taking Therion commands/options, such as `-sketch`.
- Completion ranking polish for complex contexts so highest-confidence context-valid tokens stay first and low-signal fallback entries are reduced.
- Future Smart Trace implementation with real trace detection, preview, bezier simplification, and one undo step per accepted trace.
