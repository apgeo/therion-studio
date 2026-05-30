#!/usr/bin/env python3
"""Verify Linux release artifacts (.deb + AppImage) and emit a manifest."""

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
    parser.add_argument("--appimage-dir", required=True)
    parser.add_argument("--expected-package-label", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--source-ref", required=True)
    parser.add_argument("--build-type", required=True)
    parser.add_argument("--debian-version", required=True)
    parser.add_argument("--deb-platform-label", required=True)
    parser.add_argument("--deb-build-source", required=True)
    parser.add_argument("--deb-qt-version", required=True)
    parser.add_argument("--deb-qt-architecture", required=True)
    parser.add_argument("--deb-qt-packages", required=True)
    parser.add_argument("--appimagetool-url", required=True)
    parser.add_argument("--appimagetool-sha256", required=True)
    parser.add_argument("--appimage-runtime-url", required=True)
    parser.add_argument("--appimage-runtime-sha256", required=True)
    parser.add_argument("--appimage-architecture", default="x86_64")
    parser.add_argument("--appimage-qt-source", required=True)
    parser.add_argument("--appimage-qt-version", required=True)
    parser.add_argument("--appimage-qt-architecture", required=True)
    parser.add_argument("--appimage-qt-packages", required=True)
    parser.add_argument("--manifest-out", required=True)
    parser.add_argument("--runner-label", default="ubuntu-24.04")
    parser.add_argument("--github-output")
    args = parser.parse_args()

    build_dir = pathlib.Path(args.build_dir).resolve()
    if not build_dir.exists():
        print(f"Build directory does not exist: {build_dir}")
        return 1
    appimage_dir = pathlib.Path(args.appimage_dir).resolve()
    if not appimage_dir.exists():
        print(f"AppImage build directory does not exist: {appimage_dir}")
        return 1

    expected_deb_name = (
        f"therion-studio-{args.expected_package_label}-"
        f"{args.deb_platform_label}-{args.deb_qt_architecture}.deb"
    )
    deb_path = build_dir / expected_deb_name
    if not deb_path.exists():
        print(f"Missing expected .deb artifact: {deb_path}")
        return 1

    expected_appimage_name = (
        f"TherionStudio-{args.expected_package_label}-Linux-{args.appimage_architecture}.AppImage"
    )
    appimage_path = appimage_dir / expected_appimage_name
    if not appimage_path.exists():
        print(f"Missing expected AppImage artifact: {appimage_path}")
        return 1

    deb_sha = sha256_file(deb_path)
    appimage_sha = sha256_file(appimage_path)

    manifest = {
        "artifacts": {
            "appimage": {
                "file_name": appimage_path.name,
                "sha256": appimage_sha,
                "size_bytes": appimage_path.stat().st_size,
            },
            "deb": {
                "file_name": deb_path.name,
                "sha256": deb_sha,
                "size_bytes": deb_path.stat().st_size,
            },
        },
        "build": {
            "build_type": args.build_type,
            "deb": {
                "qt": {
                    "architecture": args.deb_qt_architecture,
                    "packages": [
                        package.strip()
                        for package in args.deb_qt_packages.split(",")
                        if package.strip()
                    ],
                    "source": args.deb_build_source,
                    "version": args.deb_qt_version,
                },
            },
            "appimage": {
                "architecture": args.appimage_architecture,
                "appimagetool": {
                    "sha256": args.appimagetool_sha256,
                    "url": args.appimagetool_url,
                },
                "runtime": {
                    "sha256": args.appimage_runtime_sha256,
                    "url": args.appimage_runtime_url,
                },
                "qt": {
                    "architecture": args.appimage_qt_architecture,
                    "packages": [
                        package.strip()
                        for package in args.appimage_qt_packages.split(",")
                        if package.strip()
                    ],
                    "source": args.appimage_qt_source,
                    "version": args.appimage_qt_version,
                },
            },
            "runner": args.runner_label,
            "source_ref": args.source_ref,
        },
        "package": {
            "debian_version": args.debian_version,
            "deb_platform_label": args.deb_platform_label,
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
                "appimage_path": str(appimage_path),
                "deb_path": str(deb_path),
                "manifest_path": str(manifest_path),
            },
        )

    print(f"Verified Linux artifacts in {build_dir}")
    print(f"Wrote Linux artifact manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
