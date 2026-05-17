# Therion Studio User Manual

Last updated: 2026-05-17

This manual describes the currently implemented behavior.
Update this file whenever UI layout, workflows, keyboard shortcuts, or settings behavior changes.

## 1. Application Overview

Therion Studio is a Qt desktop editor for Therion projects with:

- project file browsing
- text editing for `.th`, `.th2`, and `.thconfig`
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
- Dragging the sidebar content resize handle below the minimum usable width snaps the content pane closed while keeping the activity rail fixed and visible.
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

- `Quick User Manual` (short in-app workflow guide)
- `User Manual (Full)` (loads `docs/USER_MANUAL.md` when available; falls back to quick manual if not found)
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
| Text editor: completion popup (manual trigger) | `Ctrl+Space` |
| Close all tabs | `Ctrl/Cmd+Shift+W` |
| Open current `.th2` in map editor | `Ctrl/Cmd+Alt+Shift+G` |
| Map: split selected line segment | `Insert` or `I` (no modifiers) |
| Map: delete selected middle line anchor | `Delete` or `Backspace` |
| Map: toggle selected line vertex smooth/corner | `S` (no modifiers) |

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
- Newly created/renamed/deleted items refresh immediately in the Files tree without restarting the app.
- Newly created Therion source/config files (`.th`, `.th2`, `thconfig`) are initialized with `encoding utf-8` on the first line.

Safety guardrails:

- Rename/delete actions are blocked if matching files/folders are currently open in tabs.

### 5.3 Edit Text Documents

Text editor includes:

- syntax highlighting
- command keyword highlighting is augmented from the generated Therion command catalog (`resources/therion_command_catalog.json`)
- left gutter with 1-based line numbers that scroll with the document
- active-line highlight that follows the text cursor (including map-driven source navigation)
- editor mode toggle: `Raw` and `Blocks` (available for `.th` and `.thconfig` files)
- find/replace bar with `Whole word` and `Case sensitive` options
- completion popup is shown while typing (commands/options/values), sourced from command catalog metadata; `Ctrl+Space` remains available as manual trigger
- when completion popup is visible, confirm a suggestion with `Enter`, `Tab`, or mouse click; `Esc` closes the popup
- when completion popup is not visible, pressing `Tab` inserts four spaces (no literal tab character)
- when confirming a block-opening command completion on an otherwise empty command line (for example `survey`, `map`, `scrap`, `centerline`, `line`, `area`), editor auto-inserts the matching `end...` line
- completion suggestions are context-aware: command position favors commands, `-` option position favors command options, and value position after an option favors allowed values
- when a command has required positional arguments and catalog-defined value tokens, completion suggests those value tokens while positional input is still incomplete (for example `centerline` `data` styles/readings such as `normal`, `from`, `to`)
- value-position completion supports both single-value and multi-value option slots from catalog metadata (`value_arity`), so options like `-context` continue suggesting valid values after the first token
- command-position completion inside `centerline` also includes inline centerline subcommands parsed from centerline syntax metadata (for example `data`, `date`, `team`, `units`, `station`, `extend`)
- for commands with required positional arguments (for example `survey <id>`), autocomplete suppresses option suggestions until required arguments are entered; manual completion shows a short hint when required arguments are still missing
- command-position completion also respects nested block scope from lines above the cursor (for example top-level vs inside `survey`/`scrap`/`centerline`) when catalog context metadata is available
- when completion popup opens, a short tooltip shows the detected scope (`top-level`, `survey`, `scrap`, `centerline`, etc.) for quick verification
- syntax highlighting validates command options against catalog metadata on each command line: known options use option styling and unknown options are marked as invalid (red wavy underline)
- syntax highlighting also validates option values where catalog enum/type metadata is available (for example `-close`, `-reverse`, and type-scoped `-subtype` on `line`/`point`/`area`), and marks incompatible values as invalid
- when caret is on an invalid option or invalid enum/subtype value, contextual help switches to a `Validation` panel with reason and allowed values
- when caret lands on an invalid token, an inline tooltip is also shown near the cursor with a short validation reason and allowed values
- Raw mode uses a right-column contextual help pane, resizable via splitter handle
- contextual help resolution is driven by `therion_command_catalog.json` only
- bottom status row for encoding notes and conversion action
- explicit `Convert to UTF-8` action shown when a file is opened with a non-UTF-8 encoding
- loading honors Therion `encoding ...` directive when the declared codec is available in Qt
- conversion is explicit: clicking `Convert to UTF-8` asks for confirmation and marks the tab dirty; disk content changes only after save
- status notes indicate whether saves are preserving original encoding or whether UTF-8 conversion is pending save
- directive aliases such as `encoding cp1250` are supported via codec-name normalization (`cpNNNN` -> `windows-NNNN` candidate), and `encoding latin2` is normalized to an `iso-8859-2` candidate path
- unsupported `encoding ...` directive tokens fall back to normal encoding detection/decode order instead of failing the file open

