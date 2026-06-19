#!/usr/bin/env bash
set -euo pipefail

artifact_dir="${THERION_STUDIO_ARTIFACT_DIR:-}"
appimage_architecture_label="${THERION_STUDIO_APPIMAGE_ARCHITECTURE_LABEL:-}"
timeout_seconds="${THERION_STUDIO_SMOKE_TIMEOUT_SECONDS:-12}"

if [[ -z "$artifact_dir" ]]; then
    echo "Missing required environment variable: THERION_STUDIO_ARTIFACT_DIR" >&2
    exit 2
fi

export DEBIAN_FRONTEND=noninteractive
export LANG=C.UTF-8
export LC_ALL=C.UTF-8

apt-get update
apt-get install -y \
    ca-certificates \
    coreutils \
    file \
    libdbus-1-3 \
    libegl1 \
    libfontconfig1 \
    libfreetype6 \
    libgl1 \
    libglib2.0-0 \
    libopengl0 \
    libxkbcommon0

appimage_name_pattern="TherionStudio-*.AppImage"
if [[ -n "$appimage_architecture_label" ]]; then
    appimage_name_pattern="TherionStudio-*Linux-${appimage_architecture_label}.AppImage"
fi

appimage_path="$(
    find "$artifact_dir" \
        -maxdepth 3 \
        -type f \
        -name "$appimage_name_pattern" \
        ! -path "*/_CPack_Packages/*" \
        | sort \
        | head -n1
)"
if [[ -z "$appimage_path" ]]; then
    echo "No AppImage artifact found in $artifact_dir" >&2
    find "$artifact_dir" -maxdepth 3 -print | sort >&2 || true
    exit 1
fi
appimage_path="$(realpath "$appimage_path")"
chmod +x "$appimage_path"

set +e
QT_QPA_PLATFORM=offscreen APPIMAGE_EXTRACT_AND_RUN=1 timeout "$timeout_seconds" \
    "$appimage_path" -platform offscreen >/tmp/therion-studio-appimage-launch.log 2>&1
exit_code=$?
set -e
if [[ "$exit_code" -ne 0 && "$exit_code" -ne 124 ]]; then
    echo "AppImage launch failed with exit code $exit_code" >&2
    cat /tmp/therion-studio-appimage-launch.log >&2
    echo "Bundled libproxy runtime libraries:" >&2
    find /tmp/appimage_extracted_* -path "*/usr/lib/libproxy.so*" -print 2>/dev/null | sort >&2 || true
    echo "Bundled libduktape runtime libraries:" >&2
    find /tmp/appimage_extracted_* -path "*/usr/lib/libduktape.so*" -print 2>/dev/null | sort >&2 || true
    echo "Bundled libproxy backend runtime libraries:" >&2
    find /tmp/appimage_extracted_* -path "*/usr/lib/libpxbackend-*.so*" -print 2>/dev/null | sort >&2 || true
    exit "$exit_code"
fi
if [[ "$exit_code" -eq 124 ]]; then
    echo "AppImage started successfully (terminated by timeout as expected)."
fi
