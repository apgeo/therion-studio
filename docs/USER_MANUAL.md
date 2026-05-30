# Therion Studio User Manual

Last updated: 2026-05-30

This guide covers end-user workflows in Therion Studio.

## 1. What Therion Studio Is

Therion Studio is a desktop editor for Therion cave-mapping projects. It provides:

- project file browsing
- text editing for `.th`, `.th2`, and `thconfig`
- structure navigation (`Survey` / `Map` / `Scrap`)
- visual map editing for `.th2` files
- integrated Therion run console

## 2. Main Window

The app window has four main areas:

- menu bar (`File`, `Edit`, `Map`, `View`, `Window`, `Help`)
- top command bar with quick actions (`Open Project`, `Close Project`, `Save`, editor/map tools)
- left sidebar with activity icons (`Files`, `Structure`, `Compiler`) and a `Compile` action
- center tab area for documents
- bottom status bar (app state, compile state, encoding, and map status)

The main window opens at a usable desktop size and clamps restored session geometry only enough
to avoid unusably narrow windows while still allowing common half-screen layouts.

## 3. Getting Started

### 3.1 Open a Project

1. Select `File -> Open Project...`.
2. Choose your project folder.
3. Use the `Files` pane to open documents.

When no project is open, the `Welcome` tab shows an `Open Project...` button.
When a project is open but no document tab is active, the `Welcome` tab shows `Open file from sidebar`.

### 3.2 Open Documents

- Double-click a file in `Files`.
- `.th2` files open in the map editor.
- Supported text files open in the text editor (`.th`, `.thconfig`, `.log`, `.txt`, and generic plain-text files).
- Unsupported files (for example images or PDF) show an `Unsupported file` message with `Open in External App`.

### 3.3 Save Changes

- Use `File -> Save` for the active tab.
- Use `File -> Save All` to save all modified tabs.

## 4. Text Editing

### 4.1 Modes

For `.th` and `thconfig` files:

- `Raw` mode: direct text editing with syntax highlighting, find/replace, and autocomplete
- `Blocks` mode: structured editing of supported commands and blocks

For `.th2` files:

- `Raw` mode: text editing
- `Visual` mode: map editing with canvas + inspector

### 4.2 Useful Text Features

- line numbers and active-line highlight
- command/option/value autocomplete while typing
- `Ctrl/Cmd+Space` to open autocomplete manually
- find and replace via `Edit` menu
- unsupported file encodings can be converted to UTF-8 with confirmation

## 5. Structure and File Operations

### 5.1 Structure Pane

The `Structure` pane shows detected `Survey`, `Map`, and `Scrap` hierarchy from open content. Selecting an item opens/synchronizes to its source location.

### 5.2 Project Tree Actions

In the `Files` pane you can:

- create files/folders
- rename files/folders
- duplicate files
- delete files/folders
- open `.th2` directly in map editor

If a target file/folder is currently open in tabs, rename/delete is blocked for safety.

## 6. Visual Map Editing (`.th2`)

### 6.1 Modes and Inspector

`Visual` mode includes:

- map canvas
- inspector tabs: `Selection`, `Objects`, `Backgrounds`

`Raw` mode stays available for direct source editing.

### 6.2 Main Map Tools

Toolbar actions include:

- zoom: `Zoom In`, `Zoom Out`, `Fit`, `Fit With Background`
- selection/drafting: `Select`, `Complete Draft`
- insertion: `Insert Scrap`, `Point`, `Line`, `Freehand`, `Area`

Canvas rendering is zoom-aware: geometry strokes and edit handles are reduced at distant zoom levels so overview remains readable.
Point, line, and area rendering can be customized with JSON style overrides; see `Configuration -> Custom Map Object Styles`.
Line objects render as a single styled stroke; there is no separate secondary detail-stroke layer.
Area fills honor Therion's default `-clip on` visually when the area is inside a scrap with a resolvable wall boundary. The canvas clips the fill and fill pattern to the scrap wall outline; areas with `-clip off` render without this scrap-boundary clipping.

