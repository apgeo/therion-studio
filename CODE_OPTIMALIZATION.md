# Therion Studio Code Optimalization Plan

## Executive Summary

This plan incorporates the useful findings from the earlier Claude-generated refactor draft and defines the current optimization roadmap. The application has reached the point where several QWidgets shells also act as application coordinators, service locators, persistence clients, and workflow controllers. That is the primary maintainability risk.

The recommended architecture is not a rewrite and not a migration away from QWidgets. The right path is an incremental refactor that keeps user-visible behavior stable, moves non-UI workflows behind explicit services, and leaves widgets responsible for presentation and event wiring only.

The highest priority is to stabilize the current in-progress refactor before adding more abstractions. The current tree already contains early dependency-injection work (`IFileSystem`, `ISessionStore`, `QtFileSystem`) and build wiring changes. Those are useful seams, but they must be made build-clean, tested, and consistently applied before deeper changes.

Key recommendations:

- Keep Qt 6 and QWidgets. The issue is not the UI toolkit; the issue is responsibility boundaries.
- Use a layered architecture: Presentation, Application, Core/Domain, Infrastructure/Platform.
- Introduce explicit application services around documents, sessions, project structure, command catalog access, Therion execution, and platform/config paths.
- Do not put pure business rules into widgets, scene items, or dialogs.
- Avoid broad service locators and static global access for testable workflows.
- Isolate OS-specific logic behind platform services and keep `Q_OS_*` usage out of feature code.
- Prefer incremental extraction with round-trip and UI smoke tests after every high-risk slice.

## Progress Tracker

This tracker records architecture optimization progress at phase level. `WORKLOG.md` remains the day-to-day active work board.

