# Therion Studio Architecture

This document records the intended architecture of Therion Studio. It is a design guardrail, not a product specification and not a task backlog.

The product source of truth remains `QtReimplementationSpecification.md`. The active implementation roadmap remains `WORKLOG.md`. When architecture, implementation, and specification diverge, resolve the divergence explicitly instead of silently choosing one.

## Architectural Goals

- Preserve Therion source files losslessly unless a user explicitly applies a source edit.
- Keep Raw, Blocks, Map, Structure, Validation, completion, highlighting, help, and compiler workflows as projections of shared source data rather than independent reparsers.
- Keep UI shells thin and move parsing, indexing, file IO, process execution, source mutation, and reusable workflow rules into focused services or core types.
- Keep the UI responsive by making expensive parsing, indexing, rendering, filesystem traversal, and Therion execution asynchronous, cancellable, debounced, or revision-keyed.
- Keep validation and problem reporting centralized in the Validation surface. Structure remains an orientation and navigation view, not a second validator.
- Move project validation toward a live diagnostic projection that updates from source/project changes. Manual validation actions are transitional controls, not the long-term primary workflow.

## Dependency Direction

The intended dependency direction is:

1. Core/domain (`src/core/**`)
2. Platform/infrastructure adapters (`src/platform/**`, filesystem/session/process/catalog adapters)
3. Application workflow services and controllers (`src/app/**` non-widget services/controllers)
4. Presentation shells and widgets (`src/app/**` widgets, dialogs, QGraphicsItems, scene/view code)

Rules:

- Core/domain code shall not depend on QtWidgets, QGraphicsView, dialogs, windows, or UI event classes.
- Widgets and scene items may orchestrate interactions, but shall not own parsing, serialization, persistence, command-catalog rules, process execution, platform-path decisions, or source rewrite logic.
- Production infrastructure shall be composed at explicit boundaries such as `main.cpp`, `MainWindow` bootstrap code, or a future composition root.
- Tests should pass fakes or explicit test adapters for IO, settings, sessions, catalogs, and process execution.
- Static/global access is acceptable only for deterministic, stateless transformations with no hidden IO, mutable cache, settings, resource, or platform dependency.

## Source Model Direction

Therion Studio is migrating toward one shared, lossless source model for `.th`, `.th2`, and `thconfig` files.

The source model should preserve:

- physical source lines
- comments and blank lines
- indentation where practical
- newline style
- encoding metadata
- source file type
- token spans and source offsets
- logical command ranges across continuations
- block ranges and parent stacks
- unknown but valid Therion directives

Consumers should use shared source snapshots and logical command metadata where available:

- Raw editor: highlighting, completion, context help, validation tooltips
- Blocks editor: cards, nesting, details, data rows, insertion/move rules
- Map editor: TH2 object projection, references, geometry source ranges, inspector edits
- Structure: project orientation, ownership hierarchy, map/scrap relationships, non-hierarchical project graph links
- Validation: active-document and project-level diagnostics
- Compiler workflows: target config resolution and source-aware feedback

Do not solve projection drift by copying parser logic into UI renderers, inspectors, sidebars, scene items, or completion code. Extend core parsing/token/range/catalog metadata instead.

## UI Surface Ownership

The main project sidebars have distinct responsibilities:

- `Files`: filesystem/project file operations and file navigation.
- `Structure`: orientation and navigation for project objects and relationships. It may show structural relationship warnings that affect orientation, such as unresolved map/scrap composition references, but shall not become the general validation UI.
- `Validation`: all validation and problem identification, including command/catalog diagnostics, context/document-type diagnostics, parse/round-trip-sensitive findings, safe fixes, and project-wide problem scans.
- `Compiler`: Therion execution, target config selection, run output, and compiler-facing workflow state.

If a problem belongs in both navigation and validation, prefer showing it in Validation first. Add Structure surfacing only when it improves orientation without duplicating the validator workflow.

## Source Mutation and Transactions

Every user-visible source mutation should be one logical transaction with:

- a clear undo label
- revision/snapshot validation
- dirty-state update
- projection invalidation or refresh policy
- selection/cursor restoration policy
- focused undo/redo or round-trip coverage

