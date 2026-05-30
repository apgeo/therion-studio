# Building Therion Studio

This document describes the current build and packaging workflow.

## Common Requirements

- CMake 3.21 or newer
- C++20 compiler
- Qt 6.4 or newer with `Widgets`, `Svg`, and `Concurrent`

The bundled command catalog, syntax palette, and UI icons are compiled into Qt resources.

## Map Object Style Gallery

The map object style gallery is a developer review artifact for bundled and user override styles.
It renders all command-catalog-supported `point`, `line`, and `area` type/subtype combinations,
including combinations with no dedicated style file that fall back to type or global defaults.

Configure the build tree first if it does not exist yet:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

Generate the gallery:

```sh
cmake --build build --target map_object_style_gallery
```

The default output is:

```text
build/style_gallery/index.html
```

Open `index.html` in a browser to review the generated PNG previews. The gallery labels each
entry by style coverage, for example `exact subtype style`, `type style`,
`inherited type style`, or `global default`.

To write the gallery to a custom directory, run the helper executable directly after building it:

```sh
cmake --build build --target MapObjectStyleGallery
QT_QPA_PLATFORM=offscreen ./build/MapObjectStyleGallery --output /tmp/therion-style-gallery
```

## macOS

macOS packaging is intended for Homebrew formula installation. The app bundle does not bundle Qt frameworks; the formula shall declare Qt dependencies instead.
CI targets the last two major macOS versions available as GitHub-hosted runners.

Configure and build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target TherionStudio
```

Install to a staging prefix:

```sh
cmake --install build --prefix /tmp/therion-studio-stage
```

The install step places `TherionStudio.app` in the prefix. Do not run `macdeployqt` for Homebrew formula builds.

Formula dependencies should include:

- `qt`

The maintained source-build Homebrew formula is published in the dedicated tap repository:

- `https://github.com/ladislavb/homebrew-therion-studio`

Use the tap formula as the source of truth for current package metadata and dependency declarations.

The external Therion command-line executable is not bundled. Users configure it in the application runner settings, or install it separately through Homebrew.

## Windows

Windows packaging is intended to produce an installer containing:

- Therion Studio
- Qt runtime libraries and plugins required by the app

The installer shall not bundle the external `therion.exe` command-line executable.
The maintained Windows packaging notes and GitHub Actions workflow details are in
`packaging/windows/README.md`.

Recommended MSVC release build:

```powershell
cmake -S . -B build-win -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64 `
  -DTHERION_STUDIO_VERSION=2026.5.0