- [x] Phase 0: build and test wiring stabilized for the initial dependency-injection checkpoint.
- [x] Phase 1: low-risk dependency-injection seams completed.
- [x] Phase 1 partial: `IFileSystem` wired into `TextEditorDocumentController` with fake-filesystem regression coverage.
- [x] Phase 1 partial: `MainWindow` accepts `ISessionStore` instead of directly owning the concrete QSettings-backed store.
- [x] Phase 1 partial: map inspector type/subtype/projection metadata can be built from an injected command catalog.
- [x] Phase 1 partial: map object orientation support metadata can be built from an injected command catalog.
- [x] Phase 1 partial: map editor controllers receive inspector/orientation metadata through explicit `MapEditorTab` context state.
- [x] Phase 1 partial: `MapEditorTab` accepts an explicit `CommandCatalogStore` for map inspector/orientation metadata construction.
- [x] Phase 1 partial: `TherionSyntaxHighlighter` accepts an explicit `CommandCatalogStore` instead of loading command metadata directly from resources.
- [x] Phase 1 partial: `TextEditorTab` owns a single `CommandCatalogStore` and passes it to raw help/completion metadata and syntax highlighting.
- [x] Phase 1 partial: `MainWindow` owns one `CommandCatalogStore` and passes it to text and map editor tabs.
- [x] Phase 1 partial: map object-details edit actions use the injected inspector symbol catalog for subtype refreshes.
- [x] Phase 1 partial: application startup composes the default `CommandCatalogStore` before constructing `MainWindow`.
- [x] Phase 1 partial: map inspector/orientation helper APIs no longer expose static default catalog access.
- [x] Phase 1 partial: raw editor syntax/help controllers require explicit command catalog metadata injection.
- [x] Phase 1 partial: `MapEditorTab` reuses its injected command catalog for the embedded raw text editor.
- [x] Phase 1 partial: `MainWindow` creates additional windows with the already injected command catalog.
- [x] Phase 1 partial: UI shell constructors no longer expose no-catalog fallback overloads.
- [x] Phase 1 partial: unused static command-catalog facade removed; catalog access goes through explicit stores.
- [x] Phase 1 partial: legacy real-adapter convenience constructors removed from editor and main-window shells.
- [x] Phase 1 complete: document/session/catalog seams are injectable enough for focused tests and no UI command metadata consumer uses static catalog access.
- [x] Phase 2 partial: session document restore/persist planning extracted from `MainWindow` into `MainWindowSessionDocumentService` with focused app-unit coverage.
- [x] Phase 2 partial: open-document active-path precedence for session persistence extracted from `MainWindow` into `MainWindowSessionDocumentService` with focused app-unit coverage.
- [x] Phase 2 partial: automatic project-restore decision and protected-folder guard extracted from `MainWindow` into `MainWindowSessionProjectService` with focused app-unit coverage.
- [x] Phase 2 partial: session state persistence writes extracted from `MainWindow` into `MainWindowSessionStateService` with focused app-unit coverage.
- [x] Phase 2 partial: structure-name override JSON parse/serialize extracted from `MainWindow` into `MainWindowStructureNameOverridesService` with focused app-unit coverage.
- [x] Phase 2 partial: open/close project lifecycle decision logic extracted from `MainWindow` into `MainWindowProjectLifecycleService` with focused app-unit coverage.
- [x] Phase 2 partial: close-project dirty-document confirmation coordination consolidated into a single `MainWindowProjectLifecycleService` decision call.
- [x] Phase 2 partial: project workspace state transitions for open/close flows extracted from `MainWindow` into `MainWindowProjectWorkspaceService` with focused app-unit coverage.
- [x] Phase 2 partial: open/close project UI-flow presentation extracted from `MainWindow` into `MainWindowProjectUiFlowService` with focused app-unit coverage.
- [x] Phase 2 partial: open/close project side-effect ordering extracted from `MainWindow` into `MainWindowProjectOrchestrationService` with focused app-unit coverage.
- [x] Phase 2 partial: open/close project workflow step execution moved out of `MainWindow` and consolidated in `MainWindowProjectController`, driven by `MainWindowProjectOrchestrationService` plans.
- [x] Phase 2 partial: session-restore plan orchestration extracted from `MainWindow` into `MainWindowSessionRestoreOrchestrationService` and consumed by `MainWindowSessionController`.
- [x] Phase 2 partial: window geometry/state restore fallback decisions extracted from `MainWindow` into `MainWindowSessionWindowRestoreService`.
- [x] Phase 2 partial: session-restore console message presentation extracted from `MainWindow` into `MainWindowSessionRestoreUiFlowService`.
- [x] Phase 2 partial: session-restore workflow composition extracted from `MainWindow` into `MainWindowSessionController` (window-restore decisions + project-restore orchestration wiring).
- [x] Phase 2 partial: open/close project workflow composition extracted from `MainWindow` into `MainWindowProjectController` (lifecycle decision + workspace/orchestration + UI presentation wiring).
- [x] Phase 2 partial: open-document restore/persist workflow composition extracted from `MainWindow` into `MainWindowSessionDocumentController` (unsupported restore handling + active document restore + detached-active precedence persistence).
- [x] Phase 2 partial: thin `MainWindowProjectStepExecutor` and `MainWindowSessionRestoreStepExecutor` layers removed; equivalent step execution kept in controllers with behavior coverage preserved.
- [x] Phase 2 partial: external Therion run/resolve workflow rules extracted from `MainWindow` into `MainWindowTherionRunnerController` with focused app-unit coverage.
- [x] Phase 2: `MainWindow` application services extracted.
- [x] Phase 3 partial: MainWindow document save/close workflow orchestration extracted into `MainWindowDocumentController` with focused app-unit coverage.
- [x] Phase 3 partial: MainWindow document open/reuse planning extracted into `MainWindowDocumentOpenController` with focused app-unit coverage.
- [x] Phase 3 partial: MainWindow text/map tab activation and new-tab post-open workflow orchestration extracted into `MainWindowDocumentTabOpenController` with focused app-unit coverage.
- [x] Phase 3 partial: TextEditor document persistence state transitions (encoding normalization/status notes + clean snapshot/dirty reset) extracted into `TextEditorDocumentPersistenceStateService` with focused unit coverage.
- [x] Phase 3 partial: TextEditor document callback orchestration order after load/save/project-root updates extracted into `TextEditorDocumentWorkflowController` with focused unit coverage.
- [x] Phase 3 partial: TextEditor document precondition/guard checks for load/save extracted into `TextEditorDocumentPreconditionsService` with focused unit coverage.
- [x] Phase 3 partial: TextEditor document file IO and persistence-input assembly extracted into `TextEditorDocumentIoService` with focused unit coverage.
- [x] Phase 3 partial: TextEditorTab text/mode interaction orchestration (`handleTextChanged`, mode requests, mode-selector visibility wiring) extracted into `TextEditorTabInteractionController` with focused unit coverage.
- [x] Phase 3 partial: TextEditorTab document/mode delegate implementation split into `TextEditorTabDocumentModeDelegates.cpp` to keep the tab shell focused and reduce monolithic translation-unit coupling.
- [x] Phase 3 partial: TextEditorTab bootstrap/build flow (`buildAll`) split into `TextEditorTabBootstrap.cpp` and document-context assembly isolated behind `buildDocumentContext()` to reduce shell-level orchestration coupling.
- [x] Phase 3 partial: TextEditorTab status/cursor/document-state delegates (title/status refresh, dirty-state apply, cursor/status accessors) split into `TextEditorTabStatusCursorDelegates.cpp` to keep shell-level orchestration localized.
- [x] Phase 3 partial: TextEditorTab source-rewrite/configure/delete delegates split into `TextEditorTabSourceRewriteDelegates.cpp` to isolate document-mutation routing from the remaining shell orchestration.
- [x] Phase 3 partial: TextEditorTab raw-find/delegate slots, block-details/canvas delegates, and context-help/encoding delegate slots split into focused companion translation units, reducing `TextEditorTab.cpp` to lifecycle + interaction core orchestration.
- [x] Phase 3: `TextEditorTab` coupling reduced.
- [x] Phase 4 partial: MapEditor scene-refresh/appearance lifecycle flow (`buildMapScene`, refresh/schedule/flush source-driven refresh, appearance-triggered scene refresh) split from `MapEditorTab.cpp` into `MapEditorTabSceneRefreshLifecycle.cpp`.
- [x] Phase 4 partial: MapEditor selection/inspector insert-mode workflow actions (`handleAdd*`, `handleSelectModeTriggered`, `handleInsertScrapTriggered`, `handleCompleteDraftTriggered`) split from `MapEditorTab.cpp` into `MapEditorTabSelectionInspectorWorkflow.cpp`.
- [x] Phase 4 partial: MapEditor source-edit orchestration delegates (source-bounds mapping, scene/source coordinate conversion, interactive-draft coordinate-row assembly, interactive-vertex capture/control mutation, and draft commit helpers) split from `MapEditorTab.cpp` into `MapEditorTabSourceEditWorkflow.cpp`.
- [x] Phase 4 partial: MapEditor event-routing shell flow (`eventFilter`, escape-key cancellation routing, map-editor event-receiver resolution) split from `MapEditorTab.cpp` into `MapEditorTabEventRouting.cpp`; parsed-line cache and interactive line-control lookup were moved into source-edit workflow delegates and the legacy `MapEditorTab.cpp` shell TU was retired.
- [x] Phase 4 partial: MapEditor line-extension rewrite planning extracted into `MapEditorLineExtensionPlanner` and consumed by `MapEditorTabLineExtension.cpp`, with focused unit coverage for prepend/append control-handle mapping and invalid-input guards.
- [x] Phase 4 partial: MapEditor workspace/document/status delegate methods extracted from `MapEditorTabWorkspace.cpp` into `MapEditorTabWorkspaceDelegates.cpp` (file/document pass-through, mode/visibility orchestration, command-trigger delegates, and undo/redo/zoom-fit handlers) to reduce shell TU breadth while preserving behavior.
- [x] Phase 4 partial: Detached map-pane window command-surface UI extracted into `MapEditorDetachedPaneWindow`, and detached-pane lifecycle/status methods (`refreshStatus`, `toggle/detach/reattach/focus`) split from `MapEditorTabWorkspace.cpp` into `MapEditorTabDetachedPaneDelegates.cpp`.
- [x] Phase 4 partial: `MapEditorTab` declaration surface narrowed by grouping detached-pane, selection-sync, and interactive-draw/session state into focused nested state structs in `MapEditorTab.h`, with map-editor context/delegate TUs rewired to consume the grouped state fields.
- [x] Phase 4 partial: Inspector panel construction/wiring for `MapEditorTab` (Objects/Selection/Background tabs and details panel UI assembly) extracted from `MapEditorTabWorkspace.cpp` into `MapEditorTabInspectorPanelUi.cpp`, reducing workspace-shell TU breadth while preserving behavior.
- [x] Phase 4 partial: Inspector/object-details controller delegate methods extracted from `MapEditorTabControllers.cpp` into `MapEditorTabInspectorControllerDelegates.cpp`, leaving scene-lifecycle/magnifier command-surface orchestration focused in `MapEditorTabControllers.cpp`.
- [x] Phase 4 partial: `MapEditorTab` object-selection and object-details panel state grouped into focused nested state structs in `MapEditorTab.h`, with context/delegate rewiring across map-editor controllers.
- [x] Phase 4: map editor responsibilities decomposed.
- [x] Phase 5 partial: application-level platform style and appearance bootstrap (style selection, dark/light palette, chrome stylesheet, and runtime appearance watcher wiring) extracted from `main.cpp` into `src/platform/ApplicationAppearanceBootstrap.cpp`.
- [x] Phase 5 partial: application startup identity/icon/translator bootstrap extracted from `main.cpp` into `src/platform/ApplicationStartupBootstrap.cpp`, with explicit startup state ownership for translator lifetime.
- [x] Phase 5 partial: duplicated Lucide icon theming/rendering logic consolidated into `src/app/LucideIconFactory.h` and consumed by `MainWindow`, `MainWindowSidebar`, and `MapEditorDetachedPaneWindow`.
- [ ] Phase 5: platform and appearance services centralized.
- [ ] Phase 6: packaging and CI hardening completed.