TH2 map-editor source mutations shall route through the shared atomic map-source helper (`applySourceTextChangeWithSnapshot`) or an equivalent single-transaction abstraction that performs source replacement and undo snapshot creation together.

New Raw, Blocks, Map, inspector, sidebar, and project workflows that mutate source text should route through `TextEditorSourceTransactionController` or its successor shared source-change service. Temporary exceptions must be narrow, documented, and covered by focused tests.

## Validation Architecture

Validation should be conservative and catalog-backed.

- Active-document and project validation should use the same source metadata and catalog rules where practical.
- Project validation should become a live, background diagnostic projection over the open project graph and open in-memory documents. It should react to source edits, saves, file additions/removals, project-open events, and catalog/source-model updates through debounce, cancellation, and revision or generation IDs.
- A manual project validation button may remain during migration or as an explicit refresh/re-run action, but the target workflow is automatic problem discovery in the Validation panel when project problems appear or disappear.
- Unknown commands, unknown options, document-type diagnostics, and context diagnostics should remain warnings unless the rule is fully deterministic.
- Safe fixes must be explicit source edits with known ranges and must not silently skip undo snapshots or dirty-state handling.
- `thconfig`, `.th`, and `.th2` document-type applicability should come from command metadata and source-type detection, not UI heuristics.
- False-positive prevention should be backed by focused tests using real Therion patterns where possible.

## Project Index and Structure

`ProjectStructureIndex` owns lightweight project orientation data:

- root config/source inference
- survey, centerline, map, scrap, station, point, line, and area entries
- stable object IDs
- source file and line navigation
- map/scrap composition references
- non-hierarchical relationships such as preview, revise, join, and equate when represented as graph data

The Structure sidebar displays the ownership/navigation projection. It should not list general validation diagnostics. Validation-owned diagnostics should be surfaced in the Validation panel and may reference project-index objects when that improves navigation.

The Therion compiler remains authoritative for final export behavior. The project index is a lightweight navigation and early-feedback projection, not a compiler replacement.

## Catalog and Metadata

Therion command, option, help, style, and symbol metadata should be data-driven.

- Prefer parser/generator fixes over editing generated catalog output by hand.
- Use `resources/therion_catalog/overrides/*.override.json` only as a short-term fallback when parser extraction is not yet feasible.
- Remove overrides once generator support exists.
- Keep command catalog loading at composition, startup, or explicit test setup boundaries. Avoid UI-side static catalog access and long-lived resource overrides.

## UI Shell Reduction

`MainWindow`, `TextEditorTab`, and `MapEditorTab` are orchestration shells under active reduction.

When touching these classes:

- prefer extracting focused non-widget collaborators over adding private state and branches
- keep callbacks narrow and ownership-aware
- prefer named QObject signals/slots or typed context interfaces for semantic workflows
- avoid broad service-locator context structs
- keep QGraphicsItems presentation-focused: rendering, hit affordances, and local interaction state only

## Directory Contracts

The editor-mode layout is stable:

- `src/app/text_editor/` for raw editor shell and shared text-editor orchestration
- `src/app/text_editor/raw_editor/` for Raw-specific components (`RawEditor*`)
- `src/app/text_editor/block_editor/` for Blocks-specific components (`BlockEditor*`)
- `src/app/text_editor/map_editor/` for Map/Visual-specific components (`MapEditor*`)

Do not add new editor-mode implementation files back into legacy top-level `src/app/` paths when they belong to this layout.

Any intentional layout migration shall update:

- `CMakeLists.txt`
- structure guardrails/tests
- `AGENTS.md`
- `ARCHITECTURE.md`
- `WORKLOG.md`
- relevant specification/manual sections when behavior changes

## Verification Expectations

Architecture-sensitive changes should include focused verification:

- parser and source model changes: round-trip, source-range, continuation, comment/blank-line, and encoding-sensitive tests
- validation/catalog changes: false-positive and document-type/context tests
- source mutation changes: undo/redo, dirty-state, revision mismatch, and projection refresh tests
- project index changes: root config/source inference, in-memory document state, namespace/reference resolution, and stable object IDs
- UI workflow changes: relevant widget/controller tests or explicit manual verification when automation is not feasible

Before committing or proposing a PR, run `python3 scripts/check_structure_constraints.py` and fix violations in the same change.
