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
Windows installer and emits `TherionStudio-<version>-Windows-x86_64.exe` in the build directory.
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
editing `CMakeLists.txt`. Branch and SHA builds derive a development package label such as
`2026.5.0-a1b2c3d`. The workflow also validates that the produced installer file name matches the
resolved package label and emits a manifest JSON with installer SHA256.

CI build workflows also run staged install-layout smoke checks via
`scripts/verify_install_layout.py`:

- Linux: verify `bin/TherionStudio`
- macOS: verify `TherionStudio.app/Contents/MacOS/TherionStudio`
- Windows: verify `bin/TherionStudio.exe` plus deployed Qt runtime layout (including `bin/platforms/qwindows.dll`)

## Linux

Linux packaging currently targets two distributable artifacts:

- `.deb` package for Debian/Ubuntu-family systems
- AppImage as the portable Linux channel

The manual workflow `.github/workflows/linux-packages.yml` builds both packages on
`ubuntu-24.04`, validates install layout and artifact naming, and uploads:

- `therion-studio-<package_label>-linux-x86_64.deb`
- `TherionStudio-<package_label>-Linux-x86_64.AppImage`
- `TherionStudio-Linux-artifacts-manifest.json` (SHA256 + build metadata)

The `.deb` artifact intentionally keeps a distro-neutral `linux-x86_64` file name because the
package is intended for tested Debian/Ubuntu-family targets rather than a single Ubuntu release.

The AppImage is built from a separate CMake tree with
`THERION_ENABLE_QT_LINUX_DEPLOY_INSTALL=ON`. That enables Qt's generated Linux deployment
script during install so the AppDir receives the required Qt runtime files and plugins. The
workflow then packages the AppDir with pinned `appimagetool` 1.9.1 and a pinned
`type2-runtime` 20251108 runtime, both SHA256-verified before execution/use.

The same workflow also runs follow-up smoke jobs in `ubuntu:26.04` and `debian:13` containers.
Each target installs the produced `.deb`, verifies installed paths, and performs offscreen launch
sanity checks for both the installed `.deb` binary and the generated AppImage. A Linux package
artifact should be documented as tested on Ubuntu 26.04 and Debian 13 only when the corresponding
smoke jobs pass for that artifact.

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
