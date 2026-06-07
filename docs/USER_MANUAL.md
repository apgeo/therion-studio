# Therion Studio User Manual

Last updated: 2026-06-07

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
- `File -> New -> Therion Source (.th)`, `Therion Map (.th2)`, or `Therion Config (.thconfig)` opens a new unsaved document. New `.th`, `.th2`, and `.thconfig` documents start with `encoding utf-8`. The toolbar `New Document` button opens the same choices. The first `Save` asks where to save it.
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

The project folder chooser starts in your home folder when opened from `Open Project...`.

When no project is open, the welcome tab shows an `Open Project...` button, and the `Files` sidebar shows an empty state with the same project-opening action instead of browsing the computer filesystem. When a project is open but no document tab is active, the welcome tab offers opening a file from the sidebar.

The welcome tab and `File -> Recent Projects` list up to five recently opened projects. Select a project from either list to reopen it.

When a project is open, the welcome tab shows the active project name and path. It also lists up to ten recent files from that project; select a recent file to reopen it. The same project-scoped list is available from `File -> Recent Files`.

### 3.2 Open Documents

- Double-click a file in `Files`.
- `.th2` files open in the map editor.
- `.th`, `thconfig`, `*.thconfig`, `thconfig.*`, `.log`, `.txt`, and ordinary text files open in the text editor.
- Unsupported files, such as images or PDF, show an `Unsupported file` message with `Open in External App`.

### 3.3 Create and Manage Files

Use `File -> New` to create an unsaved `.th`, `.th2`, or `.thconfig` document and choose its path on first save. Right-click in the `Files` pane to create folders, create saved `.th`, `.th2`, and `.thconfig` files directly in the project, rename items, duplicate files, delete items, or open `.th2` files directly in the map editor.

Rename and delete are blocked when the target file or folder is open in a document tab. Close the related tabs first, then retry the operation.

### 3.4 Save Changes

- `File -> Save` saves the active tab.
- If the active tab has not been saved yet, `File -> Save` opens `Save As`.
- `File -> Save All` saves all modified tabs.
- Closing a dirty tab asks whether to save, discard, or cancel.
- If an open file changes on disk while its Therion Studio tab has no unsaved edits, Therion Studio reloads it automatically. If the tab has unsaved edits, Therion Studio asks whether to reload from disk or keep the in-memory version.

## 4. Text Editing

### 4.1 Raw and Blocks Modes

For `.th` and Therion config files:

| Mode | Use |
|---|---|
| `Raw` | Direct source editing with syntax highlighting, search, replace, autocomplete, and contextual help. |
| `Blocks` | Structured editing of supported commands and blocks, with the same command help metadata as Raw mode. |

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
- contextual help for the current command or option; Raw and Blocks show the same complete command help, and the help panel is titled with the current command, validation context, or selected help target
- a `Selection` inspector tab in Blocks mode for editing the selected block header and supported inline options; the first panel is titled with the selected Therion command and shows its source line
- when no block is selected, the Blocks `Selection` tab shows `No block selected.`; when the fixed root `encoding` card is selected, it shows the command and encoding value as read-only text
- find and replace from the `Edit` menu
- `File -> Import -> Import PocketTopo Text...` is shown only when an existing or unsaved `.th` text document is active, and imports a PocketTopo Therion export (`.txt`) at the cursor as Therion `centreline` blocks
- a `File` inspector tab with a panel titled by the current file name, full path, copy-path action, on-disk size, last-modified timestamp, current encoding, and UTF-8 conversion for non-UTF-8 files

### 4.3 Project Search

Open the Search activity from the left rail or press `Command/Ctrl+Shift+F`. Enter literal text, choose `Whole word` or `Case sensitive` when needed, and press `Enter` or `Search` to scan the current project.

Project search scans Therion text sources (`.th`, `.th2`, and Therion config files), includes unsaved edits from open tabs, and lists matches grouped by file with line and column locations. Double-click a file or match row to open the file in Raw mode at the matching text with the inline find bar ready for next/previous navigation.

### 4.4 Blocks Data Rows

In `Blocks` mode, `data ...` blocks can be edited through a table based on the active data header. Empty body lines are ignored when the table opens, so spacing in the source does not become fake measurement data.

## 5. Structure and File Operations

The `Structure` pane is a lightweight navigation index for the opened project. Its sidebar description identifies it as the survey, map, and scrap structure for the current project. It shows `survey`, `map`, and `scrap` hierarchy and recognizes both Therion centerline spellings: `centreline` and `centerline`.

Select a row to inspect it in the tree. Double-click a source row, or select it and press `Enter`, to open its source document and navigate to the matching line.

