# 3D Viewer Integration Plan

This plan describes a cautious path toward a minimal GPU-accelerated 3D viewer in Therion Studio. The existing Therion Loch sources in `therion/loch` are useful as a behavioral and format reference, but the new feature should use neutral `3d_viewer` naming and should not import the old Loch GUI architecture wholesale.

## Goals

- Provide a minimal 3D cave viewer inside Therion Studio.
- Use GPU-accelerated rendering through modern Qt facilities.
- Use the existing Therion Loch implementation as a reference for data formats, camera behavior, scene organization, and rendering behavior.
- Keep the first version read-only and explicitly separated from 2D map editing.
- Keep loading, parsing, scene-model construction, and process/file access in C++ services rather than QML.
- Preserve the existing Qt Widgets application while allowing a focused Qt Quick/QML viewport where it provides clear rendering value.

## Non-Goals

- Do not port the old Loch wxWidgets UI into Therion Studio.
- Do not attempt full Loch feature parity in the first slice.
- Do not introduce live 3D editing or source mutation from the viewer.
- Do not move Therion execution, file IO, parsing, or project indexing into QML.
- Do not bind the rest of the application to a new QML architecture as part of this work.
- Do not depend on legacy immediate-mode OpenGL unless it is isolated behind a narrow renderer adapter and justified by a discovery spike.

## Recommended Direction

Use a C++ data/model backend first, then add a Qt Quick-based viewport once the loader and scene contract are stable. Prefer Qt Quick 3D if the required geometry can be represented as lines, points, labels, and simple meshes with acceptable performance. If Qt Quick 3D cannot represent cave-viewer rendering, picking, or large-cave performance well enough, use a lower-level custom renderer through a Qt Quick integration point such as `QQuickFramebufferObject` or an equivalent Qt 6 rendering abstraction.

The first viewer should be a preview surface, not a full Loch replacement. It should open compiled 3D output or a well-defined Therion 3D artifact, render the main cave geometry, and provide predictable navigation.

The first implementation slice should keep Qt Quick scoped to the inspector host until the loader and scene contract are stable. The current application links Qt Core, Widgets, Svg, Concurrent, and Test, but not Qt Quick or Qt Quick 3D. A `.lox` loader and neutral scene model can be implemented and tested before making packaging and runtime decisions for the viewport.

## Discovery Findings

- Therion exports Loch data through `thexpmodel::export_lox_file` in `therion/src/therion-core/thexpmodel.cxx`.
- The shared `.lox` data structures and import/export implementation live in `therion/src/common-utils/lxFile.h` and `therion/src/common-utils/lxFile.cxx`.
- Loch opens `.lox`, Compass `.plt`, and Survex `.3d`, but `.lox` is the richest native artifact and should be the MVP target.
- `.lox` contains chunked binary records for surveys, stations, shots, scrap wall meshes, terrain surfaces, and surface bitmaps.
- Loch's runtime model in `therion/loch/lxData.*` converts `lxFile` records into wx/VTK/OpenGL-oriented data structures.
- The new Studio viewer should not reuse `lxData` directly because it pulls in wx and VTK concepts; it should translate `.lox` into a small Studio-owned scene model.
- Loch's OpenGL renderer and setup dialogs are useful behavioral references, but should not become ownership boundaries in the new implementation.

## Architecture

Introduce a focused feature area, for example:

```text
src/app/3d_viewer/
  ThreeDViewerWidget.*
  ThreeDViewerController.*
  ThreeDViewerSceneModel.*
  ThreeDViewerCamera.*
  ThreeDViewerLoadService.*
  qml/
    ThreeDViewport.qml
```

The exact file list should be created only as implementation needs become clear.

Ownership rules:

- `ThreeDViewerLoadService` owns loading compiled 3D artifacts and converting them into neutral scene data.
- `ThreeDViewerSceneModel` exposes renderable points, lines, labels, meshes, visibility flags, extents, and metadata.
- `ThreeDViewerCamera` owns orbit, pan, zoom, fit, reset, projection mode, and view state.
- QML owns the inspector surface first, then later viewport presentation, input gestures, small overlay controls, and bindings to the C++ model.
- Production dependencies should be passed through explicit composition boundaries.
- No viewer class should mutate `.th`, `.th2`, or `thconfig` source in the MVP.

## MVP Scope

