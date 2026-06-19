# Worklog

Active planning only. Completed history belongs in archive files. Stable architecture belongs in `ARCHITECTURE.md`. Detailed plans live in `plans/`.

## Current Focus

1. Release readiness for `v2026.6.7`.
2. Unified Source DOM and source transaction ownership.
3. Test infrastructure hygiene and structure guardrails.
4. 3D viewer GPU-backed viewport rollout and shell migration.

## Active Work

### Release Readiness

- Run local validation before tagging or packaging handoff, including focused map inspector regressions touched during release stabilization.
- Keep Selection inspector point-geometry ordering, `-clip off`, and narrow-width layout regressions covered while stabilizing `v2026.6.7`.
- Keep Selection inspector terminology consistent: use `Options` for object-level settings and reserve `Line Point` for vertex workflows.
- Keep Selection inspector object-level actions grouped under `Options`, including `Name`/`Text`/`Value`, scrap projection, and the full object-settings entrypoint.
- Keep Selection inspector `Options` ordering deterministic across refreshes; visible rows must not jump when selection state or option visibility changes.
- Keep map inspector numeric spin boxes from committing partially typed values before editing is finished.
- Keep map path hit-testing from letting hidden line control handles steal clicks from visible nearby paths.
- Keep hidden gated map vertices reachable for context-menu selection paths without making ordinary hidden handles steal primary clicks.
- Keep map line selection readable by focusing clicked segments: primary path clicks should expose only the segment
  endpoint anchors and their control handles, with smaller visible vertex markers.
- Keep selected map line/area vertex markers close to XTherion styling: larger red anchor circles with blue
  outlines, smaller blue control-point squares, and a red focused-vertex halo.
- Keep empty scrap object cleanup explicit through validator warnings and `Apply Fix`, not silent source mutation.
- Keep deletion-style validation fixes visibly labeled as removals and preview the full source block being removed.
- Keep inline `type:subtype` map object rendering aligned with inspector preview and `-subtype` rendering.
- Keep Selection quick-field label/input visibility synchronized with wrapper visibility across clear/refresh cycles.
- Keep point `-align` rendering anchored like Therion so map canvas placement matches saved point options.
- Clear pending Selection inspector insert state when Smart Area confirmation returns to Select mode.
- Keep release notes, package metadata, and CI artifact workflow aligned with `v2026.6.7`.
- Keep Linux CI and package-builder Qt dependency lists aligned with the Qt Quick/QML-backed 3D viewer surface.
- Keep Windows CI and installer Qt archive lists aligned with the Qt Quick/QML-backed 3D viewer surface.
- Keep AppImage runtime-library staging aligned with Debian Qt runtime dependencies instead of masking missing
  bundled libraries in smoke-test containers.
- Keep AppImage package/smoke scripts diagnostic enough to identify whether missing runtime libraries were lost
  during AppDir staging or final AppImage packaging.
- Keep dynamically loaded AppImage runtime backends, such as libproxy's `libpxbackend-*.so*`, staged explicitly
  because they are not guaranteed to appear in direct `ldd` dependency scans.
- Keep AppImage runtime backend discovery recursive within system library roots so distro-specific subdirectories
  such as libproxy module directories are bundled before smoke testing.
- Keep dynamically loaded AppImage backend modules copied directly into `AppDir/usr/lib` after discovery so final
  package validation can prove they are present even when they are not ordinary `ldd` dependencies.
- Keep AppImage dynamic backend dependency closures bundled for arm64 smoke tests, including libproxy's curl,
  GSSAPI, Kerberos, LDAP, and TLS support libraries.
- Keep AppImage runtime backend diagnostics explicit enough to show searched roots and matched candidates when
  distro runtime modules are missing from the AppDir.

### Unified Source DOM / Transactions

- Tighten source-file reference resolution while preserving Therion namespace semantics from `docs/THERION_COMPATIBILITY.md`.
- Slice 2 - Expand `TherionSourceSnapshotCache` into Structure/Validation once stale-projection behavior is covered for text edits, undo/redo, document reload, source type changes, and catalog refresh.
- Slice 4 - Migrate Blocks cards/details/move planning toward shared logical command and option ranges while preserving one source transaction per user-visible change.
- Slice 5 - Migrate Map/TH2 object discovery, generic option parsing, reference resolution, and Smart Area insert planning to shared logical commands while keeping map geometry parsing map-specific.
- Slice 6 - Close source transaction ownership by routing remaining user-visible source mutations through `TextEditorSourceTransactionController` or a narrow successor with undo label, expected revision, invalidation, dirty-state, and selection/cursor policy.
- Slice 7 - Reuse cached logical documents for Structure, project index, namespace/reference resolution, search, and live Validation diagnostics.
- Slice 8 - Remove duplicate editor-local tokenizers, option parsers, line splitters, numeric classifiers, and source-range heuristics only after migrated consumers have regression coverage.