## Review Scope And Current State

The review was based on the current working tree, including active uncommitted refactor work. Important files and areas inspected include:

- `CMakeLists.txt`
- `src/main.cpp`
- `src/app/MainWindow.cpp` and `src/app/MainWindow.h`
- `src/app/text_editor/TextEditorTab.cpp` and `src/app/text_editor/TextEditorTab.h`
- `src/app/text_editor/TextEditorDocumentController.*`
- `src/app/text_editor/map_editor/MapEditorTab.h`
- `src/app/text_editor/map_editor/MapEditorTabWorkspace.cpp`
- `src/app/text_editor/map_editor/MapEditorBackgroundLayers.cpp`
- `src/app/text_editor/map_editor/MapEditorSceneRenderer.cpp`
- `src/core/DocumentFile.*`
- `src/core/SessionStore.*`
- `src/core/CommandCatalogStore.*`
- `src/core/IFileSystem.h`, `src/core/ISessionStore.h`, `src/core/QtFileSystem.*`
- `src/platform/*`
- existing test wiring in `CMakeLists.txt`

The repository already has useful guardrails: core tests, app tests, GUI smoke tests, structure constraints, and platform-specific path resolver files. The refactor should strengthen those boundaries rather than bypass them.

## 1. Flaw Analysis

### 1.1 Common QWidgets Architectural Anti-Patterns

The following anti-patterns are common in mature QWidgets applications and are relevant to Therion Studio.

**God widgets**

A widget starts as a screen or shell and gradually accumulates state management, persistence, command dispatch, background execution, selection logic, file IO, settings, validation, and model synchronization. This creates classes that are hard to test without launching the whole GUI and hard to change without regressions.

Symptoms:

- Many private members and controller pointers in one widget.
- Long `.cpp` files with unrelated workflows.
- Slots that contain business rules rather than only translating UI events.
- Helper lambdas and static local functions that exist only because the class is too broad.

Current examples:

- `MainWindow` owns project/session/window orchestration, runner integration, structure browser handling, file dialogs, recent files, window geometry, status UI, and document tab coordination.
- `TextEditorTab` coordinates raw, block, and map editor modes, file persistence, dirty state, controller wiring, command catalog integration, source synchronization, and multiple callback surfaces.
- `MapEditorTab.h` is a large shared declaration surface for many map-editor responsibilities.

**Application logic in UI classes**

QWidgets should present state and emit user intent. They should not own the rules for document loading, parsing, serialization, command execution, session restore, or platform path resolution.

Current examples:

- File and session behavior is still routed through UI shells in several places.
- `MainWindow` contains direct usage of file dialogs, path handling, tab/session behavior, and runner workflows.
- Map and block editor widgets have logic that is closer to application workflow than presentation.

**Static service access and function bags**

Static functions are not automatically wrong. They are acceptable for pure stateless algorithms. They become an architectural problem when they hide IO, global state, resource loading, process execution, configuration, or caching.

Current examples:

- `DocumentFile` is a static file IO helper. This is simple, but it couples callers directly to filesystem behavior and makes failure simulation harder.
- `CommandCatalogStore` can load the built-in resource catalog, but UI shells now receive explicit store instances rather than reaching through a static facade.
- `SessionStore` now has an `ISessionStore` seam for main-window and map-editor workflows; production composition happens at the application/main-window boundary rather than hidden editor constructors.

**Service locator by constructor context**

