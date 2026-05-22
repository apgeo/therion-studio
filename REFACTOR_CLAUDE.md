# Architectural Review — Therion Studio (Qt 6 / C++17, cross-platform)

**Reviewer:** Senior C++/Qt Architect  
**Based on:** actual repository state, not a hypothetical snapshot  
**Grounding files:** `CMakeLists.txt` (1 054 lines), `MainWindow.h/cpp` (294 / 2 203 lines), `TextEditorTab.h/cpp` (440 / 1 089 lines), `src/core/`, `src/app/`, all test targets  

---

## Executive Summary

The codebase shows meaningful, recent architectural progress — `TherionRunnerService`, `ProjectStructureScanner`, and the full suite of `TextEditor*Controller` extractions are **good engineering decisions** and demonstrate the right instinct. The remaining pain points are concentrated in three areas: the **CMake build system** (source duplication, no libraries, disabled tests), the **friend-web coupling** inside `TextEditorTab`, and the **`SessionStore` / `CommandCatalogService` static-only APIs** that resist substitution in tests.

The most valuable near-term investments are ordered below by impact-per-effort ratio.

---

## 1. Flaw Analysis

### 1.1 God Window — `MainWindow`

`MainWindow.h` exposes 80+ private data members spanning at least six distinct responsibilities:

| Responsibility | Representative members |
|---|---|
| Tab management | `editorTabs_`, `clearingDocumentTabs_`, `detachedMapWindowsByPath_` |
| Sidebar layout | `sidebarContainer_`, `sidebarPages_`, `sidebarCollapsed_`, `sidebarExpandedWidth_` |
| Console | `consoleView_`, `consoleSidebarPage_`, `consoleCollapsed_` |
| Therion runner UI | `therionExecutableEdit_`, `therionRunButton_`, `therionProcess_` (now partially extracted) |
| Structure browser | `structureModel_`, `structureTree_`, `structureNameOverrides_` |
| Workspace toolbar | `workspaceModeSwitcher_` + 22 more toolbar widget pointers |

Each responsibility is wired to the others through `private` method calls in the same `2 203`-line `.cpp`. Adding a feature to the console requires understanding the structure browser because they share the same sidebar container.

**Pattern:** God Window / God Widget. Standard mitigation is decomposition into focused `QObject`-based sub-controllers, which the `TextEditor*Controller` extractions already prove works here.

### 1.2 Friend-Web Coupling in `TextEditorTab`

`TextEditorTab.h` lists **29 `friend` class declarations**:

```cpp
friend class TextEditorContextHelpController;
friend class TextEditorCursorController;
// ... 27 more ...
friend class BlockEditorDeleteExecutor;
```

This pattern appears after a structural refactoring: the controllers were extracted from the monolith but still need access to private state, so `friend` was the path of least resistance. The result is that `TextEditorTab`'s entire private section is effectively public to 29 other translation units. Adding, removing, or renaming any private member of `TextEditorTab` requires auditing 29 other files.

**Correct mitigation:** replace friend access with narrow, explicitly-typed context objects passed to each controller at construction:

```cpp
// Instead of friend access to 50+ private fields:
struct RawEditorContext {
    QPlainTextEdit *editor;           // only what RawEditor needs
    const TherionSyntaxHighlighter *highlighter;
    QStringListModel *completionModel;
};

RawEditorFindController findController(RawEditorContext{editor_, highlighter_, completionModel_}, this);
```

Each controller documents exactly which state it touches. Changing anything else in `TextEditorTab` does not require recompiling all 29 friends.

### 1.3 Static-Only Core APIs

`TherionDocumentParser`, `TherionDocumentEditor`, `ProjectStructureIndex`, and `CommandCatalogService` expose only `static` methods:

```cpp
class TherionDocumentParser final {
public:
    static TherionParsedLine parseLine(const QString &line, int lineNumber = 0);
    static QVector<TherionParsedLine> parseText(const QString &text);
};
```

Pure-static classes are not inherently bad — they are effectively namespaced free functions, which is fine for stateless transformation logic. The problem is `SessionStore`:

```cpp
class SessionStore final {
public:
    static QString lastProjectPath();
    static void setLastProjectPath(const QString &projectPath);
    // ...14 methods total
};
```

`SessionStore` wraps `QSettings`, which writes to the real OS settings store on every call. Tests that exercise any code path touching `SessionStore` produce side effects in the developer's actual application preferences. There is currently a `SessionStoreTest.cpp` that reads and writes real `QSettings` keys. That is not a unit test; it is a system test with side effects.

