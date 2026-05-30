#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <AppDir>" >&2
    exit 2
fi

appdir="$1"
usr_dir="$appdir/usr"
app_binary="$usr_dir/bin/TherionStudio"
qt_bundle_lib_dir="$usr_dir/lib"
qt_bundle_plugin_dir="$usr_dir/plugins"

test -x "$app_binary"
mkdir -p "$qt_bundle_lib_dir"
mkdir -p "$qt_bundle_plugin_dir"
shopt -s nullglob

copy_qt_library_family() {
    local lib_path="$1"
    local dest_dir="$2"
    local source_dir
    local dest_abs
    local soname
    local family_base
    local -a matches

    source_dir="$(cd "$(dirname "$lib_path")" && pwd)"
    dest_abs="$(cd "$dest_dir" && pwd)"
    if [[ "$source_dir" == "$dest_abs" ]]; then
        return
    fi

    soname="$(basename "$lib_path")"
    family_base="${soname%%.so*}"
    if [[ "$family_base" == "$soname" ]]; then
        return
    fi

    matches=("$source_dir"/"${family_base}.so"*)
    if [[ ${#matches[@]} -eq 0 ]]; then
        echo "Unable to locate Qt library family for $lib_path" >&2
        exit 1
    fi
    cp -a "${matches[@]}" "$dest_dir/"
}

collect_qt_dependencies() {
    local binary="$1"
    local field1
    local field2
    local field3
    local rest

    while read -r field1 field2 field3 rest; do
        if [[ "$field2" == "=>" && "$field3" == /*/libQt6*.so* ]]; then
            printf "%s\n" "$field3"
        elif [[ "$field1" == /*/libQt6*.so* ]]; then
            printf "%s\n" "$field1"
        fi
    done < <(ldd "$binary" 2>/dev/null || true)
}

append_qt_plugin_root() {
    local plugin_root="$1"
    local roots_file="$2"

    if [[ -d "$plugin_root" ]] && ! grep -Fxq "$plugin_root" "$roots_file"; then
        printf "%s\n" "$plugin_root" >> "$roots_file"
    fi
}

collect_qt_plugin_roots() {
    local roots_file="$1"
    local multiarch

    : > "$roots_file"

    if [[ -n "${THERION_STUDIO_QT_PLUGIN_DIR:-}" ]]; then
        append_qt_plugin_root "$THERION_STUDIO_QT_PLUGIN_DIR" "$roots_file"
    fi
    if command -v qtpaths6 >/dev/null 2>&1; then
        append_qt_plugin_root "$(qtpaths6 --plugin-dir 2>/dev/null || true)" "$roots_file"
    fi
    if command -v qtpaths >/dev/null 2>&1; then
        append_qt_plugin_root "$(qtpaths --plugin-dir 2>/dev/null || true)" "$roots_file"
    fi
    if command -v qmake6 >/dev/null 2>&1; then
        append_qt_plugin_root "$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || true)" "$roots_file"
    fi
    if command -v dpkg-architecture >/dev/null 2>&1; then
        multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || true)"
        if [[ -n "$multiarch" ]]; then
            append_qt_plugin_root "/usr/lib/$multiarch/qt6/plugins" "$roots_file"
        fi
    fi

    append_qt_plugin_root "/usr/lib/x86_64-linux-gnu/qt6/plugins" "$roots_file"
    append_qt_plugin_root "/usr/lib/qt6/plugins" "$roots_file"
}

copy_qt_plugin_groups() {
    local roots_file="$1"
    local plugin_root
    local group
    local source_dir
    local -a plugin_groups=(
        iconengines
        imageformats
        platforminputcontexts
        platforms
        tls
        xcbglintegrations
    )

    while IFS= read -r plugin_root; do
        for group in "${plugin_groups[@]}"; do
            source_dir="$plugin_root/$group"
            if [[ -d "$source_dir" ]]; then
                mkdir -p "$qt_bundle_plugin_dir/$group"
                cp -a "$source_dir"/. "$qt_bundle_plugin_dir/$group/"
            fi
        done
    done < "$roots_file"
}

plugin_roots="$(mktemp)"
dependency_roots="$(mktemp)"
trap 'rm -f "$plugin_roots" "$dependency_roots"' EXIT

collect_qt_plugin_roots "$plugin_roots"
copy_qt_plugin_groups "$plugin_roots"

find "$usr_dir" -type f \( -path "$app_binary" -o -name "*.so" -o -name "*.so.*" \) | sort > "$dependency_roots"
while IFS= read -r binary; do
    while IFS= read -r qt_lib; do
        copy_qt_library_family "$qt_lib" "$qt_bundle_lib_dir"
    done < <(collect_qt_dependencies "$binary")
done < "$dependency_roots"

if ! find "$qt_bundle_lib_dir" -maxdepth 1 -name "libQt6Widgets.so.6*" | grep -q .; then
    echo "Qt deployment did not stage libQt6Widgets.so.6" >&2
    find "$usr_dir" -maxdepth 4 -type f | sort >&2
    exit 1
fi

if ! find "$usr_dir" -type f -path "*/platforms/libqoffscreen.so" | grep -q .; then
    echo "Qt deployment did not stage the offscreen platform plugin" >&2
    find "$usr_dir" -maxdepth 5 -type f | sort >&2
    exit 1
fi

cat > "$appdir/AppRun" <<\EOF
#!/bin/sh
set -eu

appdir="${APPDIR:-}"
if [ -z "$appdir" ]; then
  apprun="$0"
  case "$apprun" in
    /*) ;;
    *) apprun="$PWD/$apprun" ;;
  esac
  while [ -L "$apprun" ]; do
    apprun_dir="$(dirname "$apprun")"
    apprun_target="$(readlink "$apprun")"
    case "$apprun_target" in
      /*) apprun="$apprun_target" ;;
      *) apprun="$apprun_dir/$apprun_target" ;;
    esac
  done
  appdir="$(cd "$(dirname "$apprun")" && pwd)"
fi

if [ -n "${LD_LIBRARY_PATH:-}" ]; then
  export LD_LIBRARY_PATH="$appdir/usr/lib:$appdir/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH"
else
  export LD_LIBRARY_PATH="$appdir/usr/lib:$appdir/usr/lib/x86_64-linux-gnu"
fi

qt_plugin_path="$appdir/usr/plugins:$appdir/usr/lib/qt6/plugins:$appdir/usr/lib/x86_64-linux-gnu/qt6/plugins"
if [ -n "${QT_PLUGIN_PATH:-}" ]; then
  export QT_PLUGIN_PATH="$qt_plugin_path:$QT_PLUGIN_PATH"
else
  export QT_PLUGIN_PATH="$qt_plugin_path"
fi

exec "$appdir/usr/bin/TherionStudio" "$@"
EOF
chmod +x "$appdir/AppRun"

set +e
APPDIR="$appdir" QT_QPA_PLATFORM=offscreen timeout 10s \
    "$appdir/AppRun" -platform offscreen >/tmp/therion-studio-appdir-launch.log 2>&1
exit_code=$?
set -e
if [[ "$exit_code" -ne 0 && "$exit_code" -ne 124 ]]; then
    echo "AppDir launch failed with exit code $exit_code" >&2
    cat /tmp/therion-studio-appdir-launch.log >&2
    exit "$exit_code"
fi