`Blocks` mode:

- left toolbox provides draggable Therion command entries grouped by context
- `encoding` is not offered in toolbox (it is managed as a fixed document-root directive)
- toolbox has a persistent scope selector (`Auto (selected block)`, `All`, `Top-level`, `Inside ...`) to narrow command list context before drag/drop; `Inside ...` entries are populated from catalog command contexts (not hardcoded)
- default toolbox scope is `Auto (selected block)`
- toolbox has a live filter field (`Filter commands...`) for narrowing the current scoped list by keyword
- `Auto (selected block)` resolves scope from the currently selected canvas block context (container selection -> inside that container; leaf selection -> parent container; no selection -> top-level)
- toolbox list sections are catalog-context driven from `resources/therion_command_catalog.json` context metadata and show supported draggable commands from each effective scope (excluding unsupported block-pair families not yet modeled in canvas)
- toolbox also includes a draggable `Comment` block for inserting full-line comments in the current context
- selecting a command item in toolbox shows compact help in the `Block Details` pane (third column): command title + `Summary` line only
- the editable `Block Details` section is visible only when a canvas block is selected; toolbox-command preview keeps only contextual help visible
- selected-block status in `Block Details` shows command context (`Command: ...`) without source line numbers
- in Blocks mode contextual help omits the `Syntax` section; help focuses on summary/arguments/options relevant to block-parameter editing
- right canvas renders parsed block hierarchy from current source and keeps order by source line
- Blocks mode enforces exactly one `encoding ...` directive at document root:
- if missing, it is auto-inserted at line 1 using the currently detected document encoding
- duplicate `encoding` directives are removed
- encoding card is sticky (not movable) and has no delete action
- all non-`encoding` cards are visually indented one level to reinforce `encoding` as document root
- right canvas also renders catalog-recognized leaf directives in-scope (for example `input`), not only the previously hardcoded centerline leaf pair
- `.thconfig` root-level directives like `select`, `export`, and `unselect` are rendered as top-level cards in Blocks mode
- `source` is context-sensitive in Blocks mode: `source file.th` stays a leaf card, while explicit `source` ... `endsource` is rendered as a container block
- right `Block Details` pane edits parameters of the selected block directly (no modal dialog for supported block kinds)
- blocks view uses a 3-column horizontal splitter (`Toolbox | Canvas | Block Details`), so `Block Details` can be resized wider for multi-column option/value editing
- dragging a toolbox item to the canvas inserts source templates at compatible positions
- inserted templates are intentionally bare skeletons (no example IDs/paths/values are auto-filled)
- dropping below the last canvas card appends a new top-level block at document end; dropping above the first card inserts at document start
- if a toolbox drop lands between cards (not exactly on one card), insertion context resolves from the nearest canvas block by vertical position
- while dragging from toolbox to canvas, the same dashed placement guide line is shown as for block reordering
- container blocks render persistent boundary guides in the left gutter: a vertical connector from header toward closure and a thicker closure marker line indicating block end-pair intent
- the thicker closure marker line is color-matched to the opening block card family (`survey`, `centerline`, etc.) for quick visual pairing
- dropping on/near a container closure marker (toolbox insert or canvas reorder) inserts after that container block boundary
- toolbox drop insertion now uses boundary zones: before/after target near edges, or inside compatible container in middle zone
- if drop context is incompatible, insertion is auto-promoted to the nearest valid ancestor scope (or root) to avoid hard failure dialogs in common workflows
- dragging an existing block card in canvas reorders source blocks (whole block is moved, including nested content for container blocks)
- blocks-mode right-column contextual help now uses the same framed header/body style and internal padding as Raw mode contextual help for consistent spacing
- full-line source comments (`# ...`) are rendered as `comment` cards in canvas and can be moved/deleted like other leaf cards
- while dragging a canvas card, a dashed horizontal placement guide line shows the current insertion boundary between blocks
- when dragging over a container card, dropping near the card top/bottom edge inserts before/after that block; dropping in the middle keeps container-child insertion behavior
- for container-compatible child blocks (for example comment under `survey`), dropping near container bottom edge inserts at the beginning of the container body (before first child), enabling direct placement between container header and first child
- dropping onto a compatible container (for example `survey`) moves the block inside that container near its end
- block canvas updates live when application/system appearance changes (light/dark), including immediate canvas background and boundary-guide redraw
- Blocks mode panel chrome uses consistent padding/spacing so toolbox, canvas, and details borders align more cleanly with surrounding window panes
- Raw mode uses the same 12/8 panel padding/spacing rhythm as Blocks mode, and keeps contextual help in a bounded right-side column for layout consistency
- Blocks Details/Contextual Help side pane keeps a bounded width, so wide windows allocate surplus space to the canvas instead of a mostly-empty help column
- Blocks right pane uses a single framed surface (no nested double-frame around help), keeping border weight and inner padding consistent between `Block Details` and `Contextual Help`
- in dark/light modes, contextual-help panes in both Raw and Blocks modes use a unified base-surface tone across panel and text area (no mixed dark patches in the same help pane)
- block cards use a trash icon action in the top-right corner for delete
- selecting a block card focuses it in `Block Details` for editing
- delete icon asks for confirmation and removes the full logical block span (`survey`/`map`/`scrap`/`centerline` with matching end directive, `data` with its measured rows, or single-line leaf directives)
- selecting a `Survey` / `Map` / `Scrap` / `Centerline` block enables structured header editing in details pane:
- `ID` field (`required` for `survey/map/scrap`, `optional` for `centerline`)
- option key/value table
- options section is shown only when the command supports options (or when option tokens already exist on that source line)
- option list actions are inline with the `Options` section header as compact `+` and `-` controls (scope is explicit to the options table)
- `+` appends an empty option row (no forced default), so custom options can still be entered
- `-` removes the currently selected option row and is disabled when no option row is selected
- for fixed multi-parameter options, the table `Value` cell is read-only and editing is done via `Selected Option Parameters` fields (prevents ambiguous token splitting)
- option/value cells support inline completion from catalog metadata while remaining editable as free text
- when an option has fixed multi-value arity from catalog metadata (for example `-person-rename`), Block Details shows a `Selected Option Parameters` subform with one field per parameter and validates exact parameter count on `Apply`
- when catalog defines enum-like allowed values for an option, invalid values are rejected by Block Details validation (`Apply` stays disabled and inline status explains the issue)
- every editable block exposes an always-visible optional inline `Comment` field for end-of-line comments (`... # comment`)
- block cards show a comment badge when inline comment exists; hover shows comment text tooltip
- action row keeps a clear visual gap before `Contextual Help` to separate editing controls from help content
- optional `Extra Arguments (Advanced)` preservation field (shown only when such unsupported positional tokens already exist in source)
- `Apply` writes the updated command line back to source as one undoable edit step
- `Apply` is enabled only when there is a valid change against current source line
- validation errors (for example required ID/value missing or invalid option key/value arity) are shown inline in details status and block `Apply`
- contextual help in details pane follows the currently active field/row (`ID` help or selected option help)
- contextual help in Block Details always stays at parent command/parameter level; selecting option rows does not replace the panel with option-only help
- selecting `Team`, `Explo Team`, `Date`, or `Explo Date` enables direct value editing in details pane (`Person`/`Value` field + `Apply`)
- for `Team`, details pane also exposes optional `Roles` tokens (for example `station`, `length`, `compass`) as a separate field
- for `Team`/`Explo Team`, `Apply` auto-quotes the person value when needed (for example names with spaces), preserving valid Therion tokenization
- simple command fields are argument-aware: Block Details labels positional fields from command argument metadata and parses current raw-line tokens back into those fields
- selecting `Data` enables split header editing in details pane (`Style` + `Readings Order` fields + `Apply`, serialized as `data <style> <readings order>`), while `Edit Data Rows...` opens the full mixed-row data editor for body rows/directives
- `Readings Order` in `Data` header mode is tag-based: type a token and confirm with `Enter`/`Space` (or choose from suggestions) to create a chip; remove a token via its `✕` chip control; duplicate tokens are ignored
- inserting a new `Data` command from the Blocks toolbox creates a bare `data` header line; fill `Style` and `Readings Order` in Block Details
- most in-scope command cards now edit directly in details pane; unsupported kinds keep a `Legacy Configure...` fallback button
- `Apply` / `Legacy Configure...` actions are placed above `Contextual Help` in a stable row to keep commit actions in a fixed position while help content changes/scrolls
- nested commands should be inserted through toolbox drag/drop, not through block-parameter editing
- `Data` block editor dialog:
- data header (`data <style> <readings order>`) is edited in Block Details; rows dialog uses that active header as schema
- measurement rows are shown in a table generated from the current column definition
- the rows dialog does not expose a second editable columns/header input
- `Add Data Row` / `Add Directive Row` / `Remove Row` control one combined row table
- `Add Comment Row` inserts a comment-only row
- if existing body rows do not match the current `data` header schema, the dialog shows a warning (`Schema no longer matches ...`)
- table width/column widths auto-resize based on current column definition and expand to fill available dialog width
- dialog auto-expands (up to available window/screen space) so all table columns stay visible when possible
- pressing `Enter` in the last table column moves to a new row and focuses the first column
- measurement rows and directive rows (for example `extend right`) can be mixed in any order in the same table
- row `Type` can be `data` or `directive`; `directive` rows use the `Directive` column text
- row `Type` also supports `comment` for comment-only lines
- row `Type` is edited via a pick list (`Data`, `Directive`, `Comment`)
- `Directive` column has inline suggestions (centerline commands/templates) and is editable only for `directive` rows
- data value columns are locked for `directive`/`comment` rows; `Directive` is locked for `data`/`comment` rows
- all rows expose a trailing `Comment` column for inline end-of-line notes
- data column headers in the rows table are shown as consistent humanized labels
- navigation/find commands automatically switch back to `Raw` mode when line-accurate text focus is needed