Within each parent, rows are grouped as surveys, maps, then scraps, and each group is sorted alphabetically by the displayed name. Warning rows appear after the project objects they relate to.

The index uses the selected `Target Config` when it points inside the opened project. Without an explicit target config, Therion Studio tries the root `thconfig`; if that does not exist and exactly one named root config exists (`*.thconfig` or `thconfig.*`), that file is used. If several config files are possible, choose the intended `Target Config` in the `Compiler` pane.

Maps and scraps referenced inside `map ... endmap` are shown under that map when the reference is unambiguous. Unresolved or ambiguous references appear as warning rows that navigate to the source line. The Therion compiler remains the authoritative validator for export behavior.

## 6. Visual Map Editing (`.th2`)

### 6.1 Modes and Inspector

`Visual` mode contains:

- the map canvas
- inspector tabs: `Selection`, `Objects`, `Backgrounds`, `File`

`Raw` mode remains available for direct source editing.

Non-UTF-8 files are opened with a concrete source encoding when it can be resolved, including common Central European legacy encodings such as ISO-8859-2. When such a file is saved, Therion Studio keeps the resolved source encoding unless you explicitly convert the file to UTF-8 from the `File` inspector.

### 6.2 Main Map Tools

| Tool group | Actions |
|---|---|
| Zoom | `Zoom In`, `Zoom Out`, `Fit`, `Fit With Background` |
| Selection and drafting | `Select`, `Complete Draft` |
| Insertion | `Insert Scrap`, `Point`, `Line`, `Freehand`, `Area` |

The map canvas uses a stable light paper-style surface in both light and dark application modes. Toolbars, tabs, and inspectors follow the system appearance, but raster backgrounds, `.xvi` references, and map symbols are not inverted or tinted for dark mode. Drag with the right mouse button to pan the map canvas in XTherion style.

### 6.3 Insert Objects

- `Point`: click once in the map.
- `Line`: click vertices, then press `Enter` or `Complete Draft`.
- `Area`: click vertices, then press `Enter` or `Complete Draft`.
- `Freehand`: press, drag, and release to insert a simplified Bezier line.
- `Insert Scrap`: creates a new scrap immediately, selects it in `Selection` and `Objects`, then lets you edit its ID/projection before adding points, lines, freehand lines, or areas.

Starting `Point`, `Line`, `Freehand`, or `Area` activates `Inspector -> Selection` before the first point or vertex is placed. Set type, subtype, ID, point name, label text, or supported point value there before committing the new object. If a scrap or an object inside a scrap was selected when you started the tool, the new object is inserted into that scrap; the pending metadata line shows the target scrap ID. Use `Insert into` to choose a different existing target scrap before committing.

While drafting a line or area:

- click to add a straight vertex
- press-drag-release while placing a vertex to pull that vertex's Bezier handle pair, matching XTherion-style curve entry
- drag visible Bezier control handles before committing to refine the draft curve
- click the first line vertex again to finish a closed line (`-close on`); click the first area vertex again to commit the area draft
- closed lines render the final segment back to the first vertex, including two-point closed Bezier curves
- press `Backspace` or `Delete` to remove the last draft vertex
- press `Esc` to cancel insertion

### 6.4 Edit Geometry

Select a map object or one of its vertices/control handles in the canvas. The `Selection` inspector then shows the relevant controls, including the source line and enclosing scrap ID.

Map objects keep their normal rendered colors while editing. In select mode, the map canvas uses a crosshair cursor; the object under the cursor hotspot is highlighted in cyan before selection, and the selected object is highlighted in red.

Right-click a map object or line vertex without dragging to open a context menu with common XTherion-style actions. Empty canvas right-clicks do not open this menu. The menu mirrors the available `Selection` inspector groups, such as type/subtype choices, editable object fields, `Geometry`, the complete available `Line Point` panel, `Line Point Actions`, and `Object Actions`; free text or numeric editors are opened and focused in the inspector. On macOS, the trackpad secondary click, such as a two-finger click, opens the same menu. If the menu is already open, another secondary click on a different object or vertex retargets the menu to that new selection and moves it to the latest click position.

For lines and area borders:

- select a vertex to edit its line-point details
- right-click a line segment and use `Insert Point Here` to split the nearest segment at the click position
- use `Insert Before` / `Insert After` to add vertices near the selected vertex
- use `Extend Before` / `Extend After` at line endpoints to continue an existing line
- use `Delete` / `Backspace` to remove the selected line vertex; if no line vertex is selected, `Delete` / `Backspace` deletes the selected object
- when deleting a line vertex that has additional line-point options (for example `altitude .` or `subtype ...`), Therion Studio asks for confirmation before deletion
- use `<<` and `>>` to enable or remove incoming/outgoing Bezier handles
- drag Bezier handles directly on the canvas to reshape the curve

