# Linux Packaging

Therion Studio currently provides two Linux artifact families:

- `.deb` packages for Ubuntu 26.04 on `amd64` and `arm64`
- AppImages as the portable Linux channel on `x86_64` and `aarch64`

They are built by the manual GitHub Actions workflow:

- `.github/workflows/linux-packages.yml`

## Workflow Inputs

- `source_ref`: branch/tag/commit to package
- `build_type`: `Release` or `RelWithDebInfo`

## Produced Artifacts

- `therion-studio-<package_label>-ubuntu-26.04-amd64.deb`
- `therion-studio-<package_label>-ubuntu-26.04-arm64.deb`
- `TherionStudio-<package_label>-Linux-x86_64.AppImage`
- `TherionStudio-<package_label>-Linux-aarch64.AppImage`
- `TherionStudio-Linux-x86_64-artifacts-manifest.json`
- `TherionStudio-Linux-aarch64-artifacts-manifest.json`

The manifests include file names, sizes, architecture labels, and SHA256 checksums.

The `.deb` artifact intentionally uses an `ubuntu-26.04` suffix. The Ubuntu-built `.deb` is not the
Debian compatibility path and is not a general Ubuntu-family package because Qt package dependency
metadata can differ between distribution releases.
Release-tagged builds use the CalVer tag as the artifact label and Debian package version.
Snapshot builds use `dev-<short_sha>` for artifact names and
`<calver>+git<yyyymmdd>.g<short_sha>` for the Debian package `Version` field.

After artifact creation, the workflow runs Linux artifact smoke checks:

- launches an `ubuntu:26.04` container
- launches a `debian:13` container
- installs the generated `amd64` and `arm64` `.deb` packages on Ubuntu 26.04
- verifies expected installed paths for the Ubuntu `.deb`
- performs offscreen `.deb` launch sanity checks on Ubuntu 26.04
- performs offscreen `x86_64` and `aarch64` AppImage launch sanity checks on Ubuntu 26.04 and Debian 13

Ubuntu 26.04 is considered a tested `.deb` target for a given artifact only when the corresponding
workflow smoke job passes. Other Linux distributions and Ubuntu releases are covered by the
AppImage channel unless a matching distro-specific package is introduced. The AppImage is
considered tested on Debian 13 and Ubuntu 26.04 only when both AppImage smoke checks pass.

## Build Notes

- `.deb` generation uses CPack DEB configuration from `CMakeLists.txt` inside an `ubuntu:26.04`
  container.
- AppImage generation uses a separate CMake build inside a `debian:13` container with
  `THERION_ENABLE_QT_LINUX_DEPLOY_INSTALL=ON` so Qt's generated Linux deployment script
  populates the AppDir during install.
- The AppImage workflow additionally runs `scripts/linux-packages/prepare_appimage_appdir.sh` to stage
  selected Qt plugin groups, stage `ldd`-resolved `libQt6*.so*` runtime families into
  `AppDir/usr/lib`, write an `AppRun` wrapper that sets `LD_LIBRARY_PATH` and `QT_PLUGIN_PATH`,
  verify the offscreen platform plugin, and launch-test the AppDir before packaging.
- The final AppImage is produced with pinned architecture-specific `appimagetool` 1.9.1 and pinned
  architecture-specific `type2-runtime` 20251108 inputs, all SHA256-verified by the workflow before
  use.
- The manifests record architecture labels, AppImage Qt package source/version/package sets,
  `appimagetool`, and AppImage runtime inputs for release provenance.
- The workflow runs install-layout smoke verification using
  `scripts/verify_install_layout.py` before packaging.
- Artifact naming and manifest generation are validated by
  `scripts/linux-packages/verify_release_artifacts.py`.
- The Ubuntu 26.04 `.deb` build logic lives in `scripts/linux-packages/build_deb_package.sh` so the same
  container build can be reproduced locally with Docker through
  `scripts/linux-packages/build_deb_package_docker.sh`.
- The Debian 13 AppImage build logic lives in `scripts/linux-packages/build_appimage_package.sh` so the same
  container build can be reproduced locally with Docker through
  `scripts/linux-packages/build_appimage_package_docker.sh`.
- Local smoke testing uses the same install/launch checks as the workflow through
  `scripts/linux-packages/smoke_deb_package_docker.sh` for the Ubuntu `.deb` and
  `scripts/linux-packages/smoke_appimage_package_docker.sh` for AppImage launch checks on Debian 13 or
  Ubuntu 26.04.
- Smoke scripts accept either direct local build directories or an `actions/download-artifact`
  extraction root containing `build-linux-packages/` and `build-linux-appimage/` subdirectories.
- The full local Linux build and smoke pass can be run with
  `scripts/linux-packages/build_and_smoke_packages_docker.sh`. Set `THERION_STUDIO_LINUX_ARCH=arm64` to run
  the same local path for ARM64.
- Do not build production Linux artifacts from mutable
  `linuxdeployqt` `continuous` downloads or unmaintained Qt deployment plugins.
