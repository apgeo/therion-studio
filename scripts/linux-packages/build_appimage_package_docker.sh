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
            default_appimagetool_url="https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage"
            default_appimagetool_sha256="ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0"
            default_appimage_runtime_url="https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64"
            default_appimage_runtime_sha256="2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d"
            ;;
        arm64 | aarch64 | linux/arm64)
            default_appimage_architecture_label="aarch64"
            default_docker_platform="linux/arm64"
            default_appimagetool_url="https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-aarch64.AppImage"
            default_appimagetool_sha256="f0837e7448a0c1e4e650a93bb3e85802546e60654ef287576f46c71c126a9158"
            default_appimage_runtime_url="https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-aarch64"
            default_appimage_runtime_sha256="00cbdfcf917cc6c0ff6d3347d59e0ca1f7f45a6df1a428a0d6d8a78664d87444"
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
appimage_build_image="${APPIMAGE_BUILD_IMAGE:-debian:13}"
appimage_build_platform="${APPIMAGE_BUILD_PLATFORM:-${DOCKER_PLATFORM:-$default_docker_platform}}"
appimage_architecture_label="${THERION_STUDIO_APPIMAGE_ARCHITECTURE_LABEL:-$default_appimage_architecture_label}"
appimage_qt_packages="${APPIMAGE_QT_PACKAGES:-qt6-base-dev,qt6-base-dev-tools,qt6-declarative-dev,qml6-module-qtqml-workerscript,qml6-module-qtquick-controls,qml6-module-qtquick-layouts,qt6-qpa-plugins,qt6-svg-dev,qt6-tools-dev,qt6-tools-dev-tools}"
appimagetool_url="${APPIMAGETOOL_URL:-$default_appimagetool_url}"
appimagetool_sha256="${APPIMAGETOOL_SHA256:-$default_appimagetool_sha256}"
appimage_runtime_url="${APPIMAGE_RUNTIME_URL:-$default_appimage_runtime_url}"
appimage_runtime_sha256="${APPIMAGE_RUNTIME_SHA256:-$default_appimage_runtime_sha256}"

docker run --rm \
    --platform "$appimage_build_platform" \
    -e "APPIMAGETOOL_SHA256=$appimagetool_sha256" \
    -e "APPIMAGETOOL_URL=$appimagetool_url" \
    -e "APPIMAGE_QT_PACKAGES=$appimage_qt_packages" \
    -e "APPIMAGE_RUNTIME_SHA256=$appimage_runtime_sha256" \
    -e "APPIMAGE_RUNTIME_URL=$appimage_runtime_url" \
    -e "BUILD_TYPE=$build_type" \
    -e "HOST_GID=$(id -g)" \
    -e "HOST_UID=$(id -u)" \
    -e "THERION_STUDIO_APPIMAGE_ARCHITECTURE_LABEL=$appimage_architecture_label" \
    -e "THERION_STUDIO_DEBIAN_VERSION=$debian_version" \
    -e "THERION_STUDIO_PACKAGE_LABEL=$package_label" \
    -e "THERION_STUDIO_VERSION=$version" \
    -v "$PWD:/workspace" \
    -w /workspace \
    "$appimage_build_image" \
    bash scripts/linux-packages/build_appimage_package.sh