### 6.3 Drawing Basics

- `Point`: click to insert a point
- `Line`: click vertices, then press `Enter` or `Complete Draft`; click the first vertex again to finish as a closed line (`-close on`)
- `Area`: click vertices, then press `Enter` or `Complete Draft`
- `Freehand`: press-drag-release to insert a line

Starting `Point`, `Line`, `Freehand`, or `Area` activates `Inspector -> Selection` before the first point/vertex is placed. Use the pending object fields there to set type, subtype, ID, and point name where applicable; the same values are written when the new object is inserted.

`Insert Scrap` opens a pending scrap in `Inspector -> Selection` first. Set the scrap ID/projection there, then click `Insert Scrap` again to create the scrap block.

While drafting line/area:

- `Backspace`/`Delete` removes the last draft vertex
- `Esc` exits insert mode from the canvas, command toolbar, or Selection inspector fields (and commits only when a line/area draft is sufficiently complete)

### 6.4 Selection and Object Editing

In `Inspector -> Selection`, you can edit properties for selected `Scrap`, `Point`, `Line`, or `Area`, including common fields like ID/type/subtype where supported.

For point types that support Therion `-orientation`, the Selection inspector shows an orientation override. Enabling it writes `-orientation`; the point symbol rotates around its anchor using `0` as north/up and `90` as east/right. Station names stay screen-aligned for readability. Bundled `point.label` text follows `-orientation`, with the text baseline aligned to the orientation direction. A selected orientable point with explicit `-orientation` also shows a draggable direction handle on the canvas.

For point, line, and area objects, a style preview appears under `Subtype` as a full-width preview tile. The preview updates for selected objects and for pending insert objects before they are placed on the map. The tile uses a map-like light preview surface in both light and dark themes so dark map symbols remain readable. Area previews fill the preview tile so fill patterns are easier to inspect. The preview may still use preview-only contrast treatment for very low-contrast styles; the actual map rendering uses the configured style colors.

When no map object or pending insert object is selected, the `Selection` tab shows the empty state instead of reusing the current text-cursor line.

For filled areas, click inside the fill to select the area. Click directly on a line stroke, vertex, or control handle to select/edit the line instead.

In `Inspector -> Objects`, you can:

- select objects
- reorder objects by drag/drop
- toggle visibility in the current view
- delete objects (with confirmation)

### 6.5 Detached Map Window

Use `Separate Map` to move the visual map pane into its own window (for multi-monitor workflows). Use `Return Map` or close that window to reattach.

### 6.6 Background Images

In `Inspector -> Backgrounds`, you can:

- add/remove/reorder background layers (raster images and `.xvi`)
- adjust per-layer visibility, position, and opacity
- adjust gamma for raster layers (`.xvi` uses fixed gamma)
- `.xvi` backgrounds render vector shots, sketch lines, and their embedded `XVIgrid` when present

The visual map editor does not provide a separate generated metric grid. Use background layers, including `.xvi` files, for reference grid content.

## 7. Running Therion

Open the `Compiler` pane from the activity rail.

### 7.1 Main Fields

- `Executable`
- `Arguments`
- `Run Target` (`Current Config` or `Project Config`)
- `Target Config`
- `Working Directory Override`

### 7.2 Actions

- `Run Therion`
- `Stop`
- `Clear Output`
- `Copy Output`

The status bar shows compile state (`Idle`, `Running`, `OK`, `Failed`).

## 8. Keyboard Shortcuts

Platform key substitution follows Qt defaults (`Ctrl` on Windows/Linux, `Command` on macOS where applicable).

