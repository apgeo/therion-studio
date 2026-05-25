# Therion Studio

Therion Studio is a cross-platform Qt desktop editor for
[Therion](https://therion.speleo.sk/)  – cave surveying software.

## Motivation

Therion Studio aims to modernize the Therion GUI with a Qt-based, cross-platform application that
is easier to maintain and more comfortable on modern desktop systems, especially macOS.

Its development has been orchestrated with significant help from AI-assisted software engineering
tools.

## Features

- Multi-file Therion project editing with tabbed documents.
- Raw editor for `.th`, `.th2`, and `.thconfig` files.
- Syntax highlighting, auto-completion, and contextual help derived from the Therion Book.
- Synchronized Raw and Visual map editing for TH2 files.
- Visual TH2 map editor with source-linked points, lines, areas, scraps, and background images.
- Structured Block editor for more approachable `.th` and `.thconfig` authoring.
- Project structure and map-object navigation sidebars.
- Integrated Therion runner for current or project `.thconfig` compilation.
- Detached map window for multi-monitor workflows.
- Runtime light/dark appearance support.

## Status

The project is under active development. See
[`QtReimplementationSpecification.md`](QtReimplementationSpecification.md) for detailed
requirements.

## Installation

Release packaging is still being hardened.

- Windows installer notes: [`packaging/windows/README.md`](packaging/windows/README.md)
- Homebrew tap and formula: [`ladislavb/homebrew-therion-studio`](https://github.com/ladislavb/homebrew-therion-studio)
- Source build instructions: [`docs/BUILDING.md`](docs/BUILDING.md)

Release tags use CalVer, for example:

```text
v2026.5.0
```

## Notes

Therion Studio does not bundle the external Therion compiler. Install Therion separately and
configure it in the Compiler pane.

## License

Therion Studio is licensed under the GNU General Public License v3.0 or later
(`GPL-3.0-or-later`). See [`LICENSE`](LICENSE).

Third-party notices are in
[`docs/THIRD_PARTY_NOTICES.md`](docs/THIRD_PARTY_NOTICES.md).
