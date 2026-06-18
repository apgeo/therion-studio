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

build_type="${BUILD_TYPE:-Release}"
short_sha="${THERION_STUDIO_SHORT_SHA:-$(git rev-parse --short HEAD)}"
year="$(date -u +%Y)"
month="$((10#$(date -u +%m)))"
snapshot_date="$(date -u +%Y%m%d)"
version="${THERION_STUDIO_VERSION:-${year}.${month}.0}"
package_label="${THERION_STUDIO_PACKAGE_LABEL:-dev-${short_sha}}"
debian_version="${THERION_STUDIO_DEBIAN_VERSION:-${version}+git${snapshot_date}.g${short_sha}}"
deb_platform_label="${THERION_STUDIO_DEB_PLATFORM_LABEL:-ubuntu-26.04}"
deb_architecture_label="${THERION_STUDIO_DEB_ARCHITECTURE_LABEL:-$default_deb_architecture_label}"
deb_build_image="${DEB_BUILD_IMAGE:-ubuntu:26.04}"
deb_build_platform="${DEB_BUILD_PLATFORM:-${DOCKER_PLATFORM:-$default_docker_platform}}"
deb_qt_packages="${DEB_QT_PACKAGES:-qt6-base-dev,qt6-base-dev-tools,qt6-declarative-dev,qml6-module-qtqml-workerscript,qml6-module-qtquick-controls,qml6-module-qtquick-layouts,qt6-qpa-plugins,qt6-svg-dev,qt6-tools-dev,qt6-tools-dev-tools}"

docker run --rm \
    --platform "$deb_build_platform" \
    -e "BUILD_TYPE=$build_type" \
    -e "DEB_QT_PACKAGES=$deb_qt_packages" \
    -e "HOST_GID=$(id -g)" \
    -e "HOST_UID=$(id -u)" \
    -e "THERION_STUDIO_DEB_ARCHITECTURE_LABEL=$deb_architecture_label" \
    -e "THERION_STUDIO_DEB_PLATFORM_LABEL=$deb_platform_label" \
    -e "THERION_STUDIO_DEBIAN_VERSION=$debian_version" \
    -e "THERION_STUDIO_PACKAGE_LABEL=$package_label" \
    -e "THERION_STUDIO_VERSION=$version" \
    -v "$PWD:/workspace" \
    -w /workspace \
    "$deb_build_image" \
    bash scripts/linux-packages/build_deb_package.sh
