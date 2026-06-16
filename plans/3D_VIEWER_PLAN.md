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

Use a C++ data/model backend with a Qt Quick-based viewport for the first implementation. Prefer Qt Quick 3D if the required geometry can be represented as lines, points, labels, and simple meshes with acceptable performance. If Qt Quick 3D cannot represent cave-viewer rendering, picking, or large-cave performance well enough, use a lower-level custom renderer through a Qt Quick integration point such as `QQuickFramebufferObject` or an equivalent Qt 6 rendering abstraction.

The first viewer should be a preview surface, not a full Loch replacement. It should open compiled 3D output or a well-defined Therion 3D artifact, render the main cave geometry, and provide predictable navigation.

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
- QML owns viewport presentation, input gestures, small overlay controls, and bindings to the C++ model.
- Production dependencies should be passed through explicit composition boundaries.
- No viewer class should mutate `.th`, `.th2`, or `thconfig` source in the MVP.

## MVP Scope

- Load one compiled 3D cave artifact selected by the user or produced by an explicit project action.
- Render centerline geometry.
- Render station points and optional station labels.
- Render basic walls, surfaces, or meshes only if they are available from the chosen artifact with low implementation risk.
- Provide orbit, pan, zoom, fit-to-scene, and reset-view controls.
- Provide visibility toggles for at least centerline, stations, and labels.
- Show loading and error states with actionable messages.
- Keep the viewer read-only.

## Phase Plan

### Phase 1 - Discovery Spike

- Identify the primary 3D input artifact and format used by the current Therion Loch implementation.
- Map the responsibilities of `lxData`, `lxSScene`, `lxSView`, `lxRender`, and related setup/rendering code.
- Determine whether the first viewer should consume `.lox`, another compiled Therion 3D artifact, or an intermediate model produced by Therion Studio.
- Compare Qt Quick 3D against a custom Qt Quick renderer for line-heavy cave data, labels, camera control, and large scenes.
- Produce a short go/no-go note before implementing the UI.

Deliverable: a documented backend choice, input-artifact choice, and MVP data contract.

### Phase 2 - Read-Only Data Core

- Add a small C++ loader/model layer for the selected 3D artifact.
- Convert loaded data into a neutral scene model with stable identifiers where possible.
- Add bounds/extents computation for fit-to-scene.
- Add QTest coverage for loader behavior, malformed input handling, scene bounds, and camera math.
- Add a minimal fixture corpus for one small cave artifact.

Deliverable: loadable scene data without a production viewport.

### Phase 3 - Minimal GPU Viewport

- Add the Qt Quick/QML viewport host in a narrow widgets integration surface.
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
- Use rendering smoke tests only after the viewport is stable and the platform/runtime setup is known.
- Keep large corpus, performance, or GPU-dependent tests separate from fast core/service QTest runners.

## Risks And Open Questions

- The correct input artifact must be confirmed before UI work starts.
- Qt Quick 3D may not be ideal for dense line rendering, label placement, or large cave models.
- A custom renderer may be more durable but increases implementation and packaging risk.
- Cross-platform Qt Quick, graphics API, and deployment behavior must be verified on macOS, Windows, and Linux.
- Reusing Loch source code directly may carry architecture, dependency, or licensing constraints that need review.
- Large project performance may require spatial indexing, level-of-detail, or batched geometry earlier than expected.

## Release Position

This is post-`v2026.6.5` planning work. The next valuable step is the Phase 1 discovery spike, not a visible UI implementation.