### Validation And Catalog Metadata

- Keep validation conservative while moving command, option, and positional argument interpretation into catalog-backed logical-command metadata.
- Keep problem reporting centralized in the Validation panel while Structure remains an orientation/navigation view.

### Test And Structure Hygiene

- Use QTest for new C++ tests while keeping CTest as the runner.
- Keep `tests/core/` and `TherionCoreQTests` as the baseline pattern for small core-only QTest cases.
- Keep `MainWindowServiceQTests` as the baseline pattern for small MainWindow project/session service tests that share `therion_app` dependencies.
- Keep `TextEditorDocumentServiceQTests` as the baseline pattern for small text-editor document IO/state/precondition/workflow service tests.
- Keep `TherionRunnerSupportQTests` as the baseline pattern for Therion runner config, executable-selection, and presenter support tests; keep real process runner tests isolated.
- Continue migrating touched hand-rolled tests to QTest where the dependency/runtime boundary is already clear.
- Keep `python3 scripts/check_structure_constraints.py` green and preserve guardrails against map-editor source mutation bypasses.
- Keep the explicit user-confirmation gate before every `git commit`.
- Keep optional sample-data dependent tests from aborting CI when fixture directories are absent.
- Keep shared workspace command bars palette-aware across light and dark appearance changes.
- Keep Linux CI/package Qt runtime module lists aligned with QML inspector imports.
- Keep UI smoke tests deterministic across platform event-loop timing differences.

### UI Cleanup

- Continue `plans/GUI_CLEANUP.md` in independently shippable slices.
- Keep style policy, UI construction, presentation contracts, and source/model logic separated.
- Keep `plans/3D_VIEWER_PLAN.md` as the planning reference for remaining 3D viewer refinement work.

### 3D Viewer