`CommandCatalogService` loads a JSON resource file via `QFile`. Tests that require completion data must embed the resource, which is why `TextEditorCaretInteractionTest` includes `qt_add_resources(... therion_command_catalog.json ...)` — a 150-source-file test target just to get completion working.

### 1.4 Monolithic CMake Target and Test Source Duplication

`CMakeLists.txt` has one `qt_add_executable(TherionStudio ...)` listing ~200 source files. The test targets re-list subsets of these files from scratch. The worst case is `TextEditorCaretInteractionTest`, which re-lists approximately **140 source files** — essentially the entire application minus `MainWindow*`.

Consequence:
- Every change to a shared source file forces rebuilding both the app and every test that re-lists it. Full rebuilds are expensive.
- The test CMakeLists block for a single test is ~150 lines. Maintaining the test means maintaining the source list twice.
- `TextEditorCaretInteractionTest` and `TextEditorCompletionHighlightTest` are currently `DISABLED TRUE` — they are tagged `LABELS "ui"` but set `DISABLED`. If they fail on the offscreen platform, the entire UI test layer silently does not run.

### 1.5 Hardcoded macOS Platform Paths in a Cross-Platform Service

`TherionRunnerService.cpp` (line 147):

```cpp
candidates.append(QStringLiteral("/opt/homebrew/bin/therion"));
candidates.append(QStringLiteral("/usr/local/bin/therion"));
```

These are macOS-specific Homebrew paths embedded in a service that is also compiled on Windows and Linux. The service already uses `qEnvironmentVariable("HOMEBREW_PREFIX")` for the dynamic prefix, which is the right approach — the hardcoded fallbacks are just belt-and-suspenders coverage for systems where `HOMEBREW_PREFIX` is not set. However, the fallbacks should live in a platform-specific resolver function, not inline in the generic service, so the Windows and Linux builds do not silently include dead macOS paths.

### 1.6 No Localization Infrastructure

Every user-visible string is correctly wrapped in `tr()`. However, there is no `qt_add_translations`, no `.ts` files, no `QTranslator` installation in `main.cpp`, and no `lupdate` step. The `tr()` calls are a necessary first step for localization, but without the tooling they remain inert. Therion itself has Czech, German, French, and Spanish documentation — its users will expect the studio to be localizable.

### 1.7 No CI/CD

There is no `.github/workflows/` directory in this repository. Quality gates (warnings-as-errors, sanitizers, cross-platform builds) exist only on the developer's local machine. Platform-specific regressions on Windows or Linux are not caught until someone manually builds there.

---

## 2. Architecture & Abstraction Design

### 2.1 Recommended Layer Structure

The existing code already maps to a layered structure. The goal is to **make the layers explicit** by putting them in separate CMake targets with enforced dependency rules:

```
┌─────────────────────────────────────────────────────┐
│  therion_ui  (QWidgets, QGraphicsScene, QTabWidget)  │
│  MainWindow, TextEditorTab, MapEditorTab             │
│  All *Controller and *Presenter classes              │
│  Depends on: therion_app, therion_core               │
├─────────────────────────────────────────────────────┤
│  therion_app  (QObject, signals/slots, QProcess,     │
│                QtConcurrent)                         │
│  TherionRunnerService, ProjectStructureScanner       │
│  SessionStore, CommandCatalogService                 │
│  Depends on: therion_core                            │
├─────────────────────────────────────────────────────┤
│  therion_core  (QtCore only — QString, QVector, etc.)│
│  TherionDocumentParser, TherionDocumentEditor        │
│  ProjectStructureIndex, DocumentFile                 │
│  TherionCommandSyntax, MapBackgroundPlacement        │
│  TherionXviParser, TherionBackgroundMetadata         │
│  No QWidget, no QProcess, no QGraphicsScene          │
└─────────────────────────────────────────────────────┘
```

`therion_core` can be built and tested **without `QApplication`**. Tests for it are fast, headless, and require no platform display.

`therion_app` contains QObject-based services. Tests for it need `QCoreApplication` (not `QApplication`) and can run headless.

`therion_ui` contains all widget code. Tests for it need `QApplication` and the offscreen platform plugin.

### 2.2 OS-Specific Platform Isolation

Currently the only platform-specific code sits in two places: `main.cpp` (style selection, dark-mode watcher) and `TherionRunnerService.cpp` (Homebrew paths). Both are small and manageable. The right structure for future growth:

