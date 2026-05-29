#!/usr/bin/env python3
"""Verify Linux .deb release artifacts and emit a manifest."""

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
    parser.add_argument("--build-type", required=True)
    parser.add_argument("--manifest-out", required=True)
    parser.add_argument("--github-output")
    args = parser.parse_args()

    build_dir = pathlib.Path(args.build_dir).resolve()
    if not build_dir.exists():
        print(f"Build directory does not exist: {build_dir}")
        return 1

    expected_deb_name = f"therion-studio-{args.expected_package_label}-linux-x86_64.deb"
    deb_path = build_dir / expected_deb_name
    if not deb_path.exists():
        print(f"Missing expected .deb artifact: {deb_path}")
        return 1

    deb_sha = sha256_file(deb_path)

    manifest = {
        "artifacts": {
            "deb": {
                "file_name": deb_path.name,
                "sha256": deb_sha,
                "size_bytes": deb_path.stat().st_size,
            },
        },
        "build": {
            "build_type": args.build_type,
            "platform": "ubuntu-24.04",
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
                "deb_path": str(deb_path),
                "manifest_path": str(manifest_path),
            },
        )

    print(f"Verified Linux artifacts in {build_dir}")
    print(f"Wrote Linux artifact manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