| Action | Shortcut |
|---|---|
| New window | Platform default `QKeySequence::New` |
| Open project | Platform default `QKeySequence::Open` |
| Save | Platform default `QKeySequence::Save` |
| Save all | Platform default `QKeySequence::SaveAs` |
| Close app | Platform default `QKeySequence::Quit` |
| Undo | Platform default `QKeySequence::Undo` |
| Redo | Platform default `QKeySequence::Redo` |
| Find | Platform default `QKeySequence::Find` |
| Find and replace | Platform default `QKeySequence::Replace` |
| Completion popup (text editor) | `Ctrl+Space` |
| Close all tabs | `Ctrl/Cmd+Shift+W` |
| Open current `.th2` in map editor | `Ctrl/Cmd+Alt+Shift+G` |
| Split selected map line segment | `Insert` or `I` |
| Delete selected map middle anchor | `Delete` or `Backspace` |
| Toggle selected map vertex smooth/corner | `S` |

## 9. Configuration

### 9.1 Custom Map Object Styles

Map object rendering can be customized with JSON style override files. User overrides live in the application data `map_object_styles` directory.

Default platform locations are:

| Platform | Override directory |
|---|---|
| macOS | `~/Library/Application Support/Therion Studio/map_object_styles/` |
| Windows | `%APPDATA%\Therion Studio\map_object_styles\` |
| Linux | `~/.local/share/Therion Studio/map_object_styles/` |

On Linux, if `XDG_DATA_HOME` is set, the base directory is `$XDG_DATA_HOME` instead of `~/.local/share`.

Override files are partial JSON objects. Include only the fields you want to change.

File naming controls the style scope:

| File name | Scope |
|---|---|
| `point.json` | default style for all points |
| `point.<raw_type>.json` | style for a point type, for example `point.station.json` |
| `point.<raw_type>.<raw_subtype>.json` | style for a point subtype |
| `line.json` | default style for all lines |
| `line.<raw_type>.json` | style for a line type, for example `line.wall.json` |
| `line.<raw_type>.<raw_subtype>.json` | style for a line subtype, for example `line.wall.presumed.json` |
| `area.json` | default style for all areas |
| `area.<raw_type>.json` | style for an area type, for example `area.water.json` |
| `area.<raw_type>.<raw_subtype>.json` | style for an area subtype |

Defaults are applied first, then type files, then subtype files. More specific files override less specific files.

#### Point Style Parameters

Point style files support:

| Parameter | Type | Meaning |
|---|---|---|
| `symbol_parts` | array | Point symbol geometry. Each part uses a `kind`; built-in symbol kinds are `circle`, `oval`, `square`, `diamond`, `triangle`, `star`, `asterisk`, `plus`, and `x`. Primitive geometry kinds are `line`, `polyline`, `polygon`, and `ellipse`. |
| `size` | number | Symbol bounding-box size. Every symbol fits into `size x size`; for `circle`, this is the diameter. |
| `stroke_width` | number | Symbol outline or line width. |
| `stroke_color` | color string | Symbol outline color, for example `#1C1C1E` or `#F4F4F2E6`. |
| `fill_color` | color string | Fill color for filled symbols. `plus`, `x`, and `asterisk` are stroke-only symbols. |
| `label_field` | string | Optional Therion option name to render next to the point, without the leading dash. For example `name` reads `-name`; `text` reads `-text`. |
| `label_orientation` | string | `screen` keeps style-driven labels horizontal. `orientation` rotates label text from `-orientation`; this is used by bundled `point.label`. |

Example:

```json
{
  "label_field": "name",
  "size": 12.0,
  "stroke_width": 1.2,
  "stroke_color": "#1C1C1E",
  "fill_color": "#FFD60A",
  "symbol_parts": [
    { "kind": "triangle", "size": 12.0 }
  ]
}
```

For built-in symbol parts, use `x`, `y`, `size`, and optional `angle`. For `line`, use `x1`, `y1`, `x2`, `y2`. For `polyline` and `polygon`, use `points` as arrays such as `[[-3, 0], [0, -4], [3, 0]]`; `polygon` may use `fill: true`. For `ellipse`, use `x`, `y`, `width`, `height`, optional `angle`, and optional `fill: true`. Any part may override `fill_color`, `stroke_color`, and `stroke_width`; `stroke_width: 0` means the thinnest supported rendered stroke. When omitted, the owning point, line decoration, or area pattern color is used. All coordinates are relative to the owning style's `size`.

