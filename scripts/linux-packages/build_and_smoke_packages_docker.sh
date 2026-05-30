#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
cd "$repo_root"

scripts/linux-packages/build_deb_package_docker.sh
scripts/linux-packages/build_appimage_package_docker.sh
scripts/linux-packages/smoke_deb_package_docker.sh build-linux-packages
scripts/linux-packages/smoke_appimage_package_docker.sh ubuntu:26.04 build-linux-appimage
scripts/linux-packages/smoke_appimage_package_docker.sh debian:13 build-linux-appimage
