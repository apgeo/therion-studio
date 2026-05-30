#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
cd "$repo_root"

resolve_linux_architecture() {
    local requested_arch="${THERION_STUDIO_LINUX_ARCH:-}"
    if [[ -z "$requested_arch" && -n "${DOCKER_PLATFORM:-}" ]]; then
        requested_arch="${DOCKER_PLATFORM#linux/}"
    fi
    requested_arch="${requested_arch:-amd64}"

    case "$requested_arch" in
        amd64 | x86_64 | linux/amd64)
            default_appimage_architecture_label="x86_64"
            default_docker_platform="linux/amd64"
            ;;
        arm64 | aarch64 | linux/arm64)
            default_appimage_architecture_label="aarch64"
            default_docker_platform="linux/arm64"
            ;;
        *)
            echo "Unsupported Linux package architecture: $requested_arch" >&2
            exit 2
            ;;
    esac
}

resolve_linux_architecture

smoke_image="${1:-${APPIMAGE_SMOKE_IMAGE:-debian:13}}"
artifact_dir="${2:-build-linux-appimage}"
smoke_platform="${APPIMAGE_SMOKE_PLATFORM:-${DOCKER_PLATFORM:-$default_docker_platform}}"
appimage_architecture_label="${THERION_STUDIO_APPIMAGE_ARCHITECTURE_LABEL:-$default_appimage_architecture_label}"
timeout_seconds="${THERION_STUDIO_SMOKE_TIMEOUT_SECONDS:-12}"

docker run --rm \
    --platform "$smoke_platform" \
    -e "THERION_STUDIO_APPIMAGE_ARCHITECTURE_LABEL=$appimage_architecture_label" \
    -e "THERION_STUDIO_ARTIFACT_DIR=$artifact_dir" \
    -e "THERION_STUDIO_SMOKE_TIMEOUT_SECONDS=$timeout_seconds" \
    -v "$PWD:/workspace" \
    -w /workspace \
    "$smoke_image" \
    bash scripts/linux-packages/smoke_appimage_package.sh
