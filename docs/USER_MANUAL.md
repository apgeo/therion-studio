# Therion Studio User Manual

Last updated: 2026-05-31

This guide covers everyday workflows in Therion Studio. It intentionally focuses on using the application, not on the full Therion language reference. Therion source syntax, command names, options, and serialized document content stay in canonical Therion form.

The application follows the operating system language when a bundled translation is available. English, Czech, and Slovak are included. Use `File -> Settings...` (`Preferences...` in the native macOS application menu) to override the application language. Language changes take effect after restarting Therion Studio.

## Contents

- [1. What Therion Studio Is](#1-what-therion-studio-is)
- [2. Main Window](#2-main-window)
- [3. Getting Started](#3-getting-started)
- [4. Text Editing](#4-text-editing)
- [5. Structure and File Operations](#5-structure-and-file-operations)
- [6. Visual Map Editing (`.th2`)](#6-visual-map-editing-th2)
- [7. Running Therion](#7-running-therion)
- [8. Settings](#8-settings)
- [9. Keyboard Shortcuts](#9-keyboard-shortcuts)
- [10. Help And About](#10-help-and-about)
- [11. Troubleshooting](#11-troubleshooting)

## 1. What Therion Studio Is

Therion Studio is a desktop editor for Therion cave-mapping projects. It provides:

- project file browsing
- text editing for `.th`, `.th2`, and Therion config files
- structure navigation for `survey`, `centerline`, `map`, and `scrap` objects
- visual map editing for `.th2` files
- an integrated Therion run console

Therion Studio does not include the external Therion compiler. Install Therion separately and configure the executable path in Settings if auto-detection is not enough.

## 2. Main Window

The main window contains:

- the menu bar (`File`, `Edit`, `View`, `Help`)
- a top command bar with project, save, editor, and map actions
- the left activity rail (`Files`, `Structure`, `Compiler`) and a quick compile action
- document tabs in the center
- a status bar with compile, encoding, and map state

Common window actions:

- `File -> New Window` opens a new empty window. It does not copy the current project or open documents.
- `File -> Settings...` opens application settings.
- `View -> Expand Sidebar` / `Collapse Sidebar` shows or hides the left sidebar content.
- `View -> Expand Context Help`, `Expand Block Inspector`, or `Expand Map Inspector` controls the active right-side panel, depending on the current editor.
- `View -> Show Map Magnifier` / `Hide Map Magnifier` toggles the map magnifier overlay. This is only UI state and does not modify `.th2` files.
- `View -> Enter Full Screen` / `Exit Full Screen` toggles full-screen mode.

When a map pane is detached into a separate window, the main window can show both `Map Inspector` and `Context Help` controls because the visual map and raw source can be visible at the same time.

## 3. Getting Started

### 3.1 Open a Project

1. Select `File -> Open Project...`.
2. Choose the project folder.
3. Open documents from the `Files` pane.

When no project is open, the welcome tab shows an `Open Project...` button. When a project is open but no document tab is active, the welcome tab offers opening a file from the sidebar.

### 3.2 Open Documents

- Double-click a file in `Files`.
- `.th2` files open in the map editor.
- `.th`, `thconfig`, `*.thconfig`, `thconfig.*`, `.log`, `.txt`, and ordinary text files open in the text editor.
- Unsupported files, such as images or PDF, show an `Unsupported file` message with `Open in External App`.

### 3.3 Create and Manage Files

Right-click in the `Files` pane to create folders, create `.th`, `.th2`, and `thconfig` files, rename items, duplicate files, delete items, or open `.th2` files directly in the map editor.

Rename and delete are blocked when the target file or folder is open in a document tab. Close the related tabs first, then retry the operation.

### 3.4 Save Changes

- `File -> Save` saves the active tab.
- `File -> Save All` saves all modified tabs.
- Closing a dirty tab asks whether to save, discard, or cancel.

## 4. Text Editing

### 4.1 Raw and Blocks Modes

For `.th` and Therion config files:

| Mode | Use |
|---|---|
| `Raw` | Direct source editing with syntax highlighting, search, replace, autocomplete, and contextual help. |
| `Blocks` | Structured editing of supported commands and blocks. |

New `.th` and Therion config tabs open in the default editor selected in Settings. `Raw` is the default. Switching to `Blocks` does not insert missing `encoding` directives and does not rewrite the source until you make an explicit edit.

The `Blocks` toolbox is filtered by document type:

| Document | Toolbox content |
|---|---|
| `.th` | Commands from the Therion Book chapter `Creating data files`. `.th2` map-object commands such as `scrap`, `point`, `line`, and `area` are hidden because they are edited in the map editor. |
| `thconfig`, `thconfig.*`, `*.thconfig` | Commands from `Processing data`, such as `source`, `select`, and `export`. |

For `.th2` files:

| Mode | Use |
|---|---|
| `Raw` | Direct text editing of the `.th2` source. |
| `Visual` | Visual map editing with canvas and inspector. |

### 4.2 Text Editing Features

- line numbers and active-line highlight
- command, option, value, and path autocomplete while typing
- `Ctrl+Space` to open autocomplete manually
- contextual help for the current command or option
- find and replace from the `Edit` menu
- conversion of unsupported encodings to UTF-8 after confirmation

### 4.3 Blocks Data Rows

In `Blocks` mode, `data ...` blocks can be edited through a table based on the active data header. Empty body lines are ignored when the table opens, so spacing in the source does not become fake measurement data.

## 5. Structure and File Operations

The `Structure` pane is a lightweight navigation index for the opened project. It shows `survey`, `centerline`, `map`, and `scrap` hierarchy and recognizes both Therion spellings: `centreline` and `centerline`.

The index uses the selected `Target Config` when it points inside the opened project. Without an explicit target config, Therion Studio tries the root `thconfig`; if that does not exist and exactly one named root config exists (`*.thconfig` or `thconfig.*`), that file is used. If several config files are possible, choose the intended `Target Config` in the `Compiler` pane.

Maps and scraps referenced inside `map ... endmap` are shown under that map when the reference is unambiguous. Unresolved or ambiguous references appear as warning rows that navigate to the source line. The Therion compiler remains the authoritative validator for export behavior.

## 6. Visual Map Editing (`.th2`)

### 6.1 Modes and Inspector

`Visual` mode contains:

- the map canvas
- inspector tabs: `Selection`, `Objects`, `Backgrounds`

`Raw` mode remains available for direct source editing.

### 6.2 Main Map Tools

| Tool group | Actions |
|---|---|
| Zoom | `Zoom In`, `Zoom Out`, `Fit`, `Fit With Background` |
| Selection and drafting | `Select`, `Complete Draft` |
| Insertion | `Insert Scrap`, `Point`, `Line`, `Freehand`, `Area` |

The map canvas uses a stable light paper-style surface in both light and dark application modes. Toolbars, tabs, and inspectors follow the system appearance, but raster backgrounds, `.xvi` references, and map symbols are not inverted or tinted for dark mode.

### 6.3 Insert Objects

- `Point`: click once in the map.
- `Line`: click vertices, then press `Enter` or `Complete Draft`.
- `Area`: click vertices, then press `Enter` or `Complete Draft`.
- `Freehand`: press, drag, and release to insert a simplified Bezier line.
- `Insert Scrap`: set the pending scrap ID/projection in `Selection`, then click `Insert Scrap` again.

Starting `Point`, `Line`, `Freehand`, or `Area` activates `Inspector -> Selection` before the first point or vertex is placed. Set type, subtype, ID, point name, or label text there before committing the new object.

While drafting a line or area:

- click to add a straight vertex
- press-drag-release while placing a vertex to create a curved Bezier segment
- drag visible Bezier control handles before committing to refine the draft curve
- click the first line vertex again to finish a closed line (`-close on`)
- press `Backspace` or `Delete` to remove the last draft vertex
- press `Esc` to cancel insertion

### 6.4 Edit Geometry

Select a map object or one of its vertices/control handles in the canvas. The `Selection` inspector then shows the relevant controls.

For lines and area borders:

- select a vertex to edit its line-point details
- use `Insert Before` / `Insert After` to add vertices near the selected vertex
- use `Extend Before` / `Extend After` at line endpoints to continue an existing line
- use `Delete` / `Backspace` to remove the selected line vertex; if no line vertex is selected, `Delete` / `Backspace` deletes the selected object
- use `<<` and `>>` to enable or remove incoming/outgoing Bezier handles
- drag Bezier handles directly on the canvas to reshape the curve

If a line is used as an area border, some destructive line actions are blocked; select or edit the owning area instead.

### 6.5 Edit Object Properties

In `Inspector -> Selection`, you can edit properties for selected `Scrap`, `Point`, `Line`, or `Area` objects.

- `Scrap` shows ID/projection and a separate `Scrap Scale` section for XTherion/Therion-compatible `-scale [...]` calibration values.
- `Point`, `Line`, and `Area` expose common fields such as ID, type, subtype, and supported options.
- `point label` and `line label` expose `Text (-text)`. Point labels render near the point; line labels render along the label line path, so the line controls the text length and orientation.
- Point types that support `-orientation` show an orientation override and a draggable orientation handle. Station names stay screen-aligned for readability.

The style preview under `Subtype` shows how the selected or pending object will look. The preview uses a light map-like background even in dark mode.

### 6.6 Objects and Backgrounds

In `Inspector -> Objects`, you can select objects, reorder objects by drag/drop, toggle visibility in the current view, and delete objects with confirmation.

In `Inspector -> Backgrounds`, you can:

- add, remove, and reorder raster or `.xvi` background layers
- show/hide individual layers
- edit layer position and opacity
- adjust `Gamma` for raster layers (`.xvi` uses fixed Gamma)

Therion Studio does not generate a separate metric grid. Use background layers, especially `.xvi`, for reference grid content.

### 6.7 Detached Map Window

Use `Separate Map` to move the visual map pane into its own window, for example on a second monitor. Use `Return Map` or close the detached map window to reattach it.

## 7. Running Therion

Open the `Compiler` pane from the activity rail.

### 7.1 Main Fields

| Field | Meaning |
|---|---|
| `Arguments` | Additional arguments for the current session. |
| `Run Target` | `Current Config` or `Project Config`. |
| `Target Config` | Config file for project-level runs. |
| `Working Directory Override` | Optional override for the run directory. |

Set the Therion executable path in `File -> Settings...`. If no explicit path is set, Therion Studio tries `therion` and platform auto-detection.

Closing a project clears `Target Config` and `Working Directory Override` because those paths are project-specific. The Therion executable path is a global preference. Additional arguments are session-only.

### 7.2 Actions

- `Run Therion`
- `Stop`
- `Clear Output`
- `Copy Output`

The status bar shows compile state: `Idle`, `Running`, `OK`, or `Failed`.

## 8. Settings

Open settings from `File -> Settings...` or `Preferences...` on macOS.

Settings include:

- application language (`System Default`, English, Czech, Slovak)
- Therion executable path
- default editor for newly opened `.th` and Therion config tabs (`Raw` or `Blocks`)

## 9. Keyboard Shortcuts

Use `Command` on macOS and `Ctrl` on Windows/Linux unless the platform menu shows a different native shortcut.

| Action | Shortcut |
|---|---|
| New window | `Command/Ctrl+N` |
| Open project | `Command/Ctrl+O` |
| Save | `Command/Ctrl+S` |
| Save all | shown in the `File` menu |
| Close all tabs | `Command/Ctrl+Shift+W` |
| Quit | `Command/Ctrl+Q` |
| Undo | `Command/Ctrl+Z` |
| Redo | `Command/Ctrl+Shift+Z` or platform default |
| Find | `Command/Ctrl+F` |
| Find and replace | platform default replace shortcut |
| Manual completion popup (text editor) | `Ctrl+Space` |

## 10. Help And About

- `Help -> User Manual` opens the localized manual in the application. The manual viewer supports the table-of-contents links in this document.
- `Help -> About Therion Studio` shows version, build label, Qt version, platform, GitHub repository, license, maintainer, and third-party notices. On macOS, About is also available from the native application menu.

## 11. Troubleshooting

### 11.1 Therion executable not found

Fix:

- set a valid full path in `File -> Settings... -> Therion executable`
- verify executable permissions

### 11.2 Config cannot be resolved

Fix:

- set `Target Config`, or
- open the desired Therion config and run using `Current Config`, or
- verify the working directory / override path

### 11.3 Rename or delete is blocked

Fix:

- close tabs that reference the target file or folder, then retry

### 11.4 Map background looks wrong

Fix:

- select the layer in `Backgrounds`
- check layer visibility, position, opacity, and Gamma
- for `.xvi`, verify the `.xvi` file and referenced `.th2` are from the same coordinate context
