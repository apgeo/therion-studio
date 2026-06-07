# Codex Repository Review

Date: 2026-06-02

Scope: static review of repository structure, dead-code candidates, duplicate code, maintainability, performance, battery impact, and the parser/write architecture for `.th`, `.th2`, and `thconfig` documents.

## Executive Summary

Therion Studio is close to a coherent architecture, but the biggest remaining maintenance risk is that the application still does not have one authoritative source parser/model for Therion documents. `TherionDocumentParser` currently behaves mostly as a line tokenizer. Raw editor, Block editor, Map editor, Structure index, syntax highlighting, command help, background metadata, and source rewrite operations then reinterpret the same text independently.

The desired direction is correct: parse `.th`, `.th2`, and `thconfig` into one shared, lossless source model; interpret that model as Raw, Blocks, Map, Structure, and command help projections; and write back to plain text through one atomic source-change transaction that also owns snapshot/undo/redo.

For the first stable public release, I would not attempt a full parser rewrite. I would add guardrails and keep the existing behavior stable. The unified parser/source transaction migration should be planned as the next major architecture phase because it touches every high-risk area: round-trip safety, map/text synchronization, encoding, undo/redo, and performance.

## Status Update - 2026-06-04

Completed follow-ups from this review:

- ~~Low-level whitespace tokenization and normalized line splitting duplicates have been consolidated into `src/core/TherionStringUtils.h`.~~
- ~~Map editor inspector/object source mutations now require `applySourceTextChangeWithSnapshot`; the previous silent `replaceTextForCommand(...)` fallback path is gone.~~
- ~~`applySourceTextChangeWithSnapshot` is now backed by the shared `TextEditorSourceTransactionController`, with focused tests in `TextEditorSourceTransactionControllerTest`.~~
- ~~`StructureConstraintsTest` now guards low-level map-editor source mutations against unapproved `replaceTextForCommand(...)` / rewrite bypasses.~~
- ~~`TherionTokenRules` now centralizes numeric-token classification and option-boundary checks used by parser, source rewriting, structure indexing, syntax highlighting, command option parsing, and map projections/details.~~
- ~~`TherionCommandLineModel` now moves the existing catalog-backed command option parsing model into `src/core/`, and Map/Block option UI call sites use it directly.~~
- ~~`TherionCommandLineModel` now owns shared command argument and option-row serialization helpers used by option validation, Blocks option argument editing, and Map/Block command options dialog flows.~~
- ~~`TherionCommandLineModel` now owns shared command option lookup helpers for normalized option names, option values, value maps, and toggle values; Map inspector, map rendering/projections, map delete/split/reference planners, PocketTopo background placement, and Structure indexing use these helpers instead of local option-value parsers.~~
- ~~Raster background source images now use a bounded path/mtime/size cache so repeated gamma and placement operations do not decode the same raster file repeatedly.~~
- ~~Raster background gamma correction and scaled image preparation now run through `QtConcurrent` with request-id checks before applying the pixmap back on the UI thread.~~
- ~~Manual raster background adds now decode initial source images through `QtConcurrent`.~~
- ~~Metadata/session raster background auto-load now creates deterministic placeholder scene layers synchronously and decodes source images through `QtConcurrent` before applying scaled/gamma-adjusted pixmaps back on the UI thread.~~
- ~~Background raster decode jobs now use document/layer generation guardrails so stale manual-add, auto-load, and gamma results are ignored after layer removal or scene reload; failed async auto-loads report a toolbar status instead of failing silently.~~
- ~~Raster source image cache, image loading, raster size probing, and gamma/scale worker logic have been extracted from `MapEditorBackgroundLayers.cpp` into focused `MapEditorRasterBackgroundImage` helpers.~~
- ~~Raster model/preview projection and placement helpers have been extracted from `MapEditorBackgroundLayers.cpp` into focused `MapEditorRasterBackgroundPlacement` helpers.~~
- ~~Gamma-adjusted scaled raster `QImage` results now use a bounded worker-safe cache keyed by source path/mtime/size, target size, and gamma.~~
- ~~Focused `TherionDocumentParserTest` coverage now locks quoted strings, comment spans, comment-only line behavior, CRLF stripping, and the current `parseText()` token-line-only behavior before the later lossless source model work.~~
- ~~`BlockEditorApplyExecutorTest` now verifies block-details auto-commit produces an undoable text edit and redo restores the edited source.~~
- ~~`MapEditorCanvasEditSourceTransactionTest` now verifies map source snapshot/apply helpers create one undo snapshot, flush pending scene refresh, and surface undo/redo toolbar status through the shared transaction controller.~~

