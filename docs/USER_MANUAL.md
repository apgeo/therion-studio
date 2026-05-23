# Therion Studio User Manual

Last updated: 2026-05-23

This manual describes the currently implemented behavior.
Update this file whenever UI layout, workflows, keyboard shortcuts, or settings behavior changes.

## 1. Application Overview

Therion Studio is a Qt desktop editor for Therion projects with:

- project file browsing
- text editing for `.th`, `.th2`, and `.thconfig`
- structure and map-object navigation
- TH2 map workspace with source-linked geometry editing
- integrated Therion runner console

Editing model principles:

- Therion files remain plain-text source files (`.th`, `.th2`, `thconfig`); all editor modes write back to source text.
- `Raw` mode is the canonical expert surface for exact token/line-level editing.
- `Blocks` mode is a guided, lower-barrier structured editor intended to reduce syntax complexity for newer users while preserving source fidelity.
- TH2 `Visual` mode is a specialized map-editing surface for `.th2` geometry workflows that are cumbersome in raw text alone.

## 2. Main Window Layout

The main window is organized into:

- Menu bar (`File`, `Edit`, `Map`, `View`, `Window`, `Help`)
- Left sidebar with activity rail and switchable panes
- Central tab area (text tabs and map-editor tabs)
- Status bar (global app status)
- Splitter handles use a consistent native visible grab-handle style across sidebar/content, raw-help, blocks columns, and map inspector splits
- Main sidebar/content/editor seams avoid double borders; the editor area owns the continuous left seam and embedded canvases do not duplicate it
- Main document chrome uses a unified spacing/contrast system: tabs, top command toolbar, sidebar panels, and contextual-help panels share consistent separator tone, control sizing, and surface hierarchy in light/dark modes

### 2.1 Sidebar Activity Rail and Panes

The sidebar has an always-visible activity rail with 3 panes:

- `Files`
- `Structure`
- `Compiler`

Behavior:

- Clicking an activity icon opens that pane.
- Clicking the active icon again collapses the sidebar to the rail.
- Clicking any icon while collapsed expands sidebar and opens that pane.
- Dragging the sidebar content resize handle below the minimum usable width snaps the content pane closed while keeping the activity rail fixed and visible.
- Selection/map-object/background workflows are handled directly in TH2 Visual-mode right-side Inspector (`Selection`, `Objects`, and `Backgrounds` tabs), not in a dedicated rail pane.
- Activity-rail buttons use larger borderless icon buttons with hover/active highlight states tuned for both light and dark appearances.

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
- `Show Compiler` (switches sidebar to Compiler pane)

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
- for `.th`/`.thconfig`, the `Mode` switcher is in the full-width document toolbar row above the file tabs, not in an extra row inside editor content
- find/replace bar with `Whole word` and `Case sensitive` options
- completion popup is shown while typing (commands/options/values), sourced from command catalog metadata; `Ctrl+Space` remains available as manual trigger
- when completion popup is visible, confirm a suggestion with `Enter`, `Tab`, or mouse click; `Esc` closes the popup
- raw editor and contextual-help inspector are separated by a thin divider handle (no wide gutter)
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
- Raw mode uses a right-column contextual help pane, resizable via the same native visible splitter handle style used in all views
- contextual-help text presentation uses consistent paragraph/list spacing and line height in both Raw and Blocks panes for improved readability
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
- blocks-mode contextual help keeps internal padding parity with Raw mode and no additional inner border treatment
- full-line source comments (`# ...`) are rendered as `comment` cards in canvas and can be moved/deleted like other leaf cards
- while dragging a canvas card, a dashed horizontal placement guide line shows the current insertion boundary between blocks
- when dragging over a container card, dropping near the card top/bottom edge inserts before/after that block; dropping in the middle keeps container-child insertion behavior
- for container-compatible child blocks (for example comment under `survey`), dropping near container bottom edge inserts at the beginning of the container body (before first child), enabling direct placement between container header and first child
- dropping onto a compatible container (for example `survey`) moves the block inside that container near its end
- block canvas updates live when application/system appearance changes (light/dark), including immediate canvas background and boundary-guide redraw
- Blocks mode panel chrome uses consistent padding/spacing so toolbox, canvas, and details borders align more cleanly with surrounding window panes
- Blocks-mode toolbox splitter pane uses a left-side divider only (no top/right/bottom border) and padded content inset consistent with contextual-help spacing
- Raw mode uses the same 12/8 panel padding/spacing rhythm as Blocks mode, and keeps contextual help in a bounded right-side column for layout consistency
- Blocks Details/Contextual Help side pane keeps a bounded width, so wide windows allocate surplus space to the canvas instead of a mostly-empty help column
- Blocks right pane uses a single details surface (no nested double-frame around help); contextual help section is visually separated by a left divider
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
- right: active document encoding
- when a `.th2` map editor tab is active: current zoom appears before the color mode badge (`Select` in green, `Insert` in red)
- active document file names and paths are intentionally kept out of the status bar; use the tab title for document identity

