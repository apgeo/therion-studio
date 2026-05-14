# Therion Studio User Manual

Last updated: 2026-05-14

This manual describes the currently implemented behavior.
Update this file whenever UI layout, workflows, keyboard shortcuts, or settings behavior changes.

## 1. Application Overview

Therion Studio is a Qt desktop editor for Therion projects with:

- project file browsing
- text editing for `.th`, `.th2`, and `thconfig`/`*.thconfig`
- structure and map-object navigation
- TH2 map workspace with source-linked geometry editing
- integrated Therion runner console

## 2. Main Window Layout

The main window is organized into:

- Menu bar (`File`, `Edit`, `Map`, `View`, `Window`, `Help`)
- Left sidebar with activity rail and switchable panes
- Central tab area (text tabs and map-editor tabs)
- Status bar (global app status)

### 2.1 Sidebar Activity Rail and Panes

The sidebar has an always-visible activity rail with 4 panes:

- `Files`
- `Structure`
- `Map`
- `Console`

Behavior:

- Clicking an activity icon opens that pane.
- Clicking the active icon again collapses the sidebar to the rail.
- Clicking any icon while collapsed expands sidebar and opens that pane.
- `Map` pane is enabled only when the active tab is a `.th2` document.

### 2.2 Central Tabs

Tabs can host:

- `TextEditorTab` for text documents
- `MapEditorTab` for `.th2` map workflow (text + map)

If no documents are open, a Welcome tab is shown.

## 3. Menus and Commands

### 3.1 File Menu

- `New Window`
- `Open Project...`
- `Close Project`
- `Save`
- `Save All`
- `Close`
- `Close All Tabs`
- `Exit`

### 3.2 Edit Menu

- `Find`
- `Find and Replace`

### 3.3 Map Menu

- `Open Current Document in Map Editor` (available for `.th2` documents)

### 3.4 View Menu

- `Show Sidebar`
- `Show Console` (switches sidebar to Console pane)

### 3.5 Help Menu

- `About Therion Studio`

## 4. Keyboard Shortcuts

Shortcuts below are those currently wired in code.
Platform key substitution follows Qt standard behavior (`Ctrl` on Windows/Linux, `Command` on macOS where applicable).

| Action | Shortcut |
|---|---|
| New window | Platform default `QKeySequence::New` |
| Open project | Platform default `QKeySequence::Open` |
| Save | Platform default `QKeySequence::Save` |
| Save all | Platform default `QKeySequence::SaveAs` |
| Close app | Platform default `QKeySequence::Quit` |
| Find | Platform default `QKeySequence::Find` |
| Find and replace | Platform default `QKeySequence::Replace` |
| Close all tabs | `Ctrl/Cmd+Shift+W` |
| Open current `.th2` in map editor | `Ctrl/Cmd+Alt+Shift+G` |
| Map: split selected line segment | `Insert` or `I` (no modifiers) |
| Map: delete selected middle line anchor | `Delete` or `Backspace` |

Notes:

- `Insert` fallback `I` exists for keyboards without `Insert` (important on Mac keyboards).
- Shortcut-driven workflows are intended to work on macOS, Windows, and Linux.

## 5. Core Workflows

### 5.1 Open a Project

1. Use `File -> Open Project...`.
2. Select a directory.
3. Files pane roots to the selected directory.
4. Structure sidebar is rebuilt from project scan.

### 5.2 Open Files

- Double-click file in Files pane.
- `.th2` files open in map editor tabs.
- Other files open in text editor tabs.

Project tree context menu actions:

- Files: `Open`, `Open in Map Editor` (for `.th2`), `Open Externally` (non-Therion files), `Duplicate`, `Rename`, `Delete`
- Folders: `Rename Folder`, `Delete Folder`
- Create: `New Folder`, `New .th File`, `New .th2 File`, `New thconfig`

Safety guardrails:

- Rename/delete actions are blocked if matching files/folders are currently open in tabs.

### 5.3 Edit Text Documents

Text editor includes:

- syntax highlighting
- find/replace bar with `Whole word` and `Case sensitive` options
- contextual help pane (collapsible via small triangle on splitter handle)
- bottom status row with path and encoding
- explicit `Convert to UTF-8` action shown when a file is opened with a non-UTF-8 encoding
- loading honors Therion `encoding ...` directive when the declared codec is available in Qt
- conversion is explicit: clicking `Convert to UTF-8` asks for confirmation and marks the tab dirty; disk content changes only after save
- status notes indicate whether saves are preserving original encoding or whether UTF-8 conversion is pending save
- directive aliases such as `encoding cp1250` are supported via codec-name normalization (`cpNNNN` -> `windows-NNNN` candidate), and `encoding latin2` is normalized to an `iso-8859-2` candidate path
- unsupported `encoding ...` directive tokens fall back to normal encoding detection/decode order instead of failing the file open

### 5.4 Use Structure + Inspector

Structure pane shows indexed Therion hierarchy.
Selecting an entry:

- updates Inspector
- can open source location
- syncs to editor line when relevant

Inspector supports:

- renaming selected structure entry via `Apply` (persists to source)
- line-state toggles for line entries: `Closed` (`-close`) and `Reversed` (`-reverse`)
- encoding-preserving source rewrites for inspector-applied changes (non-UTF files keep their original encoding unless explicitly converted in the text editor)

### 5.5 Use Map Workspace (`.th2`)