If a line is used as an area border, some destructive line actions are blocked; select or edit the owning area instead.

### 6.5 Edit Object Properties

In `Inspector -> Selection`, you can edit properties for selected `Scrap`, `Point`, `Line`, or `Area` objects.

- `Scrap` shows ID/projection and a separate `Scrap Scale` section for XTherion/Therion-compatible `-scale [...]` calibration values.
- `Point`, `Line`, and `Area` expose common fields such as ID, type, subtype, and supported options. Choose the empty subtype value, or `No subtype` in the context menu, to remove an existing `-subtype`.
- `Edit Object Settings...` opens the full catalog-backed option editor for the selected `scrap`, `point`, `line`, or `area` command. Positional attributes such as point `x`/`y`, line `type`, and scrap `id` are shown as protected attribute rows, while `-id`, `-text`, `-orientation`, and other options remain editable option rows.
- `point label` and `line label` expose `Text (-text)`. Point labels render near the point; line labels render along the label line path, so the line controls the text length and orientation.
- supported point types such as `height`, `passage-height`, `altitude`, `dimensions`, and `date` expose `Value (-value)`. Bracketed Therion values such as `[fix 1300]` are preserved.
- Point types that support `-orientation` show an orientation override and a draggable orientation handle. Station names stay screen-aligned for readability.
- selected line vertices expose dedicated `Line Point` controls for supported per-vertex options. `Subtype` is shown for line types with segment subtypes, and `Altitude (auto)` is shown for wall line points and writes `altitude .`.
- selected line vertices also expose an `Additional line-point options` editor in `Selection` for remaining per-vertex standalone options such as `altitude`, `subtype`, `direction`, or `adjust`, so these can be edited without switching to Raw mode. Rows managed by visible dedicated controls are hidden from this editor.
- `Additional line-point options` edits are applied automatically when the field loses focus (no separate Apply/Clear buttons).
- line vertices with additional line-point options display a small marker on the vertex handle and include option preview text in the vertex tooltip.

The `Preview` row shows how the selected or pending object will look. The preview uses a light map-like background even in dark mode.

### 6.6 Objects and Backgrounds

In `Inspector -> Objects`, you can select objects, reorder objects by drag/drop, toggle visibility in the current view, and delete objects with confirmation.

In `Inspector -> Backgrounds`, you can:

- add, remove, and reorder raster, `.xvi`, or PocketTopo `.txt` background layers
- show/hide individual layers
- edit layer position and opacity
- adjust `Gamma` for raster layers (`.xvi` uses fixed Gamma)

When you add a PocketTopo Therion export (`.txt`) as a map background, Therion Studio asks for XVI scale, resolution, grid spacing, and plan or extended-elevation projection. It writes a generated `_p.xvi` or `_e.xvi` file next to the PocketTopo export, adds that `.xvi` as the background layer, and stores XTherion-compatible image metadata in the `.th2` source.

Therion Studio does not generate a separate metric grid. Use background layers, especially `.xvi`, for reference grid content.

### 6.7 Detached Map Window

Use `Separate Map` to move the visual map pane into its own window, for example on a second monitor. Use `Return Map` or close the detached map window to reattach it.

## 7. Running Therion

Open the `Compiler` pane from the activity rail. The pane description identifies it as the place to run Therion for the current project or active config.

### 7.1 Main Fields

| Field | Meaning |
|---|---|
| `Arguments` | Additional arguments for the current session. |
| `Run Target` | `Current Config` or `Project Config`. |
| `Target Config` | Config file for project-level runs. |
| `Working Directory Override` | Optional override for the run directory. |

Set the Therion executable path in `File -> Settings...`. If no explicit path is set, Therion Studio tries `therion` and platform auto-detection.

Before starting a compile, Therion Studio saves all modified open document tabs. If any open document cannot be saved, the compile is canceled and the runner is not started.

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
| Search in project | `Command/Ctrl+Shift+F` |
| Find and replace | platform default replace shortcut |
| Switch to Raw editor | `Command/Ctrl+top-row 1` |
| Switch to Blocks editor for `.th` / config, or Visual editor for `.th2` | `Command/Ctrl+top-row 2` |
| Manual completion popup (text editor) | `Ctrl+Space` |
| Complete the current map draft | `Enter` |
| Cancel map insertion/drawing | `Esc` |
| Delete the selected map object or selected line point; while drawing, delete the last draft point | `Delete` / `Backspace` |

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