### 5.4 Use Structure Sidebar

Structure pane shows simplified hierarchy with only `Survey` -> `Map` -> `Scrap` entries (no project-root or summary rows in the tree).
Each row shows an icon by item kind (`Survey`, `Map`, `Scrap`) for quicker scanning.
When a `map ... endmap` block references scraps, those scraps are shown under that map node.
Structure updates from in-memory tab content, so unsaved edits (for example deleting a scrap block) are reflected immediately.
Selecting an entry:

- opens the source file when needed
- syncs to editor line when relevant

### 5.5 Use Map Workspace (`.th2`)

Map tab uses explicit workspace modes:

- `Visual`: graphical map editor + right-side `Inspector` tabs (`Selection`, `Objects`, `Backgrounds`)
- `Raw`: source text editor + right-side contextual help inspector
- the full-width document toolbar row is shown above the file tabs for all document types
- the command toolbar sits directly above the native file tabs without an extra bottom border
- file tabs use native platform tab rendering and sit on the `QTabWidget` editor/canvas frame below them
- file tabs use native platform tab insets; the command toolbar keeps compact internal padding with matching top/bottom/left spacing at the leading edge
- embedded Visual mode uses the same thin top content separator under the file tabs as Raw/Blocks mode
- Visual inspector tabs use native `QTabWidget` rendering and sit in a borderless panel matching the other editor side panes
- the editor area uses a single dedicated left divider between sidebar content and document chrome; embedded canvases do not duplicate that divider
- from left, the toolbar defaults to `Save`, divider, `Undo`/`Redo`, divider
- `Save`, `Undo`, and `Redo` are icon-only buttons (with tooltips) in both main and detached map toolbars
- when a `.th2` map tab is active, the next left-side toolbar groups are:
- `Zoom In`, `Zoom Out`, `Fit`, `Fit With Background`
- `Select`, `Complete Draft`, divider, `Insert Scrap`, `Point`, `Line`, `Freehand`, `Area`
- when a `.th2` map tab is active in the main window, the right side of that toolbar shows compact square icon-only controls for `Visual`, `Raw`, and `Separate Map` / `Return Map` (in that order); `Separate Map` uses the screen-share icon and `Return Map` uses the screen-share-off icon
- `.th` / `.thconfig` right-side `Raw` / `Blocks` mode controls are also compact square icon-only controls, with tooltips and accessible names
- when a map editor is shown outside the main tab strip (detached map window), an equivalent top command toolbar is shown above the map canvas and inspector, but `Visual`/`Raw` buttons are omitted there
- detached map windows draw one separator below the window titlebar and one separator below their command toolbar above the canvas/inspector content

The map pane is dedicated to graphical editing; there is no separate persistent `Map Help` pane below the canvas.

Map canvas appearance:

- canvas and geometry contrast adapts to current system light/dark appearance so lines, handles, labels, and grid stay readable
- map, raw-editor, and blocks-canvas surfaces are edge-aligned with no extra outer margin around the main content area
- switching system light/dark appearance updates the map workspace and sidebar/status separators live, without restarting the app

Inspector panel (`Visual` mode):

- `Selection` tab contains details and editing controls for the currently selected map object
- `Objects` tab contains TH2 objects grouped by scrap in a source-line-linked tree
- `Backgrounds` tab starts with visual grid controls, followed by background-layer controls that were previously in the left sidebar `Map` pane
- The tab labels are the inspector heading; there is no extra standalone `Inspector` title above them
- The inspector tab pane only reinforces the left edge when needed; the remaining tab pane borders keep native styling
- `Selection` and `Backgrounds` use the same framed section style, with section headings inside their boxes
- Visual-mode Inspector uses compact outer margins; the `Objects` tree starts directly under its tab, and selection/background controls use padded inner groups where needed
- in the `Backgrounds` tab, grid controls are first; layer add uses a compact `+` button in the `Layers` header; per-layer visibility and removal use right-aligned row icons, while reorder actions remain below the layer list

