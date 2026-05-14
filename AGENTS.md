# Therion Studio Agent Instructions

## Scope

These instructions apply to the whole repository.

## Project State

- This repository is currently specification-first. The primary source of truth is [QtReimplementationSpecification.md](/Users/ladislav.blazek/Local/Code/personal/therion-studio/Therion%20Studio/QtReimplementationSpecification.md).
- Treat the specification as implementation-grade requirements for a Qt reimplementation of Therion Studio.
- Do not invent product behavior that conflicts with the specification. If a requested change requires behavior not covered by the specification, update the specification as part of the same change or clearly flag the gap.
- If implementation, tests, and the specification diverge, prefer bringing them back into alignment explicitly rather than silently choosing one source of truth.

## Specification Editing Rules

- Preserve the document's SRS style and section structure unless the change explicitly requires restructuring.
- Use normative language consistently:
  - "shall" for mandatory behavior
  - "should" for strong recommendations
  - "may" for optional behavior
- Keep requirements testable. New functional requirements should be specific enough to verify.
- Keep implementation notes and guidance separate from normative requirements.
- Preserve explicit MVP and Post-MVP distinctions where relevant.
- Prefer additive edits over broad rewrites. Do not weaken or broaden requirements casually.
- When adding a new behavior, update the most relevant acceptance criteria section if verification expectations change.

## Implementation Expectations

- Default technology assumptions should match the specification: Qt 6, cross-platform desktop, shared core logic across macOS, Windows, and Linux.
- Follow established best practices and relevant platform standards for modern Qt and C++ desktop application development unless the specification explicitly requires a different approach.
- Preserve separation of concerns:
  - domain model, parsing, serialization, and editing rules stay outside UI classes
  - widgets, views, and scene items should not own file I/O or document parsing
  - services own external interactions such as file access, process execution, and metadata loading
- Favor straightforward, testable designs over speculative abstractions.
- Keep files and translation units focused and reasonably sized. There is no hard line limit, but once a file starts mixing multiple responsibilities or becomes difficult to scan in one pass, split it by responsibility. Try to avoid files with >1000 lines if possible.
- Prefer splitting large features into focused types or files such as model, parser, serializer, widget, controller, scene item, or service components rather than accumulating unrelated behavior in one class.
- Avoid generic catch-all modules or class names such as Helpers, Utils, Misc, or Manager unless the code is genuinely cohesive.
- Do not introduce dependencies outside Qt and the standard library without explicit justification.

## Source File Splitting

- Treat a translation unit as too large once it mixes unrelated responsibilities or becomes hard to scan in one pass, even if it still compiles cleanly.
- Split by responsibility, not by line count: keep orchestration shells thin and move parsing, indexing, persistence, data transformation, and reusable UI behavior into focused companion types or files.
- Prefer extracting pure logic first, then moving stateful coordination, then narrowing the UI shell to event wiring and presentation.
- Keep public interfaces small and explicit; if a new split requires cross-file knowledge, add a narrow API rather than sharing internal state.
- When extracting behavior that affects user-visible workflows, add or update focused tests for the extracted logic before treating the split as complete.
- Preserve the spec-first boundary: if a split changes ownership or behavior, confirm the specification still describes the result or update it in the same change.

## Data Integrity Rules

- Round-trip safety is a core requirement. Changes must not cause semantic loss in supported Therion documents.
- Preserve comments, unknown but valid directives, formatting where practical, and existing encodings unless the user explicitly requests conversion.
- Treat parsing, serialization, indexing, and map/text synchronization changes as high-risk areas that require careful verification.
- Any change that touches Therion parsing, serialization, rewrite behavior, or source-to-model synchronization should include explicit round-trip or behavioral verification and should call out remaining semantic-risk areas.
- Prefer structured metadata over runtime heuristics when behavior depends on documented rules. In particular, style catalogs, highlight palettes, and help metadata should be data-driven rather than hardcoded ad hoc.

## UI and Behavior Rules

- Preserve documented user workflows before pursuing architectural or visual cleanup.
- Keep the UI responsive. Long-running work such as Therion execution, parsing, indexing, or asset loading must not block the UI thread.
- Respect platform conventions for shortcuts, menus, dialogs, and file handling, but do not violate documented functional requirements.
- Treat keyboard shortcuts as cross-platform requirements: any shortcut-driven workflow shall be usable on macOS, Windows, and Linux, including a non-`Insert` fallback for actions that rely on keys commonly missing on Mac keyboards.
- Native platform behavior should win when it does not conflict with the specification.
- Prefer actionable user-facing errors and safe recovery paths. Do not swallow failures in parsing, file I/O, asset loading, or process execution when the user needs to understand or resolve the problem.

## Verification Expectations

- Prefer focused automated tests for parser, serializer, project loading, indexing, command routing, session restore, and document-editing logic.
- If a change affects externally visible behavior, verify it against the acceptance criteria in the specification.
- If automated verification is not possible, document a concrete manual verification pass with specific user workflows or scenarios.
- Do not claim behavior is implemented if the specification, tests, or verification steps do not support that claim.

## Change Discipline

- Keep changes tightly scoped to the request.
- Update documentation when behavior, architecture, packaging, or verification expectations change.
- Maintain a living [docs/USER_MANUAL.md](docs/USER_MANUAL.md) and update it whenever UI layout, user workflows, keyboard shortcuts, or settings behavior changes; if the file does not exist yet, create it as part of the first such change.
- Surface ambiguities instead of silently choosing a product behavior that could ripple through the spec.
- When code is added later, prefer file and module names that reflect responsibility clearly.
- Document progress in the repository itself, not only in chat. Update [WORKLOG.md](WORKLOG.md) after every change so the current implementation state stays visible in the repo.
- Keep [WORKLOG.md](WORKLOG.md) structured with clear `In progress`, `Next up`, `Risks / blockers` and `Completed` sections when practical.
- In [WORKLOG.md](WORKLOG.md), cross out completed items using Markdown strikethrough (for example, `~~completed item~~`) instead of deleting them.