Bundled `point.station.json` renders `-name` next to station points. Bundled `point.label.json` renders `-text` next to label points and rotates that text according to `-orientation` when present. For `label_field: "text"`, the map canvas interprets common Therion label formatting tags such as `<br>`, `<center>/<centre>`, `<left>`, `<right>`, `<thsp>`, `<rm>`, `<it>`, `<bf>`, `<ss>`, `<si>`, `<rtl>`, `</rtl>`, and `<size:...>` when drawing the label. Alignment tags affect multi-line labels with `<br>`.

Several bundled point styles are approximated from Therion's SKBB or UIS MetaPost symbols using `symbol_parts`, including simple sand, debris, pebbles, blocks, ice, snow, crystal, gypsum, and cave-pearl symbols.

#### Line Style Parameters

Line style files support:

| Parameter | Type | Meaning |
|---|---|---|
| `stroke_visible` | boolean | Whether to draw the base line stroke. Defaults to `true`. Set to `false` for symbol-only lines such as pebbles or blocks. |
| `stroke_style` | string | One of `solid`, `dashed`, `dotted`, `dash-dot`, `dash-dot-dot`. |
| `stroke_width` | number | Line stroke width. |
| `stroke_color` | color string | Line stroke color. |
| `dash_pattern` | number array | Custom dash pattern, for example `[8, 4]`. |
| `closed_fill` | string | Optional fill for closed lines before drawing the stroke. Use `background` to emulate Therion `thclean` behavior, or a color string such as `#FFFFFF`. Defaults to `none`. |
| `decorations` | array | Optional repeated or offset line decorations drawn along the line geometry. |

Example:

```json
{
  "stroke_style": "dashed",
  "stroke_width": 2.0,
  "stroke_color": "#607089",
  "dash_pattern": [8, 4]
}
```

Decoration entries support these `kind` values:

| `kind` | Meaning |
|---|---|
| `offset_stroke` | Draw one parallel stroke at `offset`. |
| `parallel` | Draw multiple parallel strokes from `offsets`. |
| `ticks` | Draw repeated short tick marks. |
| `rungs` | Draw repeated crossbars between `from_offset` and `to_offset`. |
| `teeth` | Draw Therion/MetaPost-like filled sawtooth teeth on one side, using the line path as each tooth base. |
| `symbols` | Draw repeated `symbol_parts` geometry. |
| `waves` | Draw repeated wavy marks along one side of the line. |
| `slope_ticks` | Draw `line slope` gradient ticks using per-line-point `orientation` and `l-size` values when present. |

Common decoration parameters:

| Parameter | Type | Meaning |
|---|---|---|
| `side` | string | `center`, `left`, or `right`. `left` means the Therion left side when following the line direction from its first point; the selected-line yellow side marker points to this side. `right` is the opposite side. Defaults to `center`; `teeth` default to `right`. |
| `spacing` | number | Distance between repeated marks. With `adjust_spacing: true`, this is the target MetaPost step before fitting it to the path length. |
| `adjust_spacing` | boolean | Fit repeated marks evenly to the line length using Therion/MetaPost-style `adjust_step` spacing. Applies to repeated decorations such as `symbols`, `ticks`, `rungs`, `teeth`, `waves`, and `slope_ticks`. |
| `spacing_divisor` | number | Divides the adjusted step after fitting. The bundled SKBB slope style uses `2.0`, matching `adjust_step(..., 1.4u) / 2`. Other repeated decorations usually keep the default `1.0`. |
| `offset` | number | Normal offset from the line. With `side: left/right`, positive values move to that side. |
| `offsets` | number array | Parallel offsets for `parallel`. |
| `from_offset`, `to_offset` | number | Crossbar endpoints for `rungs`. |
| `length`, `default_length` | number | Tick or rung length when explicit endpoints are not used; wave mark width for `waves`; fallback `l-size` for `slope_ticks`. |
| `alternate_length_scale` | number | Optional positive multiplier for every second `slope_ticks` mark, used for long-short-long-short slope symbols; defaults to `1.0`. |
| `size` | number | Tooth or symbol size; wave amplitude for `waves`. |
| `symbol_parts` | array | For `symbols`: repeated symbol geometry using the same `symbol_parts` syntax as point styles. |
| `stroke_style`, `stroke_width`, `stroke_color`, `dash_pattern` | mixed | Optional decoration-specific stroke styling. Defaults inherit the line stroke color where applicable. |
| `fill_color` | color string | Fill color for filled teeth or symbols. |
| `angle`, `angle_jitter`, `size_jitter`, `offset_jitter`, `distance_jitter`, `seed` | mixed | Optional deterministic variation for repeated symbols. `distance_jitter` moves repeated marks along the line by at most 45% of their spacing. |

