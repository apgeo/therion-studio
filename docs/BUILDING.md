# Building Therion Studio

This document describes the current build and packaging workflow.

## Common Requirements

- CMake 3.21 or newer
- C++17 compiler
- Qt 6.5 or newer with `Widgets` and `Svg`

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

Recommended MSVC release build:

```powershell
cmake -S . -B build-win -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:\Qt\6.11.0\msvc2022_64

cmake --build build-win --target TherionStudio
cmake --install build-win --prefix dist\windows
cpack --config build-win\CPackConfig.cmake
```

The CMake install step runs Qt deployment on Windows, copying the required Qt runtime next to the installed executable. CPack is configured to use NSIS for the Windows installer.

Required Windows packaging tools:

- Qt for MSVC
- Ninja or another CMake generator
- NSIS for installer generation

## Linux

Linux release packaging is not finalized yet. Development builds can use the same CMake build flow as macOS, with Qt supplied by the system package manager or a Qt SDK.