Large context structs full of raw pointers and `std::function` callbacks avoid explicit dependencies, but they often become an untyped service locator. The code compiles, but ownership, lifetime, and responsibility are unclear.

Current examples:

- `TextEditorDocumentContext` contains many raw pointers and callbacks.
- Block/map editor controller context objects contain many callback hooks.
- Several workflows depend on implicit callback ordering instead of named application contracts.

**Callback-heavy UI architecture**

Callbacks are fine for small local event hooks. They are risky when used as the main architectural connection between controllers and views.

Risks:

- No obvious ownership contract.
- No easy way to discover all effects of a workflow.
- Re-entrancy problems in Qt event loops.
- Stale callback targets after widget teardown.
- Harder debugging than named signals, slots, interfaces, or presenter methods.

This risk is already visible in the drag-loop crash hardening work around stale callbacks.

**Blocking the UI thread**

Parsing, project scanning, background rendering, Therion execution, filesystem traversal, and large map redraws must not block the main thread.

Current state:

- Some project scanning and background work already uses asynchronous patterns, which is good.
- The historical `.xvi` spinning-ball issue shows that rendering and asset loading can still become UI-thread hazards.
- Long-running parsing/rendering should be explicitly modeled as cancellable, debounced application tasks.

**Cross-platform behavior embedded in feature code**

Qt allows platform checks anywhere, but scattered `Q_OS_*` branches make behavior hard to verify across Windows, macOS, and Linux.

Current examples:

- `src/main.cpp` includes platform-specific style selection.
- `MainWindow` and related UI code use path/config/window behavior directly.
- Platform path resolution exists under `src/platform`, but not all platform-specific behavior is centralized there.

**Over-styling and native behavior drift**

QWidgets applications often lose platform polish when they force global styles, palettes, fixed fonts, or hardcoded icons.

Current examples:

- `src/main.cpp` forces a preferred style and global palette/stylesheet choices.
- This may be intentional product styling, but it should be a documented appearance policy rather than scattered bootstrap logic.

**Build wiring as architecture debt**

When source/resource lists are manually maintained and duplicated, refactors become fragile.

Current concern:

- The current dirty `CMakeLists.txt` appears to contain duplicated argument/resource list fragments from recent edits. This should be corrected before further architecture work.
- Large explicit source/resource lists are workable, but they need structure and guardrails.

### 1.2 Application-Specific Risks

**Round-trip safety is a product-critical constraint**

Therion Studio edits structured text documents. Any refactor that touches parsing, serialization, rewrite behavior, map source synchronization, or command catalog metadata can cause semantic loss.

Therefore, architecture changes must preserve:

- Unknown directives.
- Comments.
- Formatting where practical.
- Encoding behavior.
- Source-to-map synchronization.
- Undo/redo semantics.
- Safe handling of partially unsupported Therion constructs.

**Map editor complexity is high-risk**

The map editor mixes scene rendering, selection, source rewrite, background layers, object details, style catalog, undo, and interactive drawing. It should be decomposed, but not aggressively rewritten in one step.

**Block editor and map editor share document-source concerns**

Source mutation rules should not be duplicated between Raw, Block, and Map modes. They should share core/application services for command parsing, source edits, and round-trip-safe rewrites.

## 2. Architecture And Abstraction Design

### 2.1 Target Layering

Recommended long-term structure:

```text
TherionStudio executable
  -> Presentation layer: QWidgets, dialogs, tabs, scene/view presentation
  -> Application layer: use-case services and workflow controllers
  -> Core/domain layer: document model, parsing, serialization, command metadata, geometry rules
  -> Infrastructure/platform layer: Qt filesystem, settings, processes, resources, OS integration
```

Dependency rule:

```text
Presentation -> Application -> Core
Presentation -> Infrastructure interfaces only through composition
Infrastructure -> Core interfaces/value types
Core -> no Presentation dependency
```

Qt usage rule:

- Core may depend on QtCore value types if that is already the project convention.
- Core shall not depend on QtWidgets, QGraphicsView, dialogs, windows, or UI event classes.
- Application services should avoid QtWidgets and should be testable with `QCoreApplication`.
- Infrastructure may depend on QtCore, QtGui, QtWidgets, or platform APIs as needed, but should expose narrow interfaces upward.

### 2.2 Proposed Modules

**Presentation layer**

Location today:

- `src/app/MainWindow*`
- `src/app/text_editor/*`
- `src/app/text_editor/raw_editor/*`
- `src/app/text_editor/block_editor/*`
- `src/app/text_editor/map_editor/*`

Responsibilities:

- Display view state.
- Own widgets, layouts, actions, menus, shortcuts, scene items, dialogs.
- Convert user interaction into application intents.
- Bind presenter/viewmodel state into controls.
- Contain no direct file IO, QProcess orchestration, catalog parsing, or session persistence decisions.

**Application layer**

Can initially live in `src/app/...` as non-widget classes, then move later if useful.

Suggested services:

- `DocumentManager`
- `SessionController`
- `ProjectStructureController`
- `TherionRunnerController`
- `CommandCatalogController` or `ICommandCatalogProvider`
- `TextEditorDocumentPresenter`
- `MapEditorWorkflowController`
- `BlockEditorWorkflowController`

Responsibilities:

- Open/save/reload documents.
- Manage dirty state and document identity.
- Coordinate session restore/persist.
- Execute Therion processes through an injected runner.
- Route project structure updates.
- Own high-level editor mode synchronization.
- Produce UI-neutral view state for widgets.

**Core/domain layer**

Location:

- `src/core/*`

Responsibilities:

- Therion document parsing and serialization.
- Round-trip-safe document edits.
- Command catalog data model and query logic.
- Map object domain model and geometry transformations.
- Style catalog schema and resolution rules, if kept UI-neutral.
- Encoding decisions and source text transformations.

Core should expose deterministic APIs that are easy to test with plain unit tests.

