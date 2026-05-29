#!/usr/bin/env python3
"""Verify Windows installer artifact naming and emit a release manifest."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import pathlib
import sys


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_github_output(output_path: pathlib.Path, fields: dict[str, str]) -> None:
    with output_path.open("a", encoding="utf-8") as handle:
        for key, value in fields.items():
            handle.write(f"{key}={value}\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--expected-package-label", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--source-ref", required=True)
    parser.add_argument("--qt-version", required=True)
    parser.add_argument("--build-type", required=True)
    parser.add_argument("--manifest-out", required=True)
    parser.add_argument("--github-output")
    args = parser.parse_args()

    build_dir = pathlib.Path(args.build_dir).resolve()
    if not build_dir.exists():
        print(f"Build directory does not exist: {build_dir}")
        return 1

    expected_name = f"TherionStudio-{args.expected_package_label}-Windows-x86_64.exe"
    expected_path = build_dir / expected_name
    if not expected_path.exists():
        print(
            "Expected installer artifact missing: "
            f"{expected_path}\n"
            f"Expected package label: {args.expected_package_label}"
        )
        return 1

    matches = sorted(build_dir.glob("TherionStudio-*-Windows-x86_64.exe"))
    if len(matches) != 1:
        print(
            "Unexpected installer artifact count in build directory. "
            f"Expected exactly 1, found {len(matches)}."
        )
        for match in matches:
            print(f"  - {match.name}")
        return 1

    installer_path = matches[0]
    if installer_path.name != expected_name:
        print(
            "Installer artifact name does not match expected package label:\n"
            f"  expected: {expected_name}\n"
            f"  actual:   {installer_path.name}"
        )
        return 1

    installer_sha256 = sha256_file(installer_path)
    installer_size = installer_path.stat().st_size

    manifest = {
        "artifact": {
            "file_name": installer_path.name,
            "relative_path": installer_path.relative_to(build_dir).as_posix(),
            "sha256": installer_sha256,
            "size_bytes": installer_size,
        },
        "build": {
            "build_type": args.build_type,
            "platform": "windows-2022",
            "qt_version": args.qt_version,
            "source_ref": args.source_ref,
        },
        "package": {
            "package_label": args.expected_package_label,
            "version": args.version,
        },
        "verified_at_utc": dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }

    manifest_path = pathlib.Path(args.manifest_out).resolve()
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    if args.github_output:
        output_path = pathlib.Path(args.github_output).resolve()
        write_github_output(
            output_path,
            {
                "installer_name": installer_path.name,
                "installer_path": str(installer_path),
                "manifest_path": str(manifest_path),
            },
        )

    print(f"Verified installer artifact: {installer_path}")
    print(f"Wrote installer manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