Still open:

- The shared source transaction controller is not yet the single source-write path for Raw, Blocks, Map, and all inspector workflows.
- Command-line modeling and higher-level option parsing/serialization are still duplicated across command option parsing, Block editor details, Map object settings, and source rewriters.
- The unified lossless parser/source document model remains post-release architecture work.
- Large map/background/rewriter files are still large and should be split only when a focused behavior change gives a natural extraction point.

Recommended next step:

- Next, return to release stabilization with the full local test pass and manual smoke checks, or start the later lossless parser/source-document phase once release stabilization allows broader parser work.

## Priority Findings

### P0 - Missing Unified Lossless Parser

Evidence:

- `src/core/TherionDocumentParser.cpp` tokenizes individual lines and returns `TherionParsedLine`.
- `TherionDocumentParser::parseText()` skips tokenless lines, so blank lines and comment-only lines are not represented in the parsed model.
- `src/core/TherionDocumentEditor.cpp`, `src/core/ProjectStructureIndex.cpp`, `src/app/text_editor/map_editor/MapEditorSceneRenderer.cpp`, `MapEditorObjectDetailsLogic.cpp`, `MapEditorSourceReferenceResolver.cpp`, and `MapEditorSceneItems.cpp` each contain their own interpretation helpers for tokens, numeric values, line points, options, and object structure.

Risk:

- The current parsed representation is not a safe canonical model for round-trip editing because it does not preserve all source trivia.
- Map, Block, Structure, completion, and syntax validation can drift because each layer reinterprets the source.
- Fixes tend to land in UI or generated JSON instead of parser/generator code.

Recommended direction:

- Introduce a shared `TherionSourceDocument` model that preserves:
  - every physical source line,
  - original newline style,
  - indentation,
  - comments and comment-only lines,
  - blank lines,
  - token spans,
  - command/directive spans,
  - block ranges,
  - source file type context (`th`, `th2`, `thconfig`),
  - encoding metadata.
- Keep raw text as the canonical persisted output.
- Build projections from this model:
  - Raw editor projection,
  - Block editor projection,
  - Map geometry projection,
  - Project/Structure projection,
  - Context Help/completion/syntax projection.

### P0 - Source Writes Are Not Yet One Cross-Editor Atomic Transaction

Evidence:

- Map editor has `applySourceTextChangeWithSnapshot()` in `src/app/text_editor/map_editor/MapEditorCanvasEditController.cpp` and routes many map writes through it.
- General source rewrites still flow through `TextEditorSourceRewriteController` and `TherionDocumentEditor` using methods such as `replaceTextForCommand()`, `replaceTextForCommandWithUndo()`, and many `rewrite*()` methods.
- Block editor auto-commit is still represented by `BlockEditorApplyExecutor`, even though the UI no longer exposes an `Apply` workflow.
- The shared transaction abstraction is still map-editor local rather than cross-editor.

Current follow-up status:

- ~~The ad hoc `MapEditorObjectDetailsEditController` / `MapEditorInspectorObjectController` fallback path that silently used `replaceTextForCommand(...)` when `applySourceTextChangeWithSnapshot` was missing has been removed. Map inspector source mutations now require the atomic map-source transaction callback.~~
- ~~The map helper is now backed by `TextEditorSourceTransactionController`, which owns the text replacement plus undo snapshot transaction for these map-editor flows.~~
- The remaining architecture gap is that `TextEditorSourceTransactionController` is not yet the single required source-edit service for Raw, Blocks, Map, and all inspector workflows.

Risk:

- Undo/redo behavior remains dependent on which editor path initiated the write.
- Snapshot creation can regress when a new map/block operation bypasses the map helper.
- The same logical edit can have different cursor, selection, refresh, and undo semantics depending on the caller.
- The removed fallback path showed how easily non-undoable source writes can reappear when mutation code is not forced through one transaction service.

Recommended direction:

- ~~Promote the map helper into a shared source-edit service, for example `TextEditorSourceTransactionController` or `TherionSourceChangeService`.~~
- ~~Keep the source transaction callback/interface required for map object edit controllers; do not reintroduce an ad hoc `replaceTextForCommand(...)` fallback in map-editing code.~~
- Continue expanding `TextEditorSourceTransactionController` until Raw, Blocks, Map, and inspector source mutations submit through one required source-edit path.
- Every source mutation should submit a single `SourceChangeRequest`:
  - before text/version,
  - after text or line-range patch,
  - undo label,
  - target source range/line for selection restoration,
  - post-change projection refresh policy.
- The service should perform:
  - document revision check,
  - text replacement,
  - undo snapshot creation,
  - dirty-state update,
  - parsed-document cache invalidation,
  - selection/cursor restoration.

### P1 - Large Multi-Responsibility Files

Largest source files:

- `src/app/text_editor/map_editor/MapEditorSceneRenderer.cpp` - 2963 lines
- `src/core/TherionDocumentEditor.cpp` - 2484 lines
- `src/app/text_editor/map_editor/MapEditorBackgroundLayers.cpp` - 2128 lines
- `src/app/MainWindow.cpp` - 1778 lines
- `src/app/text_editor/map_editor/MapEditorCanvasEditController.cpp` - 1368 lines
- `src/app/text_editor/map_editor/MapEditorSceneItems.cpp` - 1274 lines
- `src/core/ProjectStructureIndex.cpp` - 1247 lines
- `src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.cpp` - 1170 lines
- `src/app/text_editor/map_editor/MapEditorSceneInternals.h` - 1074 lines

Risk:

- These files mix parsing, rendering, state coordination, and source-write behavior.
- Changes in these files are harder to review and easier to regress.
- They increase the chance of UI-layer business rules.

Recommended splits:

- `TherionDocumentEditor.cpp`
  - `TherionSourceLineModel`
  - `TherionSourceTokenizer`
  - `TherionSourceRange`
  - `TherionObjectOptionRewriter`
  - `TherionTh2GeometryRewriter`
  - `TherionBlockRewriter`
- `MapEditorSceneRenderer.cpp`
  - map geometry extraction,
  - point rendering,
  - line rendering,
  - area rendering,
  - labels,
  - editable guide overlays,
  - station label LOD.
- `MapEditorBackgroundLayers.cpp`
  - metadata parser/writer,
  - image cache,
  - image placement,
  - inspector UI controller,
  - source rewrite adapter.
- `ProjectStructureIndex.cpp`
  - config resolver,
  - input graph scanner,
  - namespace resolver,
  - structure tree builder,
  - diagnostics.

### P1 - Duplicate Token and Option Logic

Examples:

- ~~Numeric-token checks existed in multiple places:~~
  - ~~`TherionDocumentParser.cpp::isNumericToken`~~
  - ~~`TherionDocumentEditor.cpp::tokenLooksNumeric`~~
  - ~~`ProjectStructureIndex.cpp::tokenLooksNumeric`~~
  - ~~`MapEditorObjectDetailsLogic.cpp::tokenLooksNumericForMapDetails`~~
  - ~~`MapEditorSceneItems.cpp::tokenLooksNumeric`~~
  - ~~`MapEditorSourceReferenceResolver.cpp::tokenLooksNumeric`~~
  - ~~`MapEditorSceneRenderer.cpp::tokenLooksNumeric`~~
- Line-ending and source-line splitting are duplicated across:
  - `TherionDocumentEditor.cpp`
  - `MapEditorBackgroundLayers.cpp`
  - `MapEditorObjectMovePlanner.cpp`
  - `MapEditorObjectDeletePlanner.cpp`
  - `MapEditorLineSplitPlanner.cpp`
- ~~`tokenizeWhitespace()` was duplicated in `TherionXviParser.cpp` and `TherionBackgroundMetadata.cpp`.~~
- ~~`splitLinesNormalized()` was duplicated in `TherionDocumentEditor.cpp` and `MapEditorLineSplitPlanner.cpp`.~~
- ~~Command option parsing existed in app-level `CommandOptionParser`.~~
- Command option serialization/editing behavior still exists across Block editor details, Map object settings, and source rewriters.

Current follow-up status:

- The low-level whitespace tokenization and line-splitting duplicates above have been consolidated into `src/core/TherionStringUtils.h` in the current cleanup branch.
- Numeric-token classification and option-boundary checks have been consolidated into `src/core/TherionTokenRules.h`.
- The first shared command-line model has been extracted into `src/core/TherionCommandLineModel.h`; the legacy app-level `CommandOptionParser` wrapper has been removed.
- Command-line option parsing plus option-row/value serialization now lives behind focused command-line model APIs rather than broader generic utilities.
- Command option lookup, normalized field-name matching, value maps, and toggle parsing now live behind `TherionCommandLineModel` and are used by the map inspector, map renderer/projection extraction, map delete/split/reference planners, PocketTopo background placement, and Structure indexing.

Risk:

- Semantic drift. For example, numeric parsing and option-boundary detection may behave differently in renderer, structure index, and writer.
- Harder bug fixing: a syntax or option parsing fix may require edits in several locations.

Recommended direction:

- ~~Centralize token classification in `TherionTokenRules`.~~
- Centralize line splitting/newline preservation in `TherionSourceText`.
- ~~Extract low-level text helpers such as whitespace tokenization and normalized line splitting into a focused core utility such as `TherionStringUtils`, not broad generic `Utils`.~~
- ~~Centralize command option parsing in a shared catalog-backed `TherionCommandLineModel`.~~
- ~~Continue centralizing option serialization/editing helpers in the same command-line model.~~
- Make renderers and inspectors consume already-parsed command/option data instead of re-tokenizing.

## Parser Architecture Recommendation

### Proposed Model

Introduce these layers incrementally:

1. `TherionSourceText`
   - lossless physical source lines,
   - newline preservation,
   - line/byte offsets,
   - safe line-range replacement.

2. `TherionSourceParser`
   - tokenizes every line,
   - preserves empty/comment-only lines,
   - annotates command lines,
   - recognizes block starts/ends,
   - attaches catalog metadata where available.

3. `TherionSourceDocument`
   - immutable parsed snapshot,
   - document revision,
   - file type,
   - encoding,
   - diagnostics.

4. Projections:
   - `TherionBlockProjection`
   - `TherionMapProjection`
   - `TherionStructureProjection`
   - `TherionSyntaxProjection`

5. `TherionSourceChangeService`
   - line-range rewrite,
   - text replacement,
   - undo snapshot,
   - parsed cache invalidation,
   - selection restoration.

### Migration Path

Do not rewrite everything at once. Use this order:

1. Extract shared `TherionSourceText` for line splitting/newline preservation.
2. ~~Replace duplicate numeric/token helpers with one shared `TherionTokenRules`.~~
3. Make `TherionDocumentParser::parseText()` optionally return all physical lines, not only token lines.
4. Move geometry extraction from `MapEditorSceneRenderer.cpp` into a standalone `TherionMapProjection` or `Th2GeometryProjection`.
5. Move project structure parsing to consume the same source document/projection.
6. Move all editor writes to a shared transaction service.
7. Delete or rename legacy wrappers once all call sites use the new service.

## Dead Code and Legacy Candidates

No obviously safe-to-delete large subsystem was confirmed by static inspection alone. The following are candidates for cleanup after targeted tests:

### `BlockEditorConfigureController`

Evidence:

- `src/app/text_editor/block_editor/BlockEditorConfigureController.cpp` now only acts on `kind == "data"`.
- `showCommandHelpOnly` is passed through but explicitly unused.
- The generic name suggests a broad configure workflow, but current behavior is "open data rows dialog".

Recommendation:

- Rename/scope it to `BlockEditorDataRowsController` or fold it into the specific data-row action path.
- Remove the unused `showCommandHelpOnly` parameter unless a real caller needs it.

### Block Apply Naming

Evidence:

- `BlockEditorApplyExecutor` and `BlockEditorApplyStateController` still exist.
- The UI no longer has an explicit Apply button, but auto-commit paths still call `applyBlockDetailsChanges()`.

Recommendation:

- This is probably not dead code, but the naming is stale.
- Rename toward the current behavior, for example `BlockEditorSelectionAutoCommitController`.

### MainWindow Top-Level Files

Evidence:

- `src/app/` still has many `MainWindow*` and `TherionRunner*` files at top level.
- Some are proper shell/orchestration components, but this area is still large and active.

Recommendation:

- Do not move files casually.
- If more MainWindow work is planned, extract by workflow into stable subdirectories only if it reduces responsibility mixing and updates structure guardrails.

## Duplicate Code and Consolidation Opportunities

### Inspector Infrastructure

Recent work moved toward shared `InspectorPanel`, `DocumentInspectorPanel`, `DocumentFileInspector`, and `ContextHelpInspector`. Keep going in the same direction:

- One File inspector implementation for Raw, Block, and Map.
- One Context Help implementation for Raw, Block toolbox entries, Block selected blocks, and Map command context.
- One tab/pane layout implementation.
- Different content widgets are acceptable; duplicated panel styling/layout is not.

### Command Option Editing

The restored Map `Edit Object Settings...` uses `CommandOptionsDialog`, which is the right direction. The same catalog-backed option component should become the shared option editor for:

- Map object settings,
- Block selected command options,
- any future command option dialog.

Avoid a second option parser or custom option table in Map editor.

### Style Metadata vs Renderer Heuristics

Map style behavior should remain data-driven:

- guide spine visibility,
- editable line affordances,
- station label LOD,
- decoration visibility,
- dark/light rendering choices.

If a style needs editing-only guides, put the behavior in style metadata or a dedicated editor overlay policy, not hardcoded per type in renderer code.

## Performance and Battery Findings

### Full Document Parse and Full Scene Refresh

Evidence:

- `MapEditorSceneRefreshController` clears the scene, parses current document lines, collects scene entries, collects geometry features, and rerenders.
- `MapEditorTabSourceEditWorkflow` caches parsed lines by document revision, which is good, but geometry collection and scene rebuilding still happen repeatedly.
- `MapEditorBackgroundLayers` also parses text and collects geometry bounds for background behavior.

Risk:

- Large `.th2` files can cause UI-thread work on every scene refresh.
- Full scene rebuilds are battery-expensive on laptops, especially with raster backgrounds and labels.

Recommendations:

- Keep parsed document snapshots cached by document revision.
- Cache derived projections separately:
  - map geometry,
  - structure tree,
  - background metadata,
  - source bounds.
- Debounce text-change driven map/structure refreshes.
- Avoid clearing/rebuilding the entire scene when a single object/line changed.
- Move expensive projection building to a worker where possible, then apply UI deltas on the main thread.

### Raster Background Scaling

Evidence:

- `MapEditorBackgroundLayers.cpp` reads images with `QImageReader`, scales images with `Qt::SmoothTransformation`, and creates pixmaps.
- The image reads currently happen synchronously in UI-facing paths. This includes viewport/layer update paths and user-initiated layer load/reload paths.

Risk:

- Repeated smooth scaling of large raster images is CPU/battery expensive.
- Appearance changes, gamma/opacity changes, viewport changes, or layer reordering can accidentally trigger image recomputation.
- Large cave-survey raster backgrounds can freeze the UI while `QImageReader::read()` decodes them on the main thread.

Recommendations:

- ~~Move manual image-add decoding to a background job, for example `QtConcurrent::run()` with completion delivered back to the UI thread.~~
- ~~Move metadata/session image auto-load decoding to a background job without breaking load-time layer availability.~~
- ~~Show an existing image or placeholder while a new image decode is pending.~~
- ~~Cache original decoded `QImage` per file path/mtime.~~
- ~~Move gamma-adjusted scaled image preparation off the UI thread with stale-result checks.~~
- ~~Ignore stale async decode/gamma results after layer removal or scene reload, and surface async raster load failures in the toolbar status.~~
- ~~Extract raster source cache/loading/size-probing/gamma worker helpers from the large background-layer UI file.~~
- ~~Extract raster placement/projection helpers from the large background-layer UI file.~~
- ~~Cache adjusted/scaled raster images by file identity, source mtime/size, target size, and gamma.~~
- Do not recompute background images during unrelated theme/appearance updates.
- Keep layer order in model state and render from that stable model.

### Deferred Retry Timers

Evidence:

- `MapEditorCanvasEditController` uses multiple delayed `QTimer::singleShot(...)` attempts after some source edits to restore scene selection once the scene has refreshed.

Risk:

- The retry sequence is timing-dependent and does repeated work whether or not the scene is already ready.
- It can still fail on slow machines or under heavy load if the scene refresh completes after the final retry.
- Repeated speculative retries are not severe, but they are unnecessary CPU wakeups and add battery cost.