**Infrastructure/platform layer**

Locations:

- `src/platform/*`
- future `src/infrastructure/*` or equivalent adapter files

Responsibilities:

- Filesystem adapter.
- Settings/session storage adapter.
- Resource loading adapter.
- Process runner adapter.
- Platform path resolver.
- Environment/PATH resolution.
- Appearance and theme bridge.
- Native platform behavior.

### 2.3 Composition Root

The application needs one place where concrete Qt services are assembled and injected. Today this is implicitly spread across constructors and statics.

Recommended approach:

```cpp
struct ApplicationServices {
    IFileSystem& fileSystem;
    ISessionStore& sessionStore;
    ICommandCatalogProvider& commandCatalog;
    IProcessRunner& processRunner;
    IPlatformPaths& platformPaths;
};
```

The concrete instances should be created near startup, for example in `main.cpp` or a small `AppCompositionRoot` class.

Widgets should not instantiate persistent infrastructure by default except as a temporary compatibility path during migration.

### 2.4 Platform Isolation

Use narrow platform interfaces instead of scattering platform decisions through feature code.

Recommended interfaces:

```cpp
class IPlatformPaths {
public:
    virtual ~IPlatformPaths() = default;
    virtual QString appDataDirectory() const = 0;
    virtual QString userConfigDirectory() const = 0;
    virtual QStringList externalToolSearchPathCandidates() const = 0;
};

class IProcessRunner {
public:
    virtual ~IProcessRunner() = default;
    virtual void run(const ProcessRequest& request) = 0;
};

class IAppearanceService {
public:
    virtual ~IAppearanceService() = default;
    virtual AppearanceSnapshot currentAppearance() const = 0;
};
```

Rules:

- `Q_OS_*` should be allowed in `src/platform/*` and minimal startup/bootstrap code only.
- Feature code should ask a platform service for paths, environment, shell integration, and platform behavior.
- `QStandardPaths`, `QSettings`, `QProcessEnvironment`, native menu policies, and theme detection should be centralized.
- Add a structure check once migration is complete: no `Q_OS_*` outside approved files.

### 2.5 Dependency Injection Policy

Do not create interfaces for every small helper. That creates abstraction theater.

Use interfaces when:

- The dependency performs IO.
- The dependency talks to OS/platform APIs.
- The dependency launches processes.
- The dependency reads bundled/user resources.
- The dependency needs a fake in tests.
- The dependency has multiple runtime implementations.

Use concrete value services or free pure functions when:

- The logic is deterministic and stateless.
- There is no external dependency.
- Tests can call it directly.

This distinction matters for `TherionDocumentEditor`: static pure transformation functions can be acceptable. The architectural problem is not static functions by themselves; it is static functions that hide mutable state, IO, or mixed responsibilities.

## 3. Cross-Platform And Qt Best Practices

### 3.1 General Qt Practices

**Use Qt object ownership deliberately**

- Parent widgets and QObjects when Qt ownership is appropriate.
- Use `std::unique_ptr` for non-QObject services owned by a composition root.
- Use `QPointer` when storing references to QObjects that may be deleted externally.
- Avoid raw owning pointers.
- Avoid long-lived callbacks capturing widget pointers unless lifecycle is explicit.

**Prefer signals/slots for UI events**

Use Qt signals for presentation events and state changes that naturally belong to QObject lifetimes. Use direct method calls or narrow interfaces for application services.

Avoid giant callback context structs for semantic workflows.

**Keep scene items presentation-only**

QGraphicsItems should render and expose interaction affordances. They should not own source rewrite logic, file persistence, or catalog rules.

**Use model/view patterns where they fit**

Qt's model/view framework is useful for object lists, command catalogs, project structure, background layers, and selection data. It is not necessary to force a full MVVM architecture everywhere.

Recommended compromise:

- Use presenter/viewmodel contracts for complex editor panels.
- Use `QAbstractItemModel` for hierarchical/list data.
- Keep domain models independent from QWidget controls.

### 3.2 Windows

Windows-specific requirements:

- Build the application as a GUI executable, not a console executable, for release installers.
- Deploy Qt DLLs and plugins next to the executable using `windeployqt` or equivalent CMake deployment logic.
- Ensure `platforms/qwindows.dll` is installed relative to `TherionStudio.exe`.
- Do not assume the install directory is writable.
- Store user configuration under the platform application data path.
- Test installer output on a clean Windows VM, not only on a developer machine.

UI behavior:

- Avoid relying on macOS-only shortcuts or keys such as `Insert` without alternatives.
- Keep minimum window size low enough for half-screen layouts.
- Verify high-DPI scaling on 125%, 150%, and multi-monitor setups.

### 3.3 macOS

macOS-specific requirements:

- Respect the native menu bar and application menu conventions for About, Preferences/Settings, Hide, Quit.
- Avoid assuming shell `PATH` is available to GUI apps launched from Finder.
- Keep external tool path resolution in a platform service.
- Use `~/Library/Application Support/Therion Studio` for user data if that is the chosen product convention.
- Keep signing/notarization requirements separate from core application logic.

Appearance:

- Do not casually force a non-native style on macOS unless this is a deliberate product decision.
- If a custom visual identity is required, centralize it in an appearance service or style module.
- React to system dark/light mode changes through Qt APIs where possible.

### 3.4 Linux

Linux-specific requirements:

- Follow XDG locations via `QStandardPaths`.
- Test both Wayland and X11 sessions where practical.
- Use `QIcon::fromTheme` with bundled fallback icons.
- Do not rely on one desktop environment's font, theme, or file dialog behavior.
- Package runtime assumptions explicitly for the chosen distribution format.

### 3.5 High-DPI And Graphics

Qt 6 handles much high-DPI behavior automatically, but graphics/editor code still needs care.

Recommendations:

- Use SVG or high-resolution assets for toolbar icons.
- Avoid hardcoded device-pixel values in map rendering.
- Express editor geometry in scene/document units and convert to device-aware visual sizes at paint time.
- Use `QStyleOptionGraphicsItem::levelOfDetailFromTransform()` for LOD-aware rendering.
- Keep hit targets usable independently of visual stroke width.
- Cache expensive graphics only when invalidation rules are explicit.

### 3.6 Theme, Icons, Fonts, And Style

Recommended approach:

- Centralize appearance decisions in one module.
- Prefer system fonts unless a product-specific font is explicitly required.
- Use native icons where appropriate and resource fallbacks where consistency matters.
- Avoid broad global stylesheets. They often break native metrics and platform details.
- If global styles are used, document the intended product policy and keep it testable.

## 4. Testability Strategy

### 4.1 Test Pyramid

Recommended test layers:

**Core unit tests**

- No widgets.
- No full GUI startup.
- Can run under `QCoreApplication` or no Qt application object where possible.
- Cover parsing, serialization, catalog schema, style schema, source edits, geometry transforms, and round-trip behavior.

**Application service tests**

- Use fake infrastructure: fake filesystem, fake session store, fake command catalog, fake process runner.
- Verify document workflows, dirty-state transitions, session restore/persist, runner command construction, and failure handling.
- Should run headless on all CI platforms.

**Widget/presenter tests**

- Use `QT_QPA_PLATFORM=offscreen` where supported.
- Test view binding, signal routing, menu/action enablement, selection-panel behavior, and smoke interactions.
- Do not depend on real external tools or real user directories.

**Packaging smoke tests**

- Verify installer/runtime layout on each platform.
- On Windows, verify Qt platform plugin discovery from a clean installation.
- On macOS, verify `.app` bundle layout and GUI launch environment.
- On Linux, verify runtime resource lookup and XDG paths.

### 4.2 Immediate Testability Improvements

The current `IFileSystem` and `ISessionStore` direction is useful. Finish it with fakes and tests.

Suggested fakes:

```cpp
class FakeFileSystem final : public IFileSystem {
public:
    QHash<QString, QString> files;
    QString errorToReturn;

    FileReadResult readTextFile(const QString& path) const override;
    FileWriteResult writeTextFile(const QString& path, const QString& text) override;
};
```

```cpp
class FakeSessionStore final : public ISessionStore {
public:
    QStringList openTabs;
    QByteArray mainWindowGeometry;
    // Store only the state needed by tests.
};
```

The goal is not to test Qt file IO. The goal is to test application behavior when reads fail, writes fail, session data is invalid, or paths are missing.

### 4.3 Decouple UI Behavior From Business Logic

For each complex screen, introduce a presenter or workflow controller that has no QWidget dependency.

Example shape:

```cpp
class TextEditorDocumentPresenter final : public QObject {
    Q_OBJECT
public:
    TextEditorDocumentPresenter(IFileSystem& fileSystem,
                                ICommandCatalogProvider& commandCatalog,
                                QObject* parent = nullptr);

    OpenDocumentResult open(const QString& path);
    SaveDocumentResult save(const DocumentSnapshot& snapshot);
    TextEditorViewState viewState() const;

signals:
    void viewStateChanged(const TextEditorViewState& state);
};
```

The widget then binds controls to `TextEditorViewState` and forwards user actions to the presenter.

### 4.4 Round-Trip And Regression Tests

Any refactor touching source mutation should include:

- Golden-file tests for existing fixtures.
- Round-trip tests for parse/serialize/edit/serialize workflows.
- Targeted tests for unknown directives and comments.
- Tests for multiline command continuations.
- Tests for map object type/subtype rewrites.
- Tests for area/line ownership and referenced borders.

### 4.5 CI/CD Strategy

Recommended CI jobs:

- `core-tests`: fast unit tests, no widgets.
- `app-service-tests`: headless application service tests with fakes.
- `gui-smoke-tests`: QWidgets smoke tests using offscreen/minimal platform where practical.
- `windows-installer`: build installer and verify runtime layout.
- `macos-bundle`: build `.app`, verify resource and plugin layout.
- `linux-build`: build and run tests under Linux desktop-compatible environment.
- `structure-constraints`: enforce directory, dependency, and platform macro rules.

## 5. Refactoring Roadmap

### Phase 0 - Stabilize The Current Refactor

Goal: make the current dirty architecture work build-clean before adding more abstractions.

Actions:

- Fix current `CMakeLists.txt` drift before deeper changes.
- Ensure source/resource lists are valid and not accidentally duplicated.
- Build and run the existing unit test suite.
- Verify the app still launches on the local platform.
- Keep `IFileSystem`, `ISessionStore`, and related adapters only if they are fully wired and tested.
- Add or update a short architecture note describing the intended dependency direction.

Exit criteria:

- Clean build.
- Existing tests pass or failures are documented as unrelated.
- No partially wired service abstraction is left unused or misleading.

### Phase 1 - Complete Low-Risk Dependency Injection

Goal: finish the seams that have already been started.

Actions:

- Finish `IFileSystem` use in document workflows.
- Add `FakeFileSystem` tests for open/save/failure paths.
- Finish `ISessionStore` use in session workflows.
- Remove or quarantine static `SessionStore` access behind an adapter.
- Introduce `ICommandCatalogProvider` or make `CommandCatalogStore` injectable where command metadata is required.
- Keep static catalog access only as a transitional facade if needed.

Exit criteria:

- Document/session/catalog workflows can be tested without real files, real settings, or static global state.

### Phase 2 - Extract MainWindow Application Services

Goal: turn `MainWindow` into a shell that owns widgets/actions and delegates workflows.

Recommended extraction order:

1. `DocumentManager`
2. `SessionController`
3. `ProjectStructureController`
4. `TherionRunnerController`
5. `RecentFilesController`
6. `MainWindowViewState` or equivalent action-state binder