- in `Objects`:
- each source-linked tree row includes compact visibility and delete icon actions
- visibility/delete icon actions use pointing-hand hover feedback; label/grip hover is reserved for row dragging
- map-object rows use the same command-kind icons as the map toolbar (`Scrap`, `Point`, `Line`, `Area`), with row text formatted from the Therion symbol type/subtype and optional id/name, e.g. `wall blocks: wall-1` or `station fixed: 4@hp`
- clicking an already selected object row again clears the object selection
- selecting an area also highlights the referenced border line in the canvas, making the generated/owned boundary visible while the area remains the primary selected object
- source-linked point, line, and area rows show a grip handle; hovering the label or grip uses an open-hand cursor, and dragging changes to a closed-hand cursor; drag the row onto another object row to move the corresponding TH2 source span before or after the target row depending on where you release, or onto a scrap row to move the object into that scrap before `endscrap`; a slim highlighted horizontal drop-target line shows the exact insertion edge for real moves, invalid targets and current-location boundaries hide the drop line and use a forbidden cursor with a status note, the tree auto-scrolls near its top/bottom edge during drag, and the move participates in the map undo/redo stack without creating a duplicate raw-text undo step
- the visibility icon (`eye` / `eye-off`) hides or shows the corresponding map object in the current editor view without changing the source file
- the delete icon (`trash`) asks for confirmation and removes the corresponding source command span; block objects such as `line`, `area`, and `scrap` remove their full matching `end...` block, and the removal is available through document undo/redo
- a line referenced by an area cannot be deleted separately from the object list or selected-object actions; delete the area instead
- deleting an area also removes its private referenced border line when that line is not used by another remaining area
- in `Selection`:
- groups editing controls into a selected-object section (`Scrap`, `Point`, `Line`, or `Area`), `Geometry`, `Point / Vertex`, and `Actions` sections
- when the selected object is a line used as an area border, `Delete Object` is disabled and the Object section shows `Used by area: ...`; click the area name to select the owning area
- the selected-object section shows `Source line N` and object-level quick fields
- the selected-object section also provides quick-edit fields in a stable command-focused order; scraps show ID and `Projection (-projection)` because scraps do not have type/subtype
- point, line, and area selections show `ID (-id)`, type, and subtype where supported; station points also show separate `Name (-name)` below subtype, so station name is no longer shown in place of ID
- `Geometry` appears when geometry state controls such as `Closed (-close)` and `Reversed (-reverse)` are available
- `Point / Vertex` appears when a point, vertex, or control point is selected and contains XTherion-compatible line-point controls, orientation controls, and vertex actions where applicable; exact coordinate edits remain in Raw mode
- selected line vertices expose XTherion-style `<<`, `Smooth (-smooth)`, and `>>` checkboxes plus `Insert Vertex` and `Delete Vertex` actions; `<<` controls the incoming handle, `>>` controls the outgoing handle, and unchecking either handle forces smooth off
- checking `Smooth (-smooth)` creates any missing line-point handles, aligns them into a smooth tangent, and removes `smooth off`; unchecking it keeps handles but writes `smooth off`, while the `S` shortcut still toggles the same selected-vertex state
- converting a straight line vertex into a Bezier vertex preserves supported line-point metadata such as `orientation` and slope `l-size`
- line-point control toggles and control-point drags keep the same logical vertex selected after the map scene refreshes
- `Actions` contains catalog-driven `Edit Object Settings...` and `Delete Object`; switch to `Raw` when source navigation/editing is needed
- for point symbols and selected line anchor vertices, orientation controls appear only when catalog metadata marks `-orientation` as supported for the current command type/subtype:
- `Orientation override (-orientation)` enable/disable; checking it immediately writes an explicit orientation value so the map handle appears, unchecking it removes the override, and changing the degree value updates the source/map immediately without dropping the current point or vertex selection
- degree input constrained to `0..359.999`; value changes are applied immediately
- for selected `line slope` anchor vertices, `Left size (-l-size)` can be enabled and edited with a positive numeric value
- `Left size (-l-size)` state and value changes are applied immediately; new line-point values are written in XTherion-compatible indented rows such as `orientation 90` and `l-size 40.0`
- selecting an orientable point in the map canvas shows a compact red orientation handle when `-orientation` is explicitly set, including `0`; drag it to update the point's `-orientation` directly from the map
- selecting a `line slope` anchor in the map canvas also shows a red XTherion-style line-point handle when that line point has an explicit orientation value, including `0`; drag it to update both `orientation` and `l-size` directly from the map
- for line anchors without an existing orientation row, checking `Orientation override` seeds the value perpendicular to the current local line direction instead of using `0`
- when a `scrap` is selected, shows `Scrap Scale` controls for picture point 1/2 in pixels, real point 1/2, and unit (`m`, `cm`, `mm`, `ft`, `in`)
- picture point fields are displayed as whole pixels; real point fields use compact decimal precision
- `Scrap Scale` uses stacked coordinate rows with compact X/Y fields so it remains usable in narrow inspectors
- `Use Source Bounds` fills the scrap scale form from the current map source bounds using the XTherion default inch-to-meter convention
- `Apply Scale` writes an XTherion-compatible 8-parameter `-scale [...]` option to the selected scrap command
- includes `Edit Object Settings...` for `scrap`, `point`, `line`, and `area`; this opens the same catalog-driven option editor used by structured block selection
- `Edit Object Settings...` shows parsed command arguments as protected rows in the options table: point `x`/`y`/`type`, line `type`, area `type`, and scrap `id` where present
- object identifiers stored as `-id` appear in the options table as `-id`, not as a positional ID field for commands such as `point` or `line`
- the dialog opens at a balanced width, with a wider option column so option names and values remain readable
- for map objects, contextual help stays on the selected object's command help instead of changing to per-option help when table rows are selected
- option editors keep bracketed multi-token values such as scrap `-scale [...]` in one value cell, including negative picture coordinates
- if no map geometry item is selected, `Edit Object Settings...` can target the command under the current text cursor line when that line is `scrap`, `point`, `line`, or `area`
- edits update TH2 source text immediately and stay in the same undo/redo workflow

