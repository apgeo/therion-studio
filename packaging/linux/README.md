# Linux Packaging

Therion Studio currently provides two Linux artifacts:

- `.deb` package for Ubuntu 26.04
- AppImage as the portable Linux channel

They are built by the manual GitHub Actions workflow:

- `.github/workflows/linux-packages.yml`

## Workflow Inputs

- `source_ref`: branch/tag/commit to package
- `build_type`: `Release` or `RelWithDebInfo`

## Produced Artifacts

- `therion-studio-<package_label>-ubuntu-26.04-x86_64.deb`
- `TherionStudio-<package_label>-Linux-x86_64.AppImage`
- `TherionStudio-Linux-artifacts-manifest.json`

The manifest includes file names, sizes, and SHA256 checksums.

The `.deb` artifact intentionally uses an `ubuntu-26.04` suffix. The Ubuntu-built `.deb` is not the
Debian compatibility path and is not a general Ubuntu-family package because Qt package dependency
metadata can differ between distribution releases.
Release-tagged builds use the CalVer tag as the artifact label and Debian package version.
Snapshot builds use `dev-<short_sha>` for artifact names and
`<calver>+git<yyyymmdd>.g<short_sha>` for the Debian package `Version` field.

After artifact creation, the workflow runs Linux artifact smoke checks:

- launches an `ubuntu:26.04` container
- launches a `debian:13` container
- installs the generated `.deb` on Ubuntu 26.04
- verifies expected installed paths for the Ubuntu `.deb`
- performs an offscreen `.deb` launch sanity check on Ubuntu 26.04
- performs offscreen AppImage launch sanity checks on Ubuntu 26.04 and Debian 13

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
- The AppImage workflow additionally runs `scripts/prepare_linux_appimage_appdir.sh` to stage
  `ldd`-resolved `libQt6*.so*` runtime families into `AppDir/usr/lib`, write an `AppRun` wrapper
  that sets `LD_LIBRARY_PATH` and `QT_PLUGIN_PATH`, verify the offscreen platform plugin, and
  launch-test the AppDir before packaging.
- The final AppImage is produced with pinned `appimagetool` 1.9.1 and pinned
  `type2-runtime` 20251108 `runtime-x86_64`, both SHA256-verified by the workflow before use.
- The manifest records the AppImage Qt package source/version/package set, `appimagetool`, and AppImage
  runtime inputs for release provenance.
- The workflow runs install-layout smoke verification using
  `scripts/verify_install_layout.py` before packaging.
- Artifact naming and manifest generation are validated by
  `scripts/verify_linux_release_artifacts.py`.
- Do not build production Linux artifacts from mutable
  `linuxdeployqt` `continuous` downloads or unmaintained Qt deployment plugins.