- The `.lox` loader and neutral scene model are implemented in `src/core/` and covered by `TherionCoreQTests`.
- The `ThreeDViewerCamera` model now lives in `src/core/` and owns orbit, pan, zoom, fit, and reset state for the viewer shell.
- The viewer projection helper now lives in `src/app/three_d_viewer/` and is covered by a dedicated `ThreeDViewerQTests` runner.
- The 3D viewer toolbar now exposes top-view, side-view, and rotate-left/rotate-right controls alongside fit/reset controls, with rotation around the world blue axis.
- The `ThreeDViewerTab` host is integrated into the main window as a read-only `.lox` viewer tab with basic layer toggles, scene summary, and a first interactive viewport slice.
- The 3D viewer inspector now uses a Qt Quick/QML host embedded through `QQuickWidget`, with the inspector content rendered from a shared QML surface, grouped into scene/layer sections while the viewport is moving to a GPU-backed scene-graph host.
- The viewport rendering now lives in a QQuickItem-backed scene-graph surface so the viewer can render through Qt Quick instead of QWidget painting.
- Mesh groups now render through Qt Quick's built-in GPU vertex-color material on the scene graph path.
- The 3D viewer inspector now exposes a model-coloring mode that switches the GPU model palette between altitude coloring and no coloring.
- The 3D viewer centerline now uses the same altitude or uncolored palette as the meshes.
- The 3D viewer viewport now draws a red bounding box around the current scene extent.
- The 3D viewer viewport now overlays a compass, scale bar, and altitude legend when scene bounds are available.
- The 3D viewer canvas now uses a black background to match the Loch-style presentation.
- The 3D viewer centerline, stations, and labels now use higher-contrast rendering on the black viewport background.
- The 3D viewer station markers are now smaller and less visually noisy on dense views.
- The 3D viewer station markers and fully qualified station labels now use automatic screen-space decluttering instead of drawing every overlapping station annotation.
- The 3D viewer viewport now shows hover details for station markers, including full station reference, and supports a ruler-toggle measurement mode for station-to-station distance, azimuth, and vertical difference.
- The 3D viewer hover card layout now uses a more even padding balance and larger typography for station details.
- The 3D viewer viewport now overlays Underground Passages Length and Underground Depth, computed from underground centerline shots only and excluding surface, splay, duplicate, and surface geometry contributions. The altitude legend is shown only in depth coloring mode, the compass and view-angle indicator are grouped beneath it like Loch, the view-angle semicircle uses a horizontal split and signed upper/lower motion, the scale bar is a simple line with end ticks, and the altitude legend includes more intermediate labels.
- The 3D viewer scene statistics overlay now uses larger typography for the project title and underground passages/depth summary.
- The 3D viewer HUD scale bar is aligned to the compass row with a matching gap to the view-angle indicator.
- The 3D viewer toolbar now uses arrow-based icons for `Top View` and `Side View`.
- The 3D viewer core now exposes station qualified-name construction in the shared scene model and has broader `.lox` fixture-matrix coverage for survey hierarchy, shot flags, and synthetic terrain surface chunks.
- The transitional QWidget viewport renderer has been removed now that the scene-graph viewport is the sole active render path.
- The viewport controller is now split out from the widget shell so camera interaction and camera-change signaling can be reused by future render surfaces.
- The layer inspector is now backed by a shared list model so the QWidget view and future QML UI can use the same visibility/count state.
- The 3D viewer layer inspector now shows clean layer names without visible item counts, generates centerline sublayers from present shot classes, defaults centerline sublayers to underground-only visibility, exposes disjoint station sublayers only when multiple station classes are present, and keeps station markers and labels constrained to stations attached to currently visible centerline shots.
- The 3D viewer station hover and measurement picking now ignore stations whose centerline shots are hidden by the current centerline layer filters.
- The 3D viewer labels layer now remains independent from station marker visibility while still respecting the visible centerline filters.
- Viewer fit/reset controls now live in the shared workspace command bar instead of a tab-local toolbar.
- The 3D viewer layer list now blocks internal item-change recursion during tab construction and refresh.
- The 3D viewer toolbar now has a play/stop automatic-rotation toggle, and the inspector exposes rotation speed in degrees per second.
- The 3D viewer toolbar now orders view controls as reset, fit, orthogonal projection, top/side views, rotate controls, auto-rotation, and measurement, using a home icon for reset and standard rotate icons for manual yaw.
- The 3D viewer inspector now exposes precise camera sliders for compass heading, tilt, distance, and focal length while keeping those values synchronized with viewport navigation; focal length is disabled in orthographic projection.
- The 3D viewer Fit command now computes camera distance from the projected scene bounds using the current viewport aspect ratio, reducing excessive empty space around fitted `.lox` models.
- The 3D viewer removed survey-based model coloring; Altitude is now the default model-coloring mode, and None renders meshes in one solid light-gray color.
- The 3D viewer inspector Scene Settings section now appears after Layers and exposes default-on visibility toggles for the bounding box, full HUD overlay, and title/statistics overlay.
- The 3D viewer viewport no longer draws standalone world X/Y/Z axis guide lines over the scene.
- The 3D viewer Layers default now starts with Stations and Labels hidden, while underground centerline, meshes, and surfaces remain visible.
- The 3D viewer inspector now uses consistent Title Case for English field and layer labels.
- The 3D viewer now supports arrow-key yaw/tilt navigation, higher-contrast per-vertex mesh lighting, and a palette-aware QML inspector surface styled closer to the existing QWidget inspectors.
- The project File sidebar now uses the same Therion badged document icon for `.lox` files as for `.th`, `.th2`, and `thconfig` files.
- Continue the renderer refinement and the broader Qt Quick/QML shell migration once the GPU-backed viewport proves out the document-open workflow.

## Blocked / Needs Input

- Old Therion/Metapost crash fixture: keep parked until a reproducible project or minimal fixture is available.
- Stylus/Sidecar behavior: needs hardware-specific manual validation.

## Backlog

- Replace fixed-delay map selection-restore retry timers with explicit scene-refresh completion/generation callbacks.
- Optional Structure graph view for relationships such as `preview`, `revise`, `join`, `equate`, relationship status, and station-network edges.
- Compiler-confirmed project-index comparison once lightweight indexing is no longer sufficient.
- Retire or demote the manual `Validate Project` action after live project diagnostics are reliable for edits, saves, file operations, project-open, and catalog/source-model refresh events.
- Broader Therion corpus regression tests for parsing, serialization, source rewrites, indexing, and map/text synchronization.
- Add old-project integration fixtures for Therion/Metapost runner failures once a fixture exists.
- Bounded `.xvi` cache policy for very large projects.
- Station marker/label priority ranking follow-up: tune automatic decluttering if dense projects hide important stations.
- Make line guide-spine rendering explicit in style JSON (`guide_spine_visible`) and remove the fallback when catalog coverage allows it.
- Apple Pencil/freehand stroke UX and shape-sensitive simplification polish.
- Additional map-style catalog tuning and SVG-backed symbol evaluation.
- Mapiah background editing/export follow-up for mixed XTherion/Mapiah metadata, XTherion rewrite caveats, undo/redo, Visual/Raw mode switching, selected-layer pivot marker behavior, and `Display` controls.