Actions completed in this phase:

- Session restore/save decisions moved into `MainWindowSessionController`, `MainWindowSessionDocumentController`, `MainWindowSessionProjectService`, `MainWindowSessionStateService`, `MainWindowSessionWindowRestoreService`, and related orchestration/presentation helpers.
- Project open/close workflow moved into `MainWindowProjectController` with lifecycle/workspace/orchestration/UI-flow services.
- External Therion execution/config resolution workflow moved into `MainWindowTherionRunnerController` while keeping dialogs/widgets in `MainWindow`.
- Thin execution-only layers consolidated (removed `*StepExecutor` wrappers) to reduce file count and keep execution near workflow controllers.
- Open/save/reload document-tab workflow extraction into a dedicated `DocumentManager` is intentionally deferred to the next phase because it is tightly coupled to mode-specific editor shells (`TextEditorTab`/`MapEditorTab`) that are already targeted by Phase 3 decomposition.

Exit criteria (met for Phase 2 scope):

- `MainWindow` no longer directly owns session persistence, project open/close decisioning, or Therion run/resolve rules.
- Main high-risk workflows (session restore/persist, project open/close, Therion run/stop/config resolution) are testable through focused services/controllers without a real window.

### Phase 3 - Reduce TextEditorTab Coupling

Goal: make the text editor shell coordinate editor modes without owning document rules.

Actions:

- Define a `TextEditorDocumentPresenter` or `TextEditorDocumentModel` for document state.
- Replace raw-pointer callback contexts with named contracts where workflows are semantic.
- Keep callbacks only for local, short-lived UI hooks.
- Move source rewrite and command-editing logic into shared non-UI services.
- Keep Raw, Block, and Map modes as peer presentations over one document model.

Exit criteria:

- `TextEditorTab` owns widgets and mode switching.
- Document loading/saving/source mutation can be tested independently.
- Mode synchronization has explicit APIs rather than hidden callback chains.

### Phase 4 - Decompose Map Editor Responsibilities

Goal: split map-editor responsibilities without changing map behavior.

Recommended extraction order:

1. Background layer model/controller.
2. Scene renderer contract.
3. Selection details presenter.
4. Interactive draw controller.
5. Source rewrite coordinator.
6. Style catalog resolver.

Actions:

- Keep QGraphicsItems presentation-only.
- Move map object selection state into a UI-neutral model.
- Move inspector binding into presenter/viewmodel classes.
- Keep LOD rendering logic focused and testable where possible.
- Add regression tests for map object type/subtype changes and area/line rewrite behavior.

Exit criteria:

- `MapEditorTab.h` becomes a narrow shell declaration rather than a shared declaration surface for all map subsystems.
- Map rendering, selection, background handling, and source mutation are independently understandable.

### Phase 5 - Platform Services And Appearance

Goal: centralize cross-platform behavior.

Actions:

- Introduce platform services for paths, external tool environment, and OS integration.
- Move scattered `Q_OS_*` checks into `src/platform` and bootstrap code.
- Add a structure-constraint check to enforce this rule.
- Centralize appearance policy, palette, style, icons, and high-DPI assumptions.
- Decide explicitly whether the app prefers native platform styling or a product-specific cross-platform style.

Exit criteria:

- Feature code no longer branches on OS macros.
- User data paths are platform-correct and testable.
- Appearance behavior is owned by one module.

### Phase 6 - Packaging And CI Hardening

Goal: prevent platform regressions from returning.

Actions:

- Add Windows installer smoke verification for executable, DLLs, and `platforms/qwindows.dll` layout.
- Add macOS bundle resource/plugin verification.
- Add Linux resource/path verification.
- Run headless core/application tests on all three platforms.
- Keep GUI smoke tests minimal but representative.

Exit criteria:

- Packaging failures are caught before release artifacts are shared.
- Cross-platform assumptions are verified continuously.

## 6. Cross-Check Against The Claude Draft

### 6.1 Strong Agreements

Claude's review is directionally correct on the major architectural issues.

Agreements:

- `MainWindow` is too broad and should become a thinner shell.
- `TextEditorTab` is too central and acts as an implicit service hub.
- Callback-heavy context structs are a real maintainability and lifecycle risk.
- `SessionStore` should not be both a static global utility and an injectable service.
- File IO should be behind an injectable abstraction for workflow tests.
- A `DocumentManager` extraction is a good next major seam.
- Platform-specific behavior needs a stronger boundary.
- The app should move toward testable application services and away from widget-owned workflows.

### 6.2 Important Differences

**Difference 1: Do not over-abstract everything**

Claude recommends many pure virtual interfaces. That is useful for IO, settings, processes, platform services, and resource loading. It is not necessary for every deterministic helper.

Preferred rule:

- Interface external dependencies.
- Keep pure transformations as direct, testable functions or concrete services.

**Difference 2: Static functions are not automatically architectural debt**

Claude identifies `TherionDocumentEditor` as a static utility problem. That may be true if it mixes responsibilities, hidden state, or IO. But pure stateless document transformation functions can be acceptable and very testable.

The actual rule should be:

- Static pure algorithm: acceptable.
- Static IO/global state/resource cache: avoid or wrap.
- Static mixed workflow coordinator: refactor.

**Difference 3: Widgets may depend on stable domain value types**

Claude's proposed strict layer rule suggests presentation should not include core headers directly. That can become too ceremonial in a Qt desktop app.

Better rule:

- Widgets may consume immutable domain value types and UI-neutral view models.
- Widgets shall not own parsing, serialization, file IO, session persistence, process execution, or platform path rules.

**Difference 4: Replacing all callbacks with interfaces is too absolute**

Callbacks are not always wrong. They are appropriate for small local hooks and one-shot actions. The problem is large callback context objects that encode application workflows without names.