Map tab uses a split text+map workspace by default.

Toolbar actions:

- mode buttons: `Select`, `Point`, `Line`, `Freehand`, `Smart Trace`, `Area`
- `Insert Scrap`
- `Complete Draft`
- `Undo`, `Redo`
- `Zoom -`, `Zoom +`
- `Fit`
- `Fit + BG`
- `Open Map in Window` / `Return Map Pane`

Detached map-pane behavior:

- `Open Map in Window` detaches the graphical map pane into its own top-level window (useful for a second monitor).
- The text editor stays in the main tab.
- Closing the detached map window (or clicking `Return Map Pane`) reattaches the same map pane back to the tab.
- While detached, the embedded map pane area in the main tab is hidden.

Map help pane:

- shown below map canvas
- collapsible via small triangle on splitter handle

Text + map status row:

- shared path + encoding row below workspace

### 5.6 Map Navigation and Input

Implemented map navigation behavior:

- standard wheel mouse: wheel zooms around cursor, `Cmd/Ctrl + wheel` zooms, right-button drag pans, middle-button drag pans
- precise scrolling devices (touchpads / Magic Mouse style deltas): two-finger/surface scroll pans, `Cmd/Ctrl + scroll` zooms
- pinch: native pinch zoom (`ZoomNativeGesture`) zooms around gesture position
- additional touch handling: two-touch threshold pan in Select mode, viewport gestures suppressed during active primary-pointer interaction
- explicit `Touch Controls` toolbar toggle for pen-first workflows:
  - when enabled, non-modified wheel/scroll input pans by default (including non-precise wheel devices)
  - `Cmd/Ctrl + scroll` still zooms
  - two-touch pan candidate can activate outside Select mode
  - setting is persisted in app session settings

Zoom constraints:

- zoom is clamped to `0.1 .. 50.0`

### 5.7 Background Image Management (Map Sidebar)

In sidebar `Map` pane, the `Background Images` panel provides:

- layer list with selection
- add (`+`) button (multi-file picker)
- `Remove`, `Hide/Show`, `Up`, `Down`
- position `X`, `Y` fields
- nudge arrows (`←`, `→`, `↑`, `↓`) with step 10 units
- opacity slider with reset
- gamma slider with reset

Persistence:

- layer stack, visibility, position, opacity, gamma are persisted per document in session settings.
- map editor also attempts to load layers from XTherion metadata in source text.

### 5.8 Run Therion (Console Pane)

Console pane fields:

- `Executable`
- `Working Directory`
- `Arguments`
- `Config` (resolved)
- `Config Path` (resolved)
- `Run Policy`

Buttons:

- `Run Therion`
- `Stop`
- `Use Project Root`
- `Copy Output`
- `Browse...` for executable path

Behavior:

- parallel runs are rejected while process is active
- if config is auto-detected and no explicit `-c/--config` is provided, working directory may auto-switch to config file directory
- output streams to console pane

## 6. Settings and Session Persistence

Therion Studio uses Qt `QSettings` with:

- organization: `Therion Studio`
- domain: `therionstudio.example`
- application: `Therion Studio`

Persisted values include:

- last project path
- main window geometry/state
- open document paths and active document
- structure-name overrides
- Therion executable/working directory/arguments
- map background layer session JSON
- detached map document paths (stored with open-document session state)

There is currently no dedicated Settings dialog; settings are updated through normal UI interactions.

## 7. QA and Verification References

- Encoding workflow manual checklist: [`docs/ENCODING_QA_CHECKLIST.md`](ENCODING_QA_CHECKLIST.md)

## 7. Platform Notes

### 7.1 macOS

- `Insert` key may be missing on many Mac keyboards; map vertex insertion supports `I` as fallback.
- Homebrew Therion auto-detection is supported for common paths (`/opt/homebrew/bin/therion`, `/usr/local/bin/therion`, and `$HOMEBREW_PREFIX/bin/therion`).

### 7.2 Windows

- Cross-platform shortcuts are designed to work via Qt standard key-sequence mapping.
- Map insertion supports both `Insert` and `I`.

### 7.3 Linux

- Cross-platform shortcuts are designed to work via Qt standard key-sequence mapping.
- Map insertion supports both `Insert` and `I`.

## 8. Known Current Limitations

- Non-UTF-8 support relies on Qt-supported codecs and may not perfectly decode every legacy encoding variant on every platform.
- Freehand/Smart Trace modes currently insert draft line items (no full tracing workflow yet).
- GUI automation coverage is still incomplete; many checks are currently unit/regression + manual workflows.

## 9. Troubleshooting

### 9.1 Therion executable not found

Symptom:

- `Therion executable "..." was not found or is not executable`

Fix:

- Set `Executable` to a full valid path.
- Verify the file is executable.
- On macOS Homebrew installs, try `/opt/homebrew/bin/therion`.

### 9.2 No config file resolved

Symptom:

- `Config Path` shows unresolved message.

Fix:

- Ensure working directory contains `thconfig`, or
- pass explicit `-c` / `--config` argument, or
- open a `.thconfig` file and rerun.

### 9.3 Map pane unavailable

Symptom:

- Map sidebar pane disabled.

Fix:

- Activate a `.th2` document tab.

### 9.4 Rename/delete blocked in project browser

Symptom:

- operation blocked by open tabs.

Fix:

- Close tabs that reference the target file/folder, then retry.