For `slope_ticks`, the map renderer interpolates `l-size` between line points where it is defined. If a line point has explicit `orientation`, that direction is used; otherwise the tick is perpendicular to the local line direction on the left side. If no `l-size` is present, the decoration uses `default_length` or `length` as a fallback. The bundled slope style follows the SKBB MetaPost rhythm: `spacing` is fitted to the path with `adjust_spacing`, actual marks are placed at half that fitted step, `l-size` is the long mark length, and short marks are approximately one third of that length.

Bundled line styles include lightweight SKBB/UIS-derived presets for wall material subtypes, pit/floor-step/ceiling-step ticks, wall ice, overhang teeth, fixed ladder rungs, handrail markers, survey lines, and border subtypes. Wall material decorations that Therion's SKBB MetaPost places with `rotated -90` use `side: "right"` in the bundled styles. `line rock-border -close on` uses `closed_fill: "background"` to match Therion's UIS MetaPost behavior: a closed rock-border cleans/fills the boulder interior with the map background before drawing the outline. Symbols that depend on richer Therion semantics or complex MetaPost paths may still use an approximation or the generic fallback style.

Example symbol-only pebbles line:

```json
{
  "stroke_visible": false,
  "decorations": [
    {
      "kind": "symbols",
      "side": "center",
      "spacing": 10,
      "size": 8,
      "size_jitter": 2.5,
      "angle_jitter": 30,
      "stroke_color": "#1C1C1E",
      "fill_color": "#FFFFFF",
      "stroke_width": 1.2,
      "symbol_parts": [
        { "kind": "oval", "size": 8 }
      ]
    }
  ]
}
```

Example ladder line:

```json
{
  "stroke_visible": false,
  "decorations": [
    { "kind": "parallel", "offsets": [-5, 5], "stroke_width": 1.5 },
    { "kind": "rungs", "from_offset": -5, "to_offset": 5, "spacing": 10, "stroke_width": 1.2 }
  ]
}
```

#### Area Style Parameters

Area rendering also follows common Therion area options. `-place bottom`, `-place default`, and `-place top` choose whether the area fill and pattern are drawn on the bottom, normal, or top render layer. Default and bottom areas stay below normal line work; `-place top` intentionally draws above normal line work and should be used only when that overlap is desired. Edit handles and selection overlays remain above rendered geometry. `-clip on` is the default and clips area textures to the owning scrap wall boundary when that boundary can be resolved; `-clip off` keeps the area texture un-clipped, which is useful when joining scraps or intentionally extending textures across scrap borders.

Area style files support:

| Parameter | Type | Meaning |
|---|---|---|
| `stroke_style` | string | One of `solid`, `dashed`, `dotted`, `dash-dot`, `dash-dot-dot`. |
| `stroke_width` | number | Area boundary stroke width. |
| `stroke_color` | color string | Area boundary stroke color. |
| `dash_pattern` | number array | Custom boundary dash pattern. |
| `fill_color` | color string | Area fill color. |
| `fill_opacity` | number | Fill opacity from `0.0` to `1.0`. |
| `fill_pattern` | object | Optional pattern overlay. |