Preferred rule:

- Use Qt signals for UI event notification.
- Use presenter/service methods for use cases.
- Use small callbacks only for local implementation details.
- Replace large semantic callback contexts with named contracts.

**Difference 5: `SessionStore` placement should be pragmatic during migration**

Claude's `ISessionStore` direction is right. However, moving all QSettings implementation out of core immediately may create churn. It is acceptable as a short migration step if the interface is stable and tests use a fake.

Long-term target:

- Interface in application/core boundary.
- Qt implementation in infrastructure.

**Difference 6: Style policy needs a product decision, not only a Qt best-practice answer**

Claude recommends more platform-native behavior. This is generally right, but Therion Studio may intentionally have a strong cross-platform editor style. The important requirement is consistency and explicit ownership.

Preferred rule:

- Choose native style or product-specific style intentionally.
- Centralize the decision.
- Test it on Windows, macOS, and Linux.

**Difference 7: Timeline estimates are optimistic**

The Claude plan estimates some deep extractions in days. Given map/text synchronization and round-trip safety, those estimates are likely optimistic.

Realistic expectation:

- Stabilization and DI seams: small increments.
- MainWindow extraction: moderate effort.
- TextEditorTab and MapEditor decomposition: multi-slice work with regression tests after each slice.

**Difference 8: Build wiring drift must be addressed first**

Claude's plan does not sufficiently emphasize build-system stabilization. The current tree shows signs of accidental CMake duplication from recent edits. That must be fixed before deeper architecture work.

### 6.3 Verdict On The Claude Draft

The Claude draft was useful directional input, but this file is now the source of truth for the optimization plan. Apply this stricter engineering filter:

- Stabilize first.
- Extract along existing seams.
- Avoid abstraction theater.
- Keep tests green after each slice.
- Preserve Therion round-trip behavior as the primary constraint.
- Treat QWidgets as a valid presentation technology, not as the source of the architecture problem.

## 7. Recommended Target Architecture Sketch

### 7.1 Application Services

```cpp
class DocumentManager final : public QObject {
    Q_OBJECT
public:
    DocumentManager(IFileSystem& fileSystem,
                    ICommandCatalogProvider& commandCatalog,
                    QObject* parent = nullptr);

    OpenDocumentResult openDocument(const QString& path);
    SaveDocumentResult saveDocument(const DocumentId& id);
    SaveDocumentResult saveDocumentAs(const DocumentId& id, const QString& path);

signals:
    void documentOpened(const DocumentSnapshot& snapshot);
    void documentChanged(const DocumentSnapshot& snapshot);
    void documentError(const UserVisibleError& error);
};
```

```cpp
class SessionController final : public QObject {
    Q_OBJECT
public:
    explicit SessionController(ISessionStore& store, QObject* parent = nullptr);

    SessionSnapshot loadSession() const;
    void saveSession(const SessionSnapshot& snapshot);
};
```

```cpp
class TherionRunnerController final : public QObject {
    Q_OBJECT
public:
    TherionRunnerController(IProcessRunner& runner,
                            IPlatformPaths& platformPaths,
                            QObject* parent = nullptr);

    void runCompile(const TherionRunRequest& request);
};
```

### 7.2 Widget Binding Pattern

```cpp
class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(DocumentManager& documents,
               SessionController& sessions,
               TherionRunnerController& runner,
               QWidget* parent = nullptr);

private slots:
    void onOpenTriggered();
    void onSaveTriggered();

private:
    DocumentManager& documents_;
    SessionController& sessions_;
    TherionRunnerController& runner_;
};
```

The widget owns UI controls and forwards intent. The service owns workflow rules.

### 7.3 Resource And Override Loading

Recommended shape:

```cpp
class IResourceCatalogLoader {
public:
    virtual ~IResourceCatalogLoader() = default;
    virtual CatalogLoadResult loadCommandCatalog() const = 0;
    virtual StyleCatalogLoadResult loadMapObjectStyles() const = 0;
};
```

A Qt implementation can load bundled `:/resources/...` files first and user overrides from platform application data second. The rest of the app should not know where those files physically live.

## 8. Immediate Quick Wins

These are low-risk and should happen before deep refactoring.

- Fix accidental `CMakeLists.txt` drift and verify build/test health.
- Add tests around `IFileSystem` and `ISessionStore` fakes.
- Stop adding new direct `DocumentFile` calls from widgets.
- Keep command catalog loading at composition/test setup boundaries; do not reintroduce UI-side static catalog access.
- Add a short architecture boundary note near this plan or developer docs.
- Add structure checks for new editor-mode files so they stay under the stable directory layout.
- Identify all remaining direct `Q_OS_*` usage and classify each as bootstrap, platform, or migration debt.
- Introduce a single `Appearance` module before further style/palette changes.

## 9. Non-Goals

The refactor should not do the following unless explicitly requested:

- Rewrite the UI in QML.
- Replace QWidgets.
- Introduce non-Qt third-party dependencies.
- Redesign the document model and UI simultaneously.
- Move every helper behind an interface.
- Break Therion source round-trip behavior for architectural purity.
- Change user workflows as a side effect of internal cleanup.

## 10. Final Recommendation

The application is ready for a disciplined architectural refactor, but not a broad rewrite. The best path is staged extraction around workflows that already hurt testability:

1. Stabilize build and current DI changes.
2. Finish filesystem/session/catalog seams.
3. Extract MainWindow workflows into application services.
4. Reduce TextEditorTab callback coupling.
5. Decompose MapEditorTab around renderer, selection, background layers, style resolution, and source rewrite boundaries.
6. Centralize platform and appearance behavior.
7. Harden CI and installer smoke verification across Windows, macOS, and Linux.

The main engineering principle should be: keep UI shells thin, keep domain logic deterministic, keep platform behavior injectable, and protect round-trip document safety with tests at every step.