Recommendation:

- Replace retry timers with an explicit scene refresh completion signal or callback.
- Selection restoration should run once after the relevant scene generation is available, not through fixed-delay polling.

### Async Usage Assessment

| Concern | Current assessment | Recommendation |
|---|---|---|
| Therion process execution | Async through `QProcess` signals | Keep |
| Project structure scanning | Async/cancellable pattern exists | Keep and extend cache invalidation |
| Background image loading | Synchronous in UI-facing paths | Move decode/scale preparation off the UI thread |
| Map scene rebuild | Synchronous but debounced | Accept pre-release; plan incremental/projection-based refresh |
| File I/O | Isolated behind services | Accept for current file sizes |
| Filesystem watching/polling | No tight polling found | Keep |

### Icons and Small Pixmaps

Evidence:

- Lucide pixmap rendering appears in `MainWindowStructureBrowser`, `MainWindowSidebar`, `BlockEditorCanvasItem`, and `MapEditorInspectorData`.
- Some cache exists, but rendering factories are duplicated.

Recommendation:

- Keep one icon factory/cache service for Lucide pixmaps and action icons.
- Cache by icon name, color, size, DPR, and enabled/disabled state.

### Structure Index

Evidence:

- `ProjectStructureIndex.cpp` parses inputs and builds a snapshot with config resolution, input graph, map/scrap references, and diagnostics.

Risk:

- Project-wide indexing can become expensive on large projects.

Recommendations:

- Keep indexing asynchronous/cancellable.
- Cache per-file parsed source snapshots by file path, mtime, and size.
- Rebuild only affected graph branches after a file save.
- Avoid running structure refresh on cursor/source-line movement when source content did not change.

## Maintainability Recommendations

### Recommended Next Architecture Phase

Create a dedicated phase in `WORKLOG.md` for "Unified Source Parser and Transaction Model":

1. Lossless source text model.
2. Shared token rules.
3. Shared command line parser.
4. Shared projections for Raw/Block/Map/Structure.
5. Shared source transaction/undo service.
6. Deletion/rename of legacy configure/apply wrappers.

### Release-Safe Improvements Before Public Release

These are lower-risk and useful before release:

- ~~Keep map-object inspector source transactions mandatory and avoid reintroducing non-undo fallback writes.~~
- ~~Keep duplicated low-level source string helpers consolidated in a focused shared core helper.~~
- ~~Add tests for `applySourceTextChangeWithSnapshot` equivalent behavior for all map source operations.~~
- ~~Add tests that Block editor auto-commit creates undoable text changes.~~
- Add regression tests for parser/token rules:
  - negative numbers,
  - exponent notation,
  - quoted strings,
  - comments,
  - comment-only lines,
  - line-point option rows,
  - `revise ... -stations ...`.
- Add a smoke benchmark for opening a large `.th2` file and toggling dark/light mode.

### Post-Release Improvements

These should wait until after the first public release:

- Full unified parser model.
- Incremental map scene updates.
- Project graph visualization.
- Background image cache rewrite.
- Replace map selection-restore retry timers with scene-refresh completion events.
- Large-file async parser/projection pipeline.

## Suggested Verification Strategy

For each parser/source-write migration step:

- Unit-test round-trip behavior with real sample `.th`, `.th2`, and `thconfig` files.
- Assert that blank lines, comments, line endings, and encoding are preserved.
- Assert one undo item per user operation.
- Assert redo restores the same source text.
- Test Map, Block, and Raw projections from the same parsed snapshot.
- Run structure index tests against projects with:
  - root `thconfig`,
  - `thconfig.work`,
  - multiple root configs,
  - nested `input`,
  - nested survey namespaces,
  - map/scrap references.

## Final Recommendation

For release, keep the current implementation stable and avoid major parser rewrites. The biggest actionable pre-release work is to add guardrail tests around source writes and known parser edge cases.

For the next major development phase, make the unified lossless parser and shared source transaction service the central architecture objective. This will reduce dead/duplicated code, improve undo/redo consistency, make Map/Block/Raw views coherent, and provide the best leverage for performance and battery improvements.

Immediate next step: continue the release-safe performance track by evaluating adjusted/scaled pixmap caching. Keep the full lossless parser/source-document model as the later architecture phase.