- Load one `.lox` cave artifact selected by the user or produced by an explicit project action.
- Render centerline geometry.
- Render station points and optional station labels.
- Render basic walls, surfaces, or meshes only if they are available from the chosen artifact with low implementation risk.
- Provide orbit, pan, zoom, fit-to-scene, and reset-view controls.
- Provide visibility toggles for at least centerline, stations, and labels.
- Show loading and error states with actionable messages.
- Keep the viewer read-only.

## Phase Plan

### Phase 1 - Discovery Spike

- Use `.lox` as the first supported 3D artifact.
- Treat `lxFile` as the file-format reference, not as the final Studio scene model.
- Treat `lxData` as the reference for converting stations, shots, LRUD walls, scrap walls, surfaces, and surface bitmaps into renderable structures.
- Defer Qt Quick 3D versus custom Qt Quick renderer selection until a loader-backed scene model exists.
- Keep the first implementation slice below the viewport: loader, neutral scene data, bounds, and tests.

Deliverable: a `.lox` MVP data contract and a small loader/model implementation plan.

### Phase 2 - Read-Only Data Core

- Add a small C++ loader/model layer for `.lox`.
- Convert loaded data into a neutral scene model with stable identifiers where possible.
- Add bounds/extents computation for fit-to-scene.
- Add QTest coverage for loader behavior, malformed input handling, scene bounds, and camera math.
- Add a minimal fixture corpus for one small cave artifact.

Deliverable: loadable scene data without a production viewport.

### Phase 3 - Minimal GPU Viewport

- Add the inspector host in a narrow Qt Quick/QML surface first, then move the rendering surface toward Qt Quick/QML once the shell is stable.
- Render centerline and station points first.
- Add orbit, pan, zoom, fit, and reset controls.
- Keep UI controls minimal and consistent with existing Therion Studio tool surfaces.
- Verify the viewport on macOS first, then leave explicit Windows/Linux verification gates.

Deliverable: a usable read-only 3D preview for a small fixture.

### Phase 4 - Product Integration

- Add an explicit menu, toolbar, tab, or dock entrypoint for opening the viewer.
- Wire project-aware artifact selection only after the artifact contract is clear.
- Add clear errors for missing compiled output, stale output, unsupported format, and load failures.
- Add optional persistence for viewer visibility and camera state only after the viewport behavior is stable.

Deliverable: a user-accessible viewer that does not affect source documents.

### Phase 5 - Viewer Expansion

- Add walls, surfaces, entrances, fixed points, bounding boxes, or LRUD visualization based on the selected artifact and user value.
- Add station/object picking and focus if it can be done without coupling the viewer to editor mutation.
- Add project compile/run integration when Therion runner error handling and artifact paths are stable.
- Add larger corpus/performance fixtures and profiling.

Deliverable: incremental 3D-viewer capabilities, still behind stable model and renderer boundaries.

## Testing Strategy

- Use QTest for new C++ tests.
- Keep loader, scene-model, and camera tests separate from GUI/rendering tests where practical.
- Use small deterministic artifact fixtures for loader and bounds tests.
- Start with loader/model tests before introducing any Qt Quick runtime dependency.
- Use rendering smoke tests only after the viewport is stable and the platform/runtime setup is known.
- Keep large corpus, performance, or GPU-dependent tests separate from fast core/service QTest runners.

## Risks And Open Questions

- `.lox` is the first target artifact, but `.3d` and `.plt` import can remain future compatibility work.
- Qt Quick 3D may not be ideal for dense line rendering, label placement, or large cave models.
- A custom renderer may be more durable but increases implementation and packaging risk.
- Adding Qt Quick or Qt Quick 3D changes the deployment surface; keep it out of the loader/model slice.
- Cross-platform Qt Quick, graphics API, and deployment behavior must be verified on macOS, Windows, and Linux.
- Reusing Loch source code directly may carry architecture, dependency, or licensing constraints that need review.
- Large project performance may require spatial indexing, level-of-detail, or batched geometry earlier than expected.

## Next Implementation Slice

Add the next viewport slice on top of the existing widget shell:

- refine the current widget viewport into a more durable renderer/camera layer
- keep `ThreeDViewerSceneModel` as the shared scene contract for surveys, stations, shots, mesh groups, surfaces, and bounds
- keep `ThreeDViewerLoxLoader` as the `.lox` loader and structured error reporter
- keep `ThreeDViewerTab` as the read-only shell with toolbar, inspector, and layer toggles
- add QTest coverage for loader behavior, malformed input, empty files, and bounds calculation if a future renderer refactor needs it

## Release Position

This is post-`v2026.6.5` planning work. The next valuable step is the viewport/rendering slice on top of the existing shell.