Toolbar actions:

- map toolbar commands are shown in the top command row (not as a floating in-canvas overlay)
- controls are compact icon-first Lucide buttons; hover any icon to see the text label
- top document-toolbar view actions (TH2): `Zoom In`, `Zoom Out`, `Fit`, `Fit With Background`
- current map zoom is shown in the main status bar before the `Select`/`Insert` mode badge
- history actions: `Undo`, `Redo`
- selection/draft actions: `Select`, `Complete Draft`
- insertion/drawing tools: `Insert Scrap`, `Point`, `Line`, `Freehand`, `Area`
- a divider separates draft completion from insertion/drawing tools; there is no trailing divider after `Area`
- icons recolor with the active light/dark application palette

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
- `Freehand`: press, drag, and release in the map canvas to insert a line in one gesture; the live preview shows the drawn stroke as a solid line without per-sample point markers, and the sampled stroke is simplified into bezier coordinate rows rather than a dense point-by-point polyline. Simplification is shape-sensitive: simple strokes use fewer anchors, while more complex strokes keep more anchors to preserve the drawn curve.
- `Area`: click to add draft vertices in the canvas, then press `Enter` or click `Complete Draft` to write the area; mode stays in `Area` so you can immediately draw the next area.
- In `Area` mode, bezier drafting behavior matches `Line` mode:
- click only: adds straight anchors
- click and drag while placing a new anchor: creates a curved segment with control points
- control points are visible in the draft preview and can be dragged before commit
- for curved areas, the closing segment (last anchor back to first anchor) is written as a smooth cubic segment when closing handles can be derived from first/last vertex tangents
- Area commits are serialized in Therion border-reference form: a closed `line border -id ... -close on` block is written first, then the `area ...` block references that border line id.
- The generated border line id for area commits uses `line-X` naming and increments to stay unique in the current scrap (`line-1`, `line-2`, ...).
- Auto-created scraps use `scrap-X` naming (`scrap-1`, `scrap-2`, ...).
- Scraps inserted from `Visual` mode include an XTherion-compatible default `-scale` derived from the current map source bounds. This matches XTherion's convention of mapping the source-bounds width to inches converted to meters (`width * 0.0254 m`).
- Existing scrap projection and scale can be manually edited from `Inspector -> Selection` after selecting the scrap; projection rewrites the scrap `-projection` option, and scale rewrites the scrap `-scale` option in XTherion-compatible 8-point calibration form.
- Area object `-id` is not auto-forced; only the referenced border `line` id is mandatory in generated output.
- If the file contains `##XTHERION## xth_me_area_adjust`, map insertion/render projection uses that rectangle as stable model bounds to keep hidden-background insertions and later unhide aligned.
- Line/area commit preserves all captured vertices from the current draft session.
- Line commits are serialized as per-vertex coordinate rows; rows with bezier control points are written for curved segments.
- While drafting `Line`/`Area`: `Backspace`/`Delete` removes the last draft vertex.
- `Esc` exits active insert mode and returns to `Select` mode:
- in `Line`/`Area`, if draft has enough captured vertices it is committed before exiting; otherwise the incomplete draft is canceled
- in `Point`/`Freehand`, the active insert mode is canceled

Detached map-pane behavior:

- `Separate Map` (document toolbar row above tabs) detaches the graphical map pane into its own top-level window (useful for a second monitor).
- Detached map window keeps the same top command toolbar groups and right-side `Inspector` tabs as embedded `Visual` mode, except detached toolbar omits `Visual`/`Raw` and shows icon-only `Return Map` with the screen-share-off icon.
- The main tab keeps the raw text editor visible while detached.
- While detached, the main-window toolbar hides map zoom/drawing/selection groups for that TH2 tab; use the detached map window toolbar for map commands.
- Detached map window shows its own status bar for map zoom and `Select`/`Insert` mode.
- Main-window map zoom/mode badges are hidden while detached, so map status lives with the detached map window.
- Closing the detached map window (or clicking `Return Map`) reattaches the same map pane back to the tab.
- While detached, map and source remain synchronized as one TH2 session.

Line-handle behavior:

- line and area edit vertices are shown only for the currently selected map object
- line control handles/connectors are shown only for the currently selected line vertex (or its selected control handle)
- line accent/highlight overlay is shown only for the currently selected line object
- selecting a line also shows a yellow XTherion-style direction tick at the first vertex; the tick is perpendicular to the first segment tangent and marks the left side of the oriented line
- `Reversed (-reverse)` flips the line's effective orientation, so the tick and orientation-dependent symbols move to the opposite side
- for pitch/pit-style teeth, the teeth are on the tick side; this follows Therion's general rule that free space is on the left side of an oriented line and rock is on the right
- selecting a map object (card or geometry) moves the text editor cursor to that object's source line
- selecting a map line/area vertex (including line control points) moves the text cursor to that specific vertex coordinate token
- moving the text cursor onto a line/area coordinate token selects the corresponding map vertex/control point
- visible bezier control points take precedence over nearby line/area shapes when clicking or dragging, so controls remain editable even when they sit close to another line
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
- there is no dedicated `Touch Controls` toolbar mode; navigation uses automatic input-device handling

Zoom constraints:

- zoom is clamped to `0.1 .. 50.0`

### 5.7 Background Image Management (Inspector `Backgrounds` Tab)

In `Visual` mode `Inspector -> Backgrounds`, the `Background Images` controls provide:

- square metric grid controls (`Show grid`, `Spacing (m)`) for the visual map canvas; when the TH2 scrap has `-scale`, numeric, unit, ratio, and XTherion-style calibration forms are converted from meters to TH2 source units so the grid can be used as a real-world scale reference
- layer list with selection
- add (`+`) button (multi-file picker); adding raster images writes XTherion header metadata (`xth_me_area_adjust`, `xth_me_area_zoom_to`, and `xth_me_image_insert`) to the TH2 source using document-relative paths where possible
- per-layer visibility icon (`eye` / `eye-off`) in the list row
- per-layer delete icon (`trash`) in the list row
- `Up`, `Down`
- position `X`, `Y` fields
- raster layer position, visibility, and gamma edits update the matching XTherion metadata line; deleting a metadata-backed raster layer removes that line from the TH2 source
- opacity slider with reset
- gamma slider with reset

Persistence:

- layer stack, visibility, position, opacity, gamma are persisted per document in session settings.
- map editor also attempts to load layers from XTherion metadata in source text.

### 5.8 Run Therion (Compiler Pane)

Compiler pane fields:

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
- output streams to compiler pane

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
- macOS distribution is intended to use a Homebrew formula. Qt is provided by formula dependencies rather than bundled inside `TherionStudio.app`.

### 7.2 Windows

- Cross-platform shortcuts are designed to work via Qt standard key-sequence mapping.
- Map insertion supports both `Insert` and `I`.
- Windows distribution is intended to use an installer that includes Therion Studio and the required Qt runtime only. The external `therion.exe` runner is configured separately.

### 7.3 Linux

- Cross-platform shortcuts are designed to work via Qt standard key-sequence mapping.
- Map insertion supports both `Insert` and `I`.

## 8. Known Current Limitations

- Non-UTF-8 support relies on Qt-supported codecs and may not perfectly decode every legacy encoding variant on every platform.
- Freehand mode inserts simplified bezier line geometry from the dragged stroke.
- Smart Trace is not currently exposed in the toolbar; it remains a future tracing feature rather than a staged user workflow.
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

### 9.3 Open in Map Editor unavailable

Symptom:

- `Map -> Open Current Document in Map Editor` is disabled.

Fix:

- Activate a `.th2` document tab.

### 9.4 Rename/delete blocked in project browser

Symptom:

- operation blocked by open tabs.

Fix:

- Close tabs that reference the target file/folder, then retry.