cmake --build build-win --target TherionStudio
cpack --config build-win\CPackConfig.cmake
```

Use a matching toolchain/Qt pair on Windows. If `CMAKE_PREFIX_PATH` points to
`...\msvc2022_64`, configure with MSVC (`cl`). Mixing MinGW (`g++`) with MSVC Qt
libraries causes link failures with unresolved `__imp__` Qt symbols.

CPack runs the CMake install step internally. The install step runs Qt deployment on Windows,
copying the required Qt runtime next to the installed `bin/TherionStudio.exe`, including the
Qt platform plugin at `bin/platforms/qwindows.dll`. CPack is configured to use NSIS for the
Windows installer and emits `TherionStudio-<package_label>-Windows-x86_64.exe` in the build
directory.
The installer metadata uses the root `LICENSE` file for the project license.
The installed executable is linked as a Windows GUI application and shall not open a console
window when launched from Explorer, Start Menu, or the desktop shortcut.

Required Windows packaging tools:

- Qt for MSVC
- Ninja or another CMake generator
- NSIS for installer generation

The manual GitHub Actions workflow `.github/workflows/windows-installer.yml` builds the same
installer on `windows-2022` and uploads it as an artifact. Use its `source_ref` input to build
from `main`, a release tag, or a specific commit SHA. When the checked-out commit is exactly tagged
with a CalVer release tag such as `v2026.5.0`, the workflow derives
`THERION_STUDIO_VERSION=2026.5.0` and passes it to CMake so CPack uses the tag version without
editing `CMakeLists.txt`. Branch and SHA builds derive a CalVer development application version
such as `2026.5.0` and a human-readable package label such as `dev-a1b2c3d`, matching the Linux
snapshot artifact convention. The workflow also validates that the produced installer file name
matches the resolved package label and emits a manifest JSON with installer SHA256.

CI build workflows also run staged install-layout smoke checks via
`scripts/verify_install_layout.py`:

- Linux: verify `bin/TherionStudio`
- macOS: verify `TherionStudio.app/Contents/MacOS/TherionStudio`
- Windows: verify `bin/TherionStudio.exe` plus deployed Qt runtime layout (including `bin/platforms/qwindows.dll`)

## Linux

Linux packaging currently targets two distributable artifacts:

- `.deb` package for Ubuntu 26.04
- AppImage as the portable Linux channel

The manual workflow `.github/workflows/linux-packages.yml` builds the `.deb` package inside an
`ubuntu:26.04` container and builds the AppImage inside a `debian:13` container. It validates
install layout and artifact naming, then uploads:

- `therion-studio-<package_label>-ubuntu-26.04-amd64.deb`
- `TherionStudio-<package_label>-Linux-x86_64.AppImage`
- `TherionStudio-Linux-artifacts-manifest.json` (SHA256 + build metadata)

The `.deb` artifact intentionally includes `ubuntu-26.04` in the file name because Qt package
dependency metadata is distribution-release-specific. Do not treat that `.deb` as the compatibility
path for Debian, older Ubuntu releases, or newer Ubuntu releases unless a matching package is built
and smoke-tested for that baseline.
Release-tagged builds use the CalVer tag as both the artifact label and Debian package version.
Snapshot builds use `dev-<short_sha>` as the artifact label and a Debian package version in the
form `<calver>+git<yyyymmdd>.g<short_sha>`, so artifact names stay readable while `apt` sees a
valid, monotonic package version.

The AppImage is built from a separate CMake tree inside a `debian:13` container with
`THERION_ENABLE_QT_LINUX_DEPLOY_INSTALL=ON` and Debian's distro Qt packages. The AppImage path
enables Qt's generated Linux deployment script during install so the AppDir receives Qt plugins and
deploy metadata. Because Debian's Qt shared libraries live in system library directories that Qt's
Linux deploy helper may exclude by default, the workflow also copies selected Qt plugin groups from
the distro Qt plugin directory, copies the `ldd`-resolved `libQt6*.so*` runtime families into
`AppDir/usr/lib`, writes an `AppRun` wrapper that sets `LD_LIBRARY_PATH` and `QT_PLUGIN_PATH`, and
performs an offscreen AppDir launch sanity check before packaging through
`scripts/prepare_linux_appimage_appdir.sh`. The workflow then packages the AppDir with pinned
`appimagetool` 1.9.1 and a pinned `type2-runtime` 20251108 runtime, both SHA256-verified before
execution/use. The manifest records the `.deb` and AppImage Qt package sources, versions, and
package sets in addition to `appimagetool` and runtime provenance.

The same workflow also runs follow-up smoke jobs in `ubuntu:26.04` and `debian:13` containers.
The Ubuntu target installs the produced `.deb`, verifies installed paths, and performs an offscreen
`.deb` launch sanity check. Both Ubuntu 26.04 and Debian 13 launch the generated AppImage. The
`.deb` package is the Ubuntu 26.04 coverage path only. The AppImage should be documented as tested
on Debian 13 and Ubuntu 26.04 only when both smoke jobs pass for that artifact.

Do not use mutable `linuxdeployqt` `continuous` AppImage downloads or unmaintained Qt
deployment plugins for production release artifact generation.

Regular CI still validates distribution Qt source builds on Ubuntu 24.04 and Ubuntu 26.04;
Ubuntu 26.04 runs in an official Ubuntu container image until GitHub provides a hosted runner
label for it. The Linux packaging workflow additionally smoke-tests generated release artifacts
on Debian 13.

On supported Ubuntu and Debian-family systems, install the development dependencies with:

```sh
sudo apt-get install qt6-base-dev qt6-base-dev-tools qt6-svg-dev qt6-tools-dev qt6-tools-dev-tools
```

Then use the same CMake configure/build flow as macOS.

Linux packaging details are documented in `packaging/linux/README.md`.