Main window status bar shows:

- left: transient app status (`Ready`, operation results)
- center/right: active document path and encoding
- when a `.th2` map editor tab is active: a color mode badge is shown in status bar (`Select` in green, `Insert` in red)
- long paths are middle-elided in visible text and preserved as full path in tooltip

### 5.4 Use Structure Sidebar

Structure pane shows simplified hierarchy with only `Survey` -> `Map` -> `Scrap` entries (no project-root or summary rows in the tree).
Each row shows an icon by item kind (`Survey`, `Map`, `Scrap`) for quicker scanning.
When a `map ... endmap` block references scraps, those scraps are shown under that map node.
Structure updates from in-memory tab content, so unsaved edits (for example deleting a scrap block) are reflected immediately.
Selecting an entry:

- opens the source file when needed
- syncs to editor line when relevant

### 5.5 Use Map Workspace (`.th2`)

Map tab uses a split map+text workspace by default. The embedded pane order is graphical map editor, synchronized source text editor, then the source contextual help inspector. The map pane is dedicated to graphical editing; there is no separate persistent `Map Help` pane below the canvas.

Map canvas appearance:

- canvas and geometry contrast adapts to current system light/dark appearance so lines, handles, labels, and grid stay readable
- canvas outer padding follows the same panel spacing rhythm as the adjacent source editor and contextual help inspector
- switching system light/dark appearance updates the map workspace and sidebar/status separators live, without restarting the app