```
src/
  platform/
    PlatformPathResolver.h         ← interface (pure, no Qt widget deps)
    PlatformPathResolver.cpp       ← default implementation (env vars, PATH)
    mac/MacPlatformPathResolver.cpp ← Homebrew probing, .app bundle resource paths
    win/WinPlatformPathResolver.cpp ← Registry queries, %PROGRAMFILES%, known folders
    linux/LinuxPathResolver.cpp    ← XDG paths, distribution package locations
```

`PlatformPathResolver.h` defines a plain struct or a free function set — no virtual dispatch needed for a desktop app with one platform active per build:

```cpp
namespace TherionStudio::Platform {
    // Returns ordered list of candidate therion executable paths for this platform.
    // Falls back to PATH search if none are found.
    QStringList therionExecutableCandidates();

    // Returns the writable data directory for application state.
    // Wraps QStandardPaths but resolves sandbox paths on macOS/Linux Flatpak.
    QString appDataDirectory();
}
```

CMake selects the right implementation:

```cmake
add_library(therion_platform STATIC src/platform/PlatformPathResolver.cpp)
if(APPLE)
    target_sources(therion_platform PRIVATE src/platform/mac/MacPlatformPathResolver.cpp)
elseif(WIN32)
    target_sources(therion_platform PRIVATE src/platform/win/WinPlatformPathResolver.cpp)
else()
    target_sources(therion_platform PRIVATE src/platform/linux/LinuxPathResolver.cpp)
endif()
```

`TherionRunnerService` calls `Platform::therionExecutableCandidates()` instead of listing paths inline. The platform-specific `.cpp` is the only file that changes between OS builds.

### 2.3 Decomposing `MainWindow`

`MainWindow` should be reduced to a **compositor** — a thin shell that owns sub-controllers and wires their signals together:

```cpp
class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    // Focused sub-controllers — each owns its widgets and emits typed signals
    std::unique_ptr<SidebarController>         sidebar_;
    std::unique_ptr<EditorTabsController>      editorTabs_;
    std::unique_ptr<TherionRunnerPanelController> runnerPanel_;
    std::unique_ptr<WorkspaceToolbarController> toolbar_;
    std::unique_ptr<ProjectStructureController> structureBrowser_;
    std::unique_ptr<DocumentStatusController>  statusBar_;
};
```

Each sub-controller:
- Is a `QObject` parented to `MainWindow` (not `unique_ptr`-owned in production — see note below)
- Owns the widgets it directly manages
- Exposes a narrow public API (signals for upward communication, slots for downward)
- Has **no** `friend` access to anything

> **Note on `unique_ptr` vs `QObject` parenting:** Use `QObject` parent-child ownership for controllers that are children of `MainWindow`. `unique_ptr<T>` with a QObject-parented `T` works correctly only if the `unique_ptr` is destroyed *before* the parent. In a single-window app this is fine since `MainWindow` outlives all children. Using `QPointer<T>` for observing lifetime and raw parent-owned pointer for ownership is the idiomatic Qt pattern and avoids confusion.

### 2.4 Replacing `SessionStore` Static API

`SessionStore` should gain a thin instance-based wrapper used for construction-injection in tests:

```cpp
// Production singleton — backward-compatible façade
class SessionStore final {
public:
    static SessionStore &instance();  // returns the real QSettings-backed store
    
    QString lastProjectPath() const;
    void setLastProjectPath(const QString &path);
    // ...

private:
    explicit SessionStore();  // reads from QSettings on construction
    QSettings settings_;
};

// In tests: construct a SessionStore backed by a temp QSettings
SessionStore testStore(QSettings::IniFormat, QSettings::UserScope, 
                       "test-org", "test-app");
```

Alternatively, extract a `ISessionStore` interface only if there is a **concrete second implementation** (e.g., project-local settings overriding user-global settings). Do not create interfaces for hypothetical future needs.

---

## 3. Cross-Platform & Qt Best Practices

### 3.1 Platform Style and Dark/Light Mode

`main.cpp` already handles this correctly:

- `preferredPlatformStyle()` selects Fusion on all platforms (consistent appearance)
- `ApplicationAppearanceWatcher` re-applies the custom stylesheet on `ApplicationPaletteChange`
- `QStyleHints::colorSchemeChanged` triggers stylesheet re-application

**One gap:** the per-widget stylesheets in `MainWindow.cpp` and `workspaceCommandBarStyleSheet()` use `palette(mid)`, `palette(base)`, etc., which are palette-role lookups and will update automatically on scheme change — **but only if the widget has `WA_StyledBackground` set**. Without that flag, Qt may not repaint the widget after a palette change. Audit all widgets that carry custom stylesheets and ensure:

```cpp
widget->setAttribute(Qt::WA_StyledBackground, true);
```

`workspaceModeSwitcher_` already sets this correctly. Apply it consistently everywhere.

**High-DPI:** Qt 6 handles HiDPI automatically with integer and fractional scaling. No `AA_EnableHighDpiScaling` flag is needed (it is the default and the old flag is ignored). The Lucide SVG icon set already ensures icons look correct at all densities — this is the right choice and requires no changes.

### 3.2 macOS-Specific Behaviors

- **Native menu bar:** `QMenuBar::setNativeMenuBar(true)` is the default on macOS in Qt 6. The existing code is correct. Be aware: when the app has no open windows (all closed), the menu bar remains and menu actions still fire. Any handler that calls `currentDocumentWidget()` must handle `nullptr` gracefully.
- **Retina and Info.plist:** `CMakeLists.txt` uses `MACOSX_BUNDLE` but there is no custom `Info.plist`. Add one with at minimum:
  ```xml
  <key>NSHighResolutionCapable</key><true/>
  <key>LSMinimumSystemVersion</key><string>12.0</string>
  <key>CFBundleShortVersionString</key><string>${PROJECT_VERSION}</string>
  ```
- **Keyboard shortcuts:** `QKeySequence::StandardKey` enums (e.g., `QKeySequence::Save`, `QKeySequence::Undo`) map correctly to Cmd on macOS and Ctrl on Windows/Linux. Prefer these over hardcoded `Ctrl+S` strings.

### 3.3 Windows-Specific Behaviors

- **File associations:** Register `.th` and `.th2` file extensions via NSIS or `AppUserModelId` during install so files open in Therion Studio by double-click.
- **Console output:** The existing `QProcess` integration for therion works on Windows, but `QProcess::setCreateProcessArgumentsModifier` may be needed if therion requires specific console flags (e.g., to suppress a console window flash on launch). Consider `QProcess::setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args){ args->flags |= CREATE_NO_WINDOW; })` on Windows builds.
- **Path separators:** `QDir::fromNativeSeparators` and `QDir::toNativeSeparators` are already used throughout the codebase. Continue this consistently; never call `.replace("\\", "/")` manually.

### 3.4 Linux-Specific Behaviors

- **Wayland vs X11:** `QGraphicsScene` drag-and-drop has known behavior differences under Wayland in Qt 6.5–6.7. If `MapEditorDragUndoRedoSmokeTest` is in CI, the test matrix must specify `QT_QPA_PLATFORM=xcb` explicitly for Linux runners (not just `offscreen`) to exercise X11 drag behavior.
- **Font availability:** Fonts like "Menlo" (macOS) or "Consolas" (Windows) are not available on Linux. Use `QFontDatabase::systemFont(QFontDatabase::FixedFont)` to obtain a monospace font rather than naming a specific family. Fallback chain: `["Consolas", "Menlo", "DejaVu Sans Mono", "Monospace"]` in decreasing preference.
- **AppImage/Flatpak:** `QProcess::start()` for therion will fail in a Flatpak sandbox unless `therion` is a portal-approved host command or is bundled. Design the Therion runner UI to gracefully show an actionable error if the executable is not found, rather than silently doing nothing (which the current `TherionRunnerStartResultPresenter` does handle, but the error text mentions `/opt/homebrew/` — a macOS path shown to Linux users).

### 3.5 Signal/Slot Best Practices

The codebase uses modern function-pointer connect syntax (`&Class::signal`, `&Class::slot`) throughout, which is correct. Two additional guidelines:

**Cross-thread connections:** `QFutureWatcher::finished` and `QFutureWatcher::resultReadyAt` are emitted from a thread pool thread. The `Qt::QueuedConnection` is chosen automatically by `Qt::AutoConnection` when the receiver is in a different thread. This is already handled correctly by `ProjectStructureScanner`. Document this assumption explicitly with a comment — future maintainers should know why `QtConcurrent::run` results are safe to receive in slots.

**Lambda captures in `connect`:** Some `connect` calls in `MainWindow.cpp` capture `this` in lambdas without a lifetime guard:

```cpp
// RISKY: if the lambda outlives MainWindow (e.g., via a queued connection on a deleted object)
connect(someObject, &SomeClass::signal, this, [this]() { /* uses this */ });
```

