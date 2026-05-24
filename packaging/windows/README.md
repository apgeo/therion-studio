# Windows Installer Packaging

Therion Studio uses CMake install rules plus CPack/NSIS for Windows installer generation.
The installer bundles Therion Studio and the Qt runtime deployed by Qt's CMake deployment API.
It does not bundle the external `therion.exe` compiler; users install/configure Therion separately.

## Local Release Build

Prerequisites:

- Windows 2022/11 development machine or GitHub Actions `windows-2022` runner
- Visual Studio 2022 Build Tools with the MSVC x64 toolchain
- CMake 3.21 or newer
- Ninja
- Qt 6.5 or newer for `msvc2022_64`, including `qtsvg` and `qttools`
- NSIS

Example PowerShell build:

```powershell
$QtRoot = "C:/Qt/6.7.3/msvc2022_64"
$Version = "2026.5.0"

cmake -S . -B build-win -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=$QtRoot `
  -DTHERION_STUDIO_VERSION=$Version

cmake --build build-win --target TherionStudio --parallel
cpack --config build-win/CPackConfig.cmake -C Release --verbose
```

Expected artifact:

```text
build-win/TherionStudio-<version>-Windows-x86_64.exe
```

## What CPack Does

The Windows CMake configuration:

- installs `TherionStudio.exe` at the installer root
- runs Qt deployment during install packaging so required Qt DLLs and plugins are included
- uses NSIS to create the installer
- includes the project `GPL-3.0-or-later` license from the root `LICENSE` file in CPack metadata
- assigns the bundled `resources/app/TherionStudio.ico` to the installer and installed app shortcut
- creates a Start Menu entry and desktop link for `Therion Studio`

## GitHub Actions Packaging

The repository includes `.github/workflows/windows-installer.yml` as a manual packaging workflow.
It intentionally uses `workflow_dispatch` only so installer production is explicit. The workflow
has a `source_ref` input, so the source can be built from `main`, a release tag such as `v2026.5.0`,
or a specific commit SHA.

If the checked-out commit is exactly tagged with a CalVer release tag such as `v2026.5.0`, the
workflow passes `-DTHERION_STUDIO_VERSION=2026.5.0` and
`-DTHERION_STUDIO_PACKAGE_LABEL=2026.5.0` to CMake. Branch and commit builds derive a development
version from the current UTC year/month, such as `2026.5.0`, and append the short commit SHA to
the package label, such as `2026.5.0-a1b2c3d`.

Recommended release flow:

1. Tag the source, for example `v2026.5.0`.
2. Run the `Windows Installer` workflow manually from GitHub Actions.
3. Set `source_ref` to `main` for a current development installer or to the release tag for a tagged release build.
4. Download the uploaded installer artifact.
5. Attach the installer to the GitHub Release for the tag.

The workflow currently installs Qt via `jurplel/install-qt-action`, installs NSIS/Ninja with Chocolatey,
configures a Release Ninja build, runs CPack, and uploads the generated `.exe` installer artifact.

## Future Hardening

Before public distribution, add:

- Windows Authenticode signing for `TherionStudio.exe` and the NSIS installer
- release-only version stamping from tags
- installer smoke test on a clean Windows VM
- optional checksum generation for published artifacts