Toolbar actions:

- mode buttons: `Select`, `Point`, `Line`, `Freehand`, `Smart Trace`, `Area`
- `Insert Scrap`
- `Complete Draft`
- `Undo`, `Redo`
- `Zoom -`, `Zoom +`
- `Fit`
- `Fit + BG`
- `Open Map in Window` / `Return Map Pane`

Interactive drawing (current):

- `Point`: click directly in the map canvas to insert a point immediately.
- `Line`: click to add draft vertices in the canvas, then press `Enter` or click `Complete Draft` to write the line; mode stays in `Line` so you can immediately draw the next line.
- In `Line` mode:
- click only: adds straight polyline anchors
- click and drag while placing a new anchor: creates a bezier segment with control points for curve shaping
- click+drag curve seeding uses the drag point as a curve pull point (quadratic-style), then maps it to cubic controls; this avoids rigid midpoint-coupled parallel handles across segment endpoints
- when click+drag creates a new curved segment from a vertex that already has the opposite control handle, that opposite handle is auto-mirrored so both controls stay on one tangent line
- after a curved segment exists, bezier control points are visible in the draft preview and can be dragged to adjust shape before commit
- while hovering a draft bezier control point, the map cursor changes to a hand; while dragging it, cursor changes to closed hand
- when a draft line vertex has both incoming and outgoing bezier controls, dragging one control mirrors/adapts the opposite control to keep smooth tangent behavior
- `Freehand`: press, drag, and release in the map canvas to insert a sampled line stroke in one gesture.
- `Area`: click to add draft vertices in the canvas, then press `Enter` or click `Complete Draft` to write the area; mode stays in `Area` so you can immediately draw the next area.
- In `Area` mode, bezier drafting behavior matches `Line` mode:
- click only: adds straight anchors
- click and drag while placing a new anchor: creates a curved segment with control points
- control points are visible in the draft preview and can be dragged before commit
- for curved areas, the closing segment (last anchor back to first anchor) is written as a smooth cubic segment when closing handles can be derived from first/last vertex tangents
- Area commits are serialized in Therion border-reference form: a closed `line border -id ... -close on` block is written first, then the `area ...` block references that border line id.
- The generated border line id for area commits uses `line-X` naming and increments to stay unique in the current scrap (`line-1`, `line-2`, ...).
- Auto-created scraps use `scrap-X` naming (`scrap-1`, `scrap-2`, ...).
- Area object `-id` is not auto-forced; only the referenced border `line` id is mandatory in generated output.
- If the file contains `##XTHERION## xth_me_area_adjust`, map insertion/render projection uses that rectangle as stable model bounds to keep hidden-background insertions and later unhide aligned.
- Line/area commit preserves all captured vertices from the current draft session.
- Line commits are serialized as per-vertex coordinate rows; rows with bezier control points are written for curved segments.
- While drafting `Line`/`Area`: `Backspace`/`Delete` removes the last draft vertex.
- `Esc` exits active insert mode and returns to `Select` mode:
- in `Line`/`Area`, if draft has enough captured vertices it is committed before exiting; otherwise the incomplete draft is canceled
- in `Point`/`Freehand`, the active insert mode is canceled
- `Smart Trace` still uses the existing draft-card workflow (not full trace capture yet).

Detached map-pane behavior:

- `Open Map in Window` detaches the graphical map pane into its own top-level window (useful for a second monitor).
- The text editor stays in the main tab.
- Closing the detached map window (or clicking `Return Map Pane`) reattaches the same map pane back to the tab.
- While detached, the embedded map pane area in the main tab is hidden.

Line-handle behavior:

- line and area edit vertices are shown only for the currently selected map object
- line control handles/connectors are shown only for the currently selected line vertex (or its selected control handle)
- line accent/highlight overlay is shown only for the currently selected line object
- selecting a map object (card or geometry) moves the text editor cursor to that object's source line
- selecting a map line/area vertex (including line control points) moves the text cursor to that specific vertex coordinate token
- moving the text cursor onto a line/area coordinate token selects the corresponding map vertex/control point
- moving the text cursor onto vertex-related line-option rows (for example `smooth off`) selects the corresponding current line vertex
- moving the text cursor onto `scrap` or `endscrap` selects all map objects that belong to that scrap block
- dragging a line anchor moves attached incoming/outgoing control handles by the same delta
- on smooth vertices, dragging one control handle updates the opposite control handle live (mirrored/collinear) during drag

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