Supported `fill_pattern.kind` values are `hatch`, `cross_hatch`, and `dots`.

`hatch` and `cross_hatch` pattern parameters:

| Parameter | Type | Meaning |
|---|---|---|
| `spacing` | number | Distance between pattern lines. |
| `stroke_angle` | number | Pattern angle in degrees. |
| `stroke_width` | number | Pattern line width. |
| `stroke_style` | string | One of `solid`, `dashed`, `dotted`, `dash-dot`, `dash-dot-dot`. |
| `stroke_color` | color string | Pattern line color. |
| `dash_pattern` | number array | Optional custom dash pattern. |
| `angle_jitter` | number | Optional deterministic angle variation. |
| `offset_jitter` | number | Optional deterministic offset variation. |
| `seed` | integer | Optional deterministic pattern seed. |

`dots` pattern parameters:

| Parameter | Type | Meaning |
|---|---|---|
| `spacing` | number | Distance between grid centers, or scatter cell size when `placement` is `scatter`. |
| `placement` | string | Optional dot placement mode: `grid` (default) or `scatter` for a looser deterministic distribution inside spacing cells. |
| `symbol_parts` | array | Repeated symbol geometry using the same `symbol_parts` syntax as point styles. |
| `size` | number | Optional symbol motif bounding-box override. For `dots`, omit it in normal cases; the renderer derives it from the `symbol_parts` bounds. |
| `size_jitter` | number | Optional deterministic symbol size variation in the same units as `size`. |
| `angle_jitter` | number | Optional deterministic symbol rotation variation in degrees. |
| `offset_jitter` | number | Optional deterministic dot offset variation. If omitted for `placement: "scatter"`, the renderer uses most of each spacing cell. |
| `seed` | integer | Optional deterministic pattern seed. |

Example `dots` symbol pattern:

```json
{
  "fill_pattern": {
    "kind": "dots",
    "spacing": 13.0,
    "size_jitter": 1.2,
    "angle_jitter": 35.0,
    "symbol_parts": [
      { "kind": "asterisk", "size": 5.4, "stroke_color": "#8FB6D8" }
    ]
  }
}
```

Example hatch style:

```json
{
  "fill_color": "#2A7FFF",
  "fill_opacity": 0.2,
  "stroke_color": "#0D4EA3",
  "stroke_width": 1.4,
  "fill_pattern": {
    "kind": "hatch",
    "spacing": 12.0,
    "stroke_angle": 18,
    "stroke_width": 0.55,
    "stroke_style": "dashed",
    "dash_pattern": [5, 5],
    "stroke_color": "#0D4EA3"
  }
}
```

Invalid or missing style fields are ignored and the application falls back to the default styles.

## 10. Help And About

- Use `Help -> Quick User Manual` for a compact workflow summary.
- Use `Help -> User Manual (Full)` to open this manual from the application.
- Use `Help -> About Therion Studio` to view the installed application version, build label, Qt version, platform, GitHub repository, license, maintainer, and third-party notices link.

## 11. Troubleshooting

### 11.1 Therion executable not found

Symptom:

- error that the Therion executable path is missing or not executable

Fix:

- set a valid full path in `Compiler -> Executable`
- verify executable permissions

### 11.2 Config cannot be resolved

Symptom:

- compile target/config cannot be resolved

Fix:

- set `Target Config`, or
- open the desired `.thconfig` and run using `Current Config`, or
- verify working directory/override path

### 11.3 “Open Current Document in Map Editor” is disabled

Symptom:

- map-open command is unavailable

Fix:

- activate a `.th2` tab first

### 11.4 Rename/delete is blocked in project tree

Symptom:

- rename or delete action is refused

Fix:

- close tabs that reference the target file/folder, then retry
