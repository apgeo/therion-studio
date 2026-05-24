# Building Therion Studio

This document describes the current build and packaging workflow.

## Common Requirements

- CMake 3.21 or newer
- C++20 compiler
- Qt 6.5 or newer with `Widgets`, `Svg`, and `Concurrent`

The bundled command catalog, syntax palette, and UI icons are compiled into Qt resources.

## macOS

macOS packaging is intended for Homebrew formula installation. The app bundle does not bundle Qt frameworks; the formula shall declare Qt dependencies instead.

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

- `qtbase`
- `qtsvg`

The draft source-build Homebrew formula is kept in `packaging/homebrew/Formula/therion-studio.rb`
until it is moved to a dedicated external tap repository. Before publishing it, replace the
repository URL/checksum placeholders and declare the final project license.

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
  -DCMAKE_PREFIX_PATH=C:\Qt\6.7.3\msvc2022_64 `
  -DTHERION_STUDIO_VERSION=2026.5.0

cmake --build build-win --target TherionStudio
cpack --config build-win\CPackConfig.cmake
```

CPack runs the CMake install step internally. The install step runs Qt deployment on Windows,
copying the required Qt runtime next to the installed executable. CPack is configured to use
NSIS for the Windows installer and emits `TherionStudio-<version>-Windows-x86_64.exe` in the
build directory.

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
`2026.5.0-a1b2c3d`.

## Linux

Linux release packaging is not finalized yet. Development builds can use the same CMake build flow as macOS, with Qt supplied by the system package manager or a Qt SDK.
