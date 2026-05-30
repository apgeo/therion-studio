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

test -x "$app_binary"
mkdir -p "$qt_bundle_lib_dir"
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

dependency_roots="$(mktemp)"
trap 'rm -f "$dependency_roots"' EXIT
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
