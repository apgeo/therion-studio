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
            default_deb_architecture_label="amd64"
            default_docker_platform="linux/amd64"
            ;;
        arm64 | aarch64 | linux/arm64)
            default_deb_architecture_label="arm64"
            default_docker_platform="linux/arm64"
            ;;
        *)
            echo "Unsupported Linux package architecture: $requested_arch" >&2
            exit 2
            ;;
    esac
}

resolve_linux_architecture

artifact_dir="${1:-build-linux-packages}"
smoke_image="${DEB_SMOKE_IMAGE:-ubuntu:26.04}"
smoke_platform="${DEB_SMOKE_PLATFORM:-${DOCKER_PLATFORM:-$default_docker_platform}}"
deb_architecture_label="${THERION_STUDIO_DEB_ARCHITECTURE_LABEL:-$default_deb_architecture_label}"
timeout_seconds="${THERION_STUDIO_SMOKE_TIMEOUT_SECONDS:-10}"

docker run --rm \
    --platform "$smoke_platform" \
    -e "THERION_STUDIO_ARTIFACT_DIR=$artifact_dir" \
    -e "THERION_STUDIO_DEB_ARCHITECTURE_LABEL=$deb_architecture_label" \
    -e "THERION_STUDIO_SMOKE_TIMEOUT_SECONDS=$timeout_seconds" \
    -v "$PWD:/workspace" \
    -w /workspace \
    "$smoke_image" \
    bash scripts/linux-packages/smoke_deb_package.sh
