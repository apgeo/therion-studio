#!/usr/bin/env python3
"""Fail when key large translation units grow beyond ratcheted thresholds."""

from __future__ import annotations

import pathlib
import sys


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent


# Ratchet baseline: keep current large files from growing while extraction work continues.
LINE_LIMITS = {
    "src/app/text_editor/TextEditorTab.cpp": 1087,
    "src/app/text_editor/raw_editor/RawEditorCompletionController.cpp": 267,
    "src/app/text_editor/map_editor/MapEditorTab.cpp": 860,
    "src/app/MainWindow.cpp": 2203,
}


# Stable editor-mode layout contract: required paths must exist and legacy paths
# from pre-reorganization layout must stay absent.
REQUIRED_PATHS = (
    "src/app/text_editor/TextEditorTab.cpp",
    "src/app/text_editor/TextEditorTab.h",
    "src/app/text_editor/raw_editor",
    "src/app/text_editor/block_editor",
    "src/app/text_editor/map_editor",
    "src/app/text_editor/map_editor/MapEditorTab.cpp",
    "src/app/text_editor/map_editor/MapEditorTab.h",
)

FORBIDDEN_PATHS = (
    "src/app/TextEditorTab.cpp",
    "src/app/TextEditorTab.h",
    "src/app/MapEditorTab.cpp",
    "src/app/MapEditorTab.h",
    "src/app/MapEditorBackgroundLayers.cpp",
    "src/app/MapEditorInputPolicy.cpp",
    "src/app/MapEditorInputPolicy.h",
    "src/app/MapEditorSceneInternals.h",
    "src/app/MapEditorSceneItems.cpp",
    "src/app/MapEditorSceneRenderer.cpp",
    "src/app/MapEditorSceneSupport.h",
    "src/app/MapEditorTabWorkspace.cpp",
)

ALLOWED_BLOCK_EDITOR_TEXT_EDITOR_INCLUDES = {
    "src/app/text_editor/block_editor/BlockEditorPanelBuilder.cpp",
}

TEXT_EDITOR_BLOCK_EDITOR_FORBIDDEN_PATTERNS = (
    ("src/app/text_editor/TextEditorTab.h", "friend class BlockEditor", "TextEditorTab shall not expose private state to BlockEditor friends"),
)

BLOCK_EDITOR_FORBIDDEN_PATTERNS = (
    ("TextEditorTab *owner", "BlockEditor classes shall use explicit context objects instead of TextEditorTab owner pointers"),
    ("TextEditorTab* owner", "BlockEditor classes shall use explicit context objects instead of TextEditorTab owner pointers"),
    ("TextEditorTab *owner_", "BlockEditor classes shall use explicit context objects instead of TextEditorTab owner pointers"),
    ("TextEditorTab* owner_", "BlockEditor classes shall use explicit context objects instead of TextEditorTab owner pointers"),
)


def count_lines(path: pathlib.Path) -> int:
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        return sum(1 for _ in handle)


def contains_text(path: pathlib.Path, text: str) -> bool:
    return text in path.read_text(encoding="utf-8", errors="replace")


def main() -> int:
    violations: list[tuple[str, int, int]] = []
    checked: list[tuple[str, int, int]] = []
    layout_violations: list[str] = []
    dependency_violations: list[str] = []

    for relative_path, limit in LINE_LIMITS.items():
        absolute_path = REPO_ROOT / relative_path
        if not absolute_path.exists():
            violations.append((relative_path, -1, limit))
            continue

        actual = count_lines(absolute_path)
        checked.append((relative_path, actual, limit))
        if actual > limit:
            violations.append((relative_path, actual, limit))

    for relative_path in REQUIRED_PATHS:
        absolute_path = REPO_ROOT / relative_path
        if not absolute_path.exists():
            layout_violations.append(f"missing required path: {relative_path}")

    for relative_path in FORBIDDEN_PATHS:
        absolute_path = REPO_ROOT / relative_path
        if absolute_path.exists():
            layout_violations.append(f"legacy path must stay absent: {relative_path}")

    for relative_path, pattern, message in TEXT_EDITOR_BLOCK_EDITOR_FORBIDDEN_PATTERNS:
        absolute_path = REPO_ROOT / relative_path
        if absolute_path.exists() and contains_text(absolute_path, pattern):
            dependency_violations.append(f"{relative_path}: forbidden `{pattern}` ({message})")

    block_editor_root = REPO_ROOT / "src/app/text_editor/block_editor"
    for absolute_path in sorted(block_editor_root.glob("BlockEditor*.[ch]*")):
        relative_path = absolute_path.relative_to(REPO_ROOT).as_posix()
        file_text = absolute_path.read_text(encoding="utf-8", errors="replace")
        for pattern, message in BLOCK_EDITOR_FORBIDDEN_PATTERNS:
            if pattern in file_text:
                dependency_violations.append(f"{relative_path}: forbidden `{pattern}` ({message})")
        if (
            '#include "../TextEditorTab.h"' in file_text
            and relative_path not in ALLOWED_BLOCK_EDITOR_TEXT_EDITOR_INCLUDES
        ):
            dependency_violations.append(
                f"{relative_path}: forbidden direct TextEditorTab include "
                "(BlockEditor implementation shall depend on explicit contexts)"
            )

    if violations or layout_violations or dependency_violations:
        print("Structure constraint violations detected:")
        for relative_path, actual, limit in violations:
            if actual < 0:
                print(f"  - {relative_path}: file missing (expected max {limit} lines)")
            else:
                print(f"  - {relative_path}: {actual} lines (max {limit})")
        for violation in layout_violations:
            print(f"  - {violation}")
        for violation in dependency_violations:
            print(f"  - {violation}")
        print("")
        print("Keep large UI translation units from growing and preserve stable editor-mode/dependency layout.")
        return 1

    print("Structure constraints passed:")
    for relative_path, actual, limit in checked:
        print(f"  - {relative_path}: {actual}/{limit} lines")
    print("Layout constraints passed:")
    for relative_path in REQUIRED_PATHS:
        print(f"  - required present: {relative_path}")
    for relative_path in FORBIDDEN_PATHS:
        print(f"  - legacy absent: {relative_path}")
    print("Dependency constraints passed:")
    print("  - TextEditorTab has no BlockEditor friend declarations")
    print("  - BlockEditor controllers use explicit contexts instead of TextEditorTab owner pointers")
    return 0


if __name__ == "__main__":
    sys.exit(main())