Use the context-object overload of `connect` (already the Qt 6 default when passing `this` as the third argument) so the connection is automatically cleaned up when `this` is destroyed. Passing `this` as the context object is already done in most places; ensure it is done *everywhere* lambda connections capture `this`.

---

## 4. Build System & Dependency Management

### 4.1 Target Decomposition — The Highest Leverage CMake Change

Extract `src/core/` into a standalone `therion_core` STATIC library immediately. It has **zero QWidget dependencies** and all its sources are already re-listed verbatim in every test target. This is the one CMake change that:

- Eliminates source duplication in 9 of the 13 test targets
- Enables rebuilding core tests without rebuilding any widget code
- Makes the dependency graph visible and enforced by the linker

```cmake
# CMakeLists.txt — Phase A addition

add_library(therion_core STATIC
    src/core/TherionDocumentParser.cpp   src/core/TherionDocumentParser.h
    src/core/TherionDocumentEditor.cpp   src/core/TherionDocumentEditor.h
    src/core/ProjectStructureIndex.cpp   src/core/ProjectStructureIndex.h
    src/core/DocumentFile.cpp            src/core/DocumentFile.h
    src/core/TherionCommandSyntax.cpp    src/core/TherionCommandSyntax.h
    src/core/TherionXviParser.cpp        src/core/TherionXviParser.h
    src/core/TherionBackgroundMetadata.cpp
    src/core/MapBackgroundPlacement.cpp
    src/core/SessionStore.cpp            src/core/SessionStore.h
    src/core/CommandCatalogService.cpp   src/core/CommandCatalogService.h
)
target_link_libraries(therion_core PUBLIC Qt6::Core)
target_compile_definitions(therion_core PUBLIC QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
set_target_properties(therion_core PROPERTIES POSITION_INDEPENDENT_CODE ON)  # Linux

# Tests now just link:
target_link_libraries(TherionDocumentEditorTest PRIVATE therion_core Qt6::Test)
```

**Qt static library caveat:** `qt_add_resources` embedded in a static library requires the application to call `Q_INIT_RESOURCE(resourceName)` at startup, otherwise the resources are linked in but not registered. For `CommandCatalogService` (which loads `therion_command_catalog.json`), either:
- Keep resources in the executable target and load them from there, or
- Call `Q_INIT_RESOURCE` in `therion_core`'s public init function

### 4.2 Recommended Full CMake Structure

```cmake
cmake_minimum_required(VERSION 3.21)
project(TherionStudio VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)   # see §4.4
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Svg Concurrent Test)
qt_standard_project_setup()

# ── Layer 1: pure QtCore domain logic ──────────────────────────────────────
add_library(therion_core STATIC ...)
target_link_libraries(therion_core PUBLIC Qt6::Core)

# ── Layer 2: platform path resolution ──────────────────────────────────────
add_library(therion_platform STATIC ...)
# per-platform sources via if(APPLE)/if(WIN32)/else()
target_link_libraries(therion_platform PRIVATE therion_core)

# ── Layer 3: app-layer services (QObject, async, process) ──────────────────
add_library(therion_app STATIC
    src/app/TherionRunnerService.cpp
    src/app/ProjectStructureScanner.cpp
    # ...
)
target_link_libraries(therion_app PUBLIC therion_core therion_platform Qt6::Core Qt6::Concurrent)

# ── Layer 4: widget UI ──────────────────────────────────────────────────────
qt_add_executable(TherionStudio MACOSX_BUNDLE
    src/main.cpp
    src/app/MainWindow.cpp  ...
)
target_link_libraries(TherionStudio PRIVATE therion_app therion_core Qt6::Widgets Qt6::Svg)

# ── Tests ────────────────────────────────────────────────────────────────────
qt_add_executable(TherionDocumentEditorTest tests/TherionDocumentEditorTest.cpp)
target_link_libraries(TherionDocumentEditorTest PRIVATE therion_core Qt6::Test)

qt_add_executable(TextEditorCaretInteractionTest tests/TextEditorCaretInteractionTest.cpp)
target_link_libraries(TextEditorCaretInteractionTest PRIVATE therion_app Qt6::Widgets Qt6::Test)
```

### 4.3 Package Manager

