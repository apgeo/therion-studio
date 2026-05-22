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


def count_lines(path: pathlib.Path) -> int:
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        return sum(1 for _ in handle)


def main() -> int:
    violations: list[tuple[str, int, int]] = []
    checked: list[tuple[str, int, int]] = []
    layout_violations: list[str] = []

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

    if violations or layout_violations:
        print("Structure constraint violations detected:")
        for relative_path, actual, limit in violations:
            if actual < 0:
                print(f"  - {relative_path}: file missing (expected max {limit} lines)")
            else:
                print(f"  - {relative_path}: {actual} lines (max {limit})")
        for violation in layout_violations:
            print(f"  - {violation}")
        print("")
        print("Keep large UI translation units from growing and preserve stable editor-mode layout.")
        return 1

    print("Structure constraints passed:")
    for relative_path, actual, limit in checked:
        print(f"  - {relative_path}: {actual}/{limit} lines")
    print("Layout constraints passed:")
    for relative_path in REQUIRED_PATHS:
        print(f"  - required present: {relative_path}")
    for relative_path in FORBIDDEN_PATHS:
        print(f"  - legacy absent: {relative_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
