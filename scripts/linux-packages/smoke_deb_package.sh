#!/usr/bin/env bash
set -euo pipefail

artifact_dir="${THERION_STUDIO_ARTIFACT_DIR:-}"
deb_architecture_label="${THERION_STUDIO_DEB_ARCHITECTURE_LABEL:-}"
timeout_seconds="${THERION_STUDIO_SMOKE_TIMEOUT_SECONDS:-10}"

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
    file

deb_name_pattern="therion-studio-*.deb"
if [[ -n "$deb_architecture_label" ]]; then
    deb_name_pattern="therion-studio-*-${deb_architecture_label}.deb"
fi

deb_path="$(
    find "$artifact_dir" \
        -maxdepth 3 \
        -type f \
        -name "$deb_name_pattern" \
        ! -path "*/_CPack_Packages/*" \
        | sort \
        | head -n1
)"
if [[ -z "$deb_path" ]]; then
    echo "No .deb artifact found in $artifact_dir" >&2
    find "$artifact_dir" -maxdepth 3 -print | sort >&2 || true
    exit 1
fi
deb_path="$(realpath "$deb_path")"

apt-get install -y "$deb_path"

test -x /usr/bin/TherionStudio
test -f /usr/share/applications/therion-studio.desktop
test -f /usr/share/icons/hicolor/256x256/apps/therion-studio.png
test -f /usr/share/metainfo/therion-studio.metainfo.xml

set +e
QT_QPA_PLATFORM=offscreen timeout "$timeout_seconds" \
    /usr/bin/TherionStudio -platform offscreen >/tmp/therion-studio-launch.log 2>&1
exit_code=$?
set -e
if [[ "$exit_code" -ne 0 && "$exit_code" -ne 124 ]]; then
    echo "TherionStudio launch failed with exit code $exit_code" >&2
    cat /tmp/therion-studio-launch.log >&2
    exit "$exit_code"
fi
if [[ "$exit_code" -eq 124 ]]; then
    echo "TherionStudio started successfully (terminated by timeout as expected)."
fi