This project has **one external dependency: Qt 6**. There are no third-party non-Qt libraries. Introducing vcpkg or Conan today adds toolchain manifest overhead, CI complexity, and Qt-specific friction (Qt is not well-supported in vcpkg's binary caching for all three platforms) for zero immediate gain.

**Recommendation:** Manage Qt installation explicitly per platform:
- macOS: `brew install qt@6` or Qt installer
- Windows: Qt Online Installer → MSVC 2022 kit
- Linux: `apt install qt6-base-dev qt6-svg-dev` (Ubuntu 22.04+) or Qt installer for older distros

If a second non-Qt library is added in the future (e.g., a compression library, a crypto library), adopt **vcpkg manifest mode** at that point:

```json
// vcpkg.json — add when a second dependency arrives
{
  "name": "therion-studio",
  "version": "0.2.0",
  "dependencies": [ "zlib" ]
}
```

### 4.4 C++ Standard Upgrade

Upgrade from C++17 to **C++20**. The payoff for this codebase:

- `std::span` for read-only views over `QVector<T>` in parser functions — avoids copies
- Designated initializers for the many aggregates like `ProjectStructureEntry`, `TherionParsedLine`
- `[[nodiscard]]` on all `bool` return values from `TherionDocumentEditor` static methods (currently none of the return values are marked nodiscard, meaning callers can silently ignore parse failures)
- `std::string_view` interop with `QStringView` where intermediate conversions happen

C++23's `std::expected` is tempting for result types but introduces a split idiom with Qt's signal-based error model. Defer it until the codebase standardizes on `std::expected` at all layers — do not introduce it at one layer only.

### 4.5 Compiler Warning Hygiene

Add a warnings profile to CMakeLists that activates in CI:

```cmake
option(THERION_ENABLE_STRICT_WARNINGS "Enable -Werror strict warnings" OFF)

if(THERION_ENABLE_STRICT_WARNINGS)
    if(MSVC)
        target_compile_options(therion_core PRIVATE /W4 /WX /permissive-)
        target_compile_options(therion_app  PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(therion_core PRIVATE -Wall -Wextra -Werror -Wpedantic)
        target_compile_options(therion_app  PRIVATE -Wall -Wextra -Werror -Wpedantic)
    endif()
endif()
```

Apply to `therion_core` and `therion_app` first — widget code generates more noise from Qt macros. Enable `THERION_ENABLE_STRICT_WARNINGS=ON` in CI only.

---

## 5. Testability Strategy

### 5.1 Current State

| Test | Type | Status | Problem |
|---|---|---|---|
| `TherionDocumentEditorTest` | unit | passing | re-lists 4 source files |
| `TherionXviParserTest` | unit | passing | re-lists 2 source files |
| `SessionStoreTest` | system | passing | writes real QSettings |
| `TextEditorCaretInteractionTest` | UI smoke | **DISABLED** | 140-file source list, hangs offscreen? |
| `TextEditorCompletionHighlightTest` | UI smoke | **DISABLED** | same |
| `MapEditorDragUndoRedoSmokeTest` | UI smoke | passing | offscreen |

### 5.2 Fixing the Disabled Tests

Before adding new tests, **fix the two disabled UI tests**. They are disabled because they either:
1. Hang on `QT_QPA_PLATFORM=offscreen` due to an event loop issue, or
2. Segfault due to resource-initialization order with the 140-file source list

Diagnosis path:
```bash
QT_QPA_PLATFORM=offscreen ./TextEditorCaretInteractionTest --log-level all
```

If it hangs: the most common cause is a modal dialog shown during test setup (e.g., `QMessageBox::critical` triggered by a missing resource). Add `QCoreApplication::processEvents()` calls or mock the dialog path.

If it segfaults: resource initialization order — add `Q_INIT_RESOURCE(...)` calls before any code that reads catalog JSON.

After `therion_core` extraction, these tests will link the library rather than re-listing 140 sources, which eliminates the most likely initialization-order issue.

### 5.3 Test Pyramid After Library Extraction

```
Layer 1 — therion_core tests (fast, no QApplication, CI <5s)
  TherionDocumentEditorTest      ← links therion_core
  TherionDocumentParserTest      ← new, pure logic
  TherionXviParserTest           ← links therion_core
  ProjectStructureIndexTest      ← links therion_core
  DocumentFileEncodingTest       ← links therion_core
  MapBackgroundPlacementTest     ← links therion_core
  TherionBackgroundMetadataTest  ← links therion_core
  MapGeometryFeatureParsingTest  ← links therion_core + therion_app

Layer 2 — therion_app tests (QCoreApplication, no display, CI <15s)
  TherionRunnerServiceTest       ← new, tests process lifecycle with a stub binary
  ProjectStructureScannerTest    ← new, async scanning with temp directories
  SessionStoreTest               ← re-scoped: uses QSettings with temp path

Layer 3 — UI smoke tests (QApplication + offscreen, CI <60s per platform)
  TextEditorCaretInteractionTest  ← links therion_app, tests caret / highlight behavior
  TextEditorCompletionHighlightTest
  MapEditorDetachedPaneTest
  MapEditorDragUndoRedoSmokeTest
  MapEditorInputPolicyTest
```

### 5.4 Making `SessionStoreTest` a True Unit Test

```cpp
// Before: reads/writes real user QSettings — a system test
void SessionStoreTest::testLastProjectPath() {
    SessionStore::setLastProjectPath("/some/path");
    QCOMPARE(SessionStore::lastProjectPath(), "/some/path");
    // Side effect: wrote to ~/Library/Preferences/... on macOS
}

// After: use a scoped temp-path QSettings
void SessionStoreTest::testLastProjectPath() {
    QTemporaryDir tmpDir;
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tmpDir.path());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    SessionStore store;  // instance-based; reads from isolated temp path
    store.setLastProjectPath("/some/path");
    QCOMPARE(store.lastProjectPath(), "/some/path");
    // No side effects outside tmpDir
}
```

### 5.5 CI/CD Matrix

```yaml
# .github/workflows/build-and-test.yml

name: Build and Test

on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-22.04, windows-2022, macos-14]
        build_type: [Debug, Release]

    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4

      - name: Install Qt (Linux)
        if: runner.os == 'Linux'
        run: sudo apt-get install -y qt6-base-dev qt6-svg-dev qt6-tools-dev libgl1-mesa-dev

      - name: Install Qt (macOS)
        if: runner.os == 'macOS'
        run: brew install qt@6

      - name: Install Qt (Windows)
        if: runner.os == 'Windows'
        uses: jurplel/install-qt-action@v4
        with:
          version: '6.7.*'
          arch: win64_msvc2022_64

      - name: Configure
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
                -DTHERION_ENABLE_STRICT_WARNINGS=ON

      - name: Build
        run: cmake --build build --parallel

      - name: Run unit tests (no display needed)
        run: ctest --test-dir build -L unit --output-on-failure

      - name: Run UI smoke tests (Linux needs virtual display)
        if: runner.os == 'Linux'
        run: |
          sudo apt-get install -y xvfb
          xvfb-run --auto-servernum ctest --test-dir build -L ui --output-on-failure
        env:
          QT_QPA_PLATFORM: xcb

      - name: Run UI smoke tests (Windows/macOS)
        if: runner.os != 'Linux'
        run: ctest --test-dir build -L ui --output-on-failure
        env:
          QT_QPA_PLATFORM: offscreen
```

**Linux UI test note:** Use `xvfb-run` with `QT_QPA_PLATFORM=xcb` for drag-and-drop tests, not `offscreen`. The `offscreen` platform does not simulate mouse events through the platform layer the same way a real display does. `QGraphicsScene` drag behavior in `MapEditorDragUndoRedoSmokeTest` needs `xcb` to exercise the actual input event path.

---

## 6. Refactoring Roadmap

### Phase A — Stop the Bleeding (1 week, zero behavioral change)

These are pure structural changes with no product risk:

1. **Extract `therion_core` STATIC library** from `CMakeLists.txt`. All `src/core/*.cpp` sources. Update all test targets to link it instead of re-listing sources. Measure: `CMakeLists.txt` shrinks from ~1 050 lines to ~700.

2. **Apply `QT_NO_CAST_FROM_ASCII` / `QT_NO_CAST_TO_ASCII` to all test targets** (currently missing). Fix any implicit conversions exposed.

3. **Fix the two `DISABLED TRUE` tests.** Remove `DISABLED TRUE`, diagnose, fix. Do not proceed to Phase B while tests are silently skipped.

4. **Wire GitHub Actions CI** with the matrix above. Start with `unit` label only; add `ui` label after fixing the disabled tests.

---

### Phase B — Platform and Service Hygiene (1–2 weeks)

5. **Extract `therion_platform`** as described in §2.2. Move `/opt/homebrew/` paths out of `TherionRunnerService` into `MacPlatformPathResolver`. `TherionRunnerService` calls `Platform::therionExecutableCandidates()` instead.

6. **Add custom `Info.plist`** for macOS bundle with `LSMinimumSystemVersion`, `NSHighResolutionCapable`, version strings wired to `PROJECT_VERSION`.

7. **Localization scaffold:** Add `qt_add_translations(TherionStudio TS_FILES translations/therion_studio_cs.ts)` to CMakeLists. Run `lupdate` once to capture all `tr()` strings into the `.ts` file. Add empty `QTranslator` installation in `main.cpp`. No actual translations needed yet — the scaffold ensures strings are extracted going forward.

8. **Upgrade to C++20** (`CMAKE_CXX_STANDARD 20`). Add `[[nodiscard]]` to all `bool`-returning methods in `TherionDocumentEditor` and `TherionDocumentParser`. Replace any `QVector<T>` copies in pure-transform functions with `std::span<const T>` where appropriate.

---

### Phase C — `TextEditorTab` Friend-Web Reduction (2–3 weeks)

9. **Define narrow context structs** for each controller group (raw editor, block editor). Pass at controller construction. Eliminate `friend` declarations one group at a time. Start with `RawEditorFindController` (fewest cross-dependencies) and work toward `BlockEditor*` (most complex).

10. **Extract `therion_app` STATIC library.** Move `TherionRunnerService`, `ProjectStructureScanner`, `TherionRunnerConfigResolver`, `TherionExecutableSelectionController`, and related presenters from `src/app/` into `therion_app`. Tests for these services no longer need `Qt6::Widgets`.

---

### Phase D — `MainWindow` Decomposition (3–5 weeks, ongoing)

11. **Extract `SidebarController`** owning `sidebarContainer_`, `sidebarPages_`, `sidebarCollapseButton_`, and all sidebar-layout state. Expose signals: `sidebarPaneRequested(SidebarPane)`, `sidebarWidthChanged(int)`.

12. **Extract `TherionRunnerPanelController`** owning all `therion*` member widgets in `MainWindow.h`. This is largely done in `MainWindowTherionRunner.cpp`; finish the extraction by moving member variable ownership.

13. **Extract `WorkspaceToolbarController`** owning all `workspace*` member widget pointers. The existing `refreshWorkspaceModeSwitcher()` logic moves here.

14. **`MainWindow` becomes a compositor** with ≤20 private member variables: the sub-controllers, the `QTabWidget`, the splitter, and the struct-scan generation counter.

---

### Phase E — Cross-Platform Packaging (2–3 weeks)

15. **macOS:** Add code signing and notarization to CI. Use `cmake --install` + `codesign --deep` + `xcrun notarytool`. Require `APPLE_DEVELOPER_ID` secret in CI. Add a post-install `xcrun stapler staple` step.

16. **Windows:** Add `signtool.exe` to the NSIS packaging job. Fix any Unicode path issues in CPack NSIS (test with a path containing Czech characters). Add `CREATE_NO_WINDOW` flag to `QProcess` for therion execution.

17. **Linux:** Add an `AppImage` packaging job using `linuxdeploy` + Qt plugin. Add `AppStream` metainfo.xml. Test under both X11 (`QT_QPA_PLATFORM=xcb`) and Wayland (`QT_QPA_PLATFORM=wayland`) in CI to surface drag-and-drop differences.

---

## Summary of Priorities

| Priority | Action | Effort | Risk |
|---|---|---|---|
| 1 | Extract `therion_core` static library | 4 h | Low — pure CMake, no source changes |
| 2 | Fix disabled tests (`DISABLED TRUE`) | 1–2 d | Low-Medium — diagnose root cause first |
| 3 | Wire GitHub Actions CI | 4 h | Low |
| 4 | Move Homebrew paths to platform resolver | 4 h | Low |
| 5 | C++20 upgrade + `[[nodiscard]]` sweep | 4 h | Low |
| 6 | macOS Info.plist + localization scaffold | 4 h | Low |
| 7 | Remove `TextEditorTab` friend declarations | 1–2 w | Medium — test each controller after change |
| 8 | Extract `therion_app` library | 1 w | Medium |
| 9 | Decompose `MainWindow` into sub-controllers | 3–5 w | Medium-High — most behavior-affecting |
| 10 | Cross-platform packaging (sign/notarize) | 2–3 w | High — platform-specific tooling |

---

*Review performed against: `CMakeLists.txt` (1 054 lines, C++17, single `qt_add_executable`, 2 disabled tests), `MainWindow.h` (294 lines, 80+ private members), `TextEditorTab.h` (29 friend declarations), `TherionRunnerService.cpp` (hardcoded Homebrew paths), `main.cpp` (correct dark-mode handling), `ProjectStructureScanner.cpp` (correct `QtConcurrent` pattern). No CI configuration exists in this repository.*
